// ============================================================
//  SmartWand2 —— 魔法小屋手势控制系统 (毕设答辩注释版)
//  核心架构：数据采集 -> 物理唤醒 -> AI推理 -> 二次校验 -> 状态机 -> 红外发射
// ============================================================

#include <Arduino.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <SmartWand2_inferencing.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

// ==========================================
// [模块 1] 硬件引脚与通信协议定义
// ==========================================
#define LED_PIN   2
#define SDA_PIN   4
#define SCL_PIN   5
#define IR_PIN    6

// AI类别索引 (严格匹配 Edge Impulse 模型输出数组的顺序)
#define IDX_Z           0
#define IDX_HUAQUAN     1
#define IDX_QIANCI      2
#define IDX_SHANGTIAO   3
#define IDX_ZAOSHENG    4

// 最终意图的 NEC 红外编码 (32位，需与下游执行端匹配)
#define CODE_LIGHT_ON      0xAA000001UL
#define CODE_LIGHT_OFF     0xAA000002UL
#define CODE_CURTAIN_OPEN  0xAA000003UL
#define CODE_CURTAIN_CLOSE 0xAA000004UL
#define CODE_DOOR_OPEN     0xAA000006UL
#define CODE_DOOR_CLOSE    0xAA000005UL

#define NEC_BITS  32

// ==========================================
// [模块 2] 核心算法与鲁棒性调参区
// ==========================================
#define TRIGGER_THRESHOLD            0.80f  // AI及格线：置信度 > 0.8 才采信
#define NOISE_SUPPRESS               0.75f  // 降噪门槛：噪声概率 < 0.75 才继续处理
#define ACCEL_THRESHOLD              13.0f  // 物理唤醒线：总加速度 > 13 启动AI推理，省电防误触

// 物理二次校验门槛：防止AI产生“捷径学习”导致误判
#define QIANCI_AXIS_THRESHOLD        5.0f   // 前刺动作 Y轴最低爆发力
#define QIANCI_DISTANCE_THRESHOLD    4.0f  // 前刺动作 最低估算位移
#define SHANGTIAO_FORCE_THRESHOLD    8.0f   // 上挑动作 最低总爆发力
#define SHANGTIAO_DISTANCE_THRESHOLD 4.0f  // 上挑动作 最低估算位移

// 红外通信与状态机参数
#define IR_REPEAT_COUNT        6     // 冗余设计：单次动作连发6次防丢包
#define IR_REPEAT_INTERVAL_MS  400   // 连发间隔
#define PREFIX_EXPIRE_MS       5000  // 状态机：前缀动作 5秒超时作废

// ==========================================
// [模块 3] 系统全局状态与缓存变量
// ==========================================
#define INFERENCE_EVERY_N  5         // 降频推理：每集齐 5 帧数据推理一次
#define COOLDOWN_MS        1000      // 技能冷却：触发后 2 秒内不响应新动作
#define WARMUP_DISCARD     50        // 预热过滤：丢弃开机前 50 帧不稳定数据

// 有限状态机 (FSM) 状态定义
#define PREFIX_NONE      0
#define PREFIX_SHANGTIAO 1
#define PREFIX_QIANCI    2

int      prefix_buffer   = PREFIX_NONE; // 状态机记忆体：记录当前的前缀动作
uint32_t prefix_start_ms = 0;           // 记录前缀发生的时间戳，用于计算超时

Adafruit_MPU6050 mpu;
IRsend           irsend(IR_PIN);
float buffer[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = { 0 }; // 滑动窗口缓存：存放送入AI的实时特征序列

int      frame_counter   = 0;
uint32_t last_trigger_ms = 0;           // 上次触发的时间戳，用于冷却计算

// ==========================================
// [模块 4] 底层数据采集函数
// ==========================================
// 作用：读取 MPU6050 六轴原始数据，并按序填入特征缓存数组
void write_frame(float* buf, int idx) {
    sensors_event_t a, g, t;
    mpu.getEvent(&a, &g, &t);
    buf[idx + 0] = a.acceleration.x;
    buf[idx + 1] = a.acceleration.y;
    buf[idx + 2] = a.acceleration.z;
    buf[idx + 3] = g.gyro.x;
    buf[idx + 4] = g.gyro.y;
    buf[idx + 5] = g.gyro.z;
}

// ==========================================
// [模块 5] 动作执行与通信函数
// ==========================================
// 作用：执行最终决策，点亮指示灯并发送红外信号，最后重置环境状态
void send_ir(uint32_t code, const char* name) {
    digitalWrite(LED_PIN, HIGH); 
    Serial.printf("\n📡 发射信号：%s (0x%08X)\n", name, code);

    // 循环连发保障送达率
    for (int i = 1; i <= IR_REPEAT_COUNT; i++) {
        irsend.sendNEC(code, NEC_BITS);
        Serial.printf("  -> 第 %d 次发射...\n", i);
        if (i < IR_REPEAT_COUNT) {
            delay(IR_REPEAT_INTERVAL_MS);
        }
    }

    digitalWrite(LED_PIN, LOW); 
    Serial.println("✅ 发射完成，进入冷却模式");

    memset(buffer, 0, sizeof(buffer)); // 清空滑动窗口，防止动作余震导致二次触发
    last_trigger_ms = millis();
}

// ==========================================
// [模块 6] 状态机控制逻辑
// ==========================================
// 作用 1：非阻塞检查前缀动作是否超时
void check_prefix_expire() {
    if (prefix_buffer != PREFIX_NONE &&
        millis() - prefix_start_ms > PREFIX_EXPIRE_MS) {
        Serial.println("⏰ 前缀动作缓冲已过期，重置为基础状态");
        prefix_buffer = PREFIX_NONE;
    }
}

// 作用 2：处理通过了所有校验的有效动作，结合前缀记忆下发最终指令
void handle_gesture(int idx) {
    Serial.println("-----------------------------");

    // 记录前缀动作，重置 5 秒倒计时
    if (idx == IDX_SHANGTIAO) {
        prefix_buffer   = PREFIX_SHANGTIAO;
        prefix_start_ms = millis();
        Serial.println("🌟 识别到：上挑 (已记录前缀)");
        last_trigger_ms = millis(); 
        return;
    }

    if (idx == IDX_QIANCI) {
        prefix_buffer   = PREFIX_QIANCI;
        prefix_start_ms = millis();
        Serial.println("🗡️ 识别到：前刺 (已记录前缀)");
        last_trigger_ms = millis();
        return;
    }

    // 触发动作：检查是否有前缀，组合释放魔法
    if (idx == IDX_HUAQUAN) {
        if (prefix_buffer == PREFIX_SHANGTIAO) {
            send_ir(CODE_CURTAIN_OPEN, "开窗帘");
            prefix_buffer = PREFIX_NONE;
        } else if (prefix_buffer == PREFIX_QIANCI) {
            send_ir(CODE_DOOR_OPEN, "开门");
            prefix_buffer = PREFIX_NONE;
        } else {
            send_ir(CODE_LIGHT_ON, "开灯"); // 无前缀的默认动作
        }
        return;
    }

    if (idx == IDX_Z) {
        if (prefix_buffer == PREFIX_SHANGTIAO) {
            send_ir(CODE_CURTAIN_CLOSE, "关窗帘");
            prefix_buffer = PREFIX_NONE;
        } else if (prefix_buffer == PREFIX_QIANCI) {
            send_ir(CODE_DOOR_CLOSE, "关门");
            prefix_buffer = PREFIX_NONE;
        } else {
            send_ir(CODE_LIGHT_OFF, "关灯"); // 无前缀的默认动作
        }
        return;
    }
}

// ==========================================
// [模块 7] 系统初始化
// ==========================================
void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    irsend.begin();

    Wire.begin(SDA_PIN, SCL_PIN);
    if (!mpu.begin(0x68)) {
        Serial.println("❌ MPU6050 未连接，请检查线路！");
        while (true) { delay(100); }
    }
    
    // 初始化传感器硬件滤波与量程，必须严格匹配模型训练环境
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

    Serial.println("⏳ 传感器预热采样中...");
    for (int i = 0; i < WARMUP_DISCARD; i++) {
        sensors_event_t a, g, t;
        mpu.getEvent(&a, &g, &t);
        delay(EI_CLASSIFIER_INTERVAL_MS);
    }
    
    // 预填满滑动窗口
    for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i += 6) {
        write_frame(buffer, i);
        delay(EI_CLASSIFIER_INTERVAL_MS);
    }
    Serial.println("✅ 魔法小屋系统就绪，开始监控动作...");
}

// ==========================================
// [模块 8] 系统主循环 (流水线作业)
// ==========================================
void loop() {
    // 节拍器：保证严格按固定间隔(10ms)采样，防止时序特征变形
    uint64_t next_tick = micros() + (EI_CLASSIFIER_INTERVAL_MS * 1000);

    check_prefix_expire(); // 检查前缀超时

    // 环节 A：维护滑动窗口 (Rolling Buffer)，旧数据前移，新数据放队尾
    memmove(buffer, buffer + 6,
            (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 6) * sizeof(float));
    write_frame(buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - 6);
    frame_counter++;

    // 环节 B：积攒够 N 帧数据，进入分析推断流程
    if (frame_counter >= INFERENCE_EVERY_N) {
        frame_counter = 0;

        float max_accel = 0.0f;
        float max_abs_y = 0.0f;
        float estimated_velocity = 0.0f;
        float estimated_distance = 0.0f;
        float dt = (float)EI_CLASSIFIER_INTERVAL_MS / 1000.0f;

        // 环节 C：提取窗口内物理特征，做简易二次积分估算位移
        for (int i = 0; i < EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; i += 6) {
            float ax = buffer[i + 0];
            float ay = buffer[i + 1];
            float az = buffer[i + 2];
            float acc = sqrt(ax*ax + ay*ay + az*az);
            
            if (acc > max_accel) max_accel = acc;
            if (fabs(ay) > max_abs_y) max_abs_y = fabs(ay);
            
            float a_dyn = acc - 9.81f; // 剔除重力
            if (fabs(a_dyn) < 2.0f) a_dyn = 0.0f;
            estimated_velocity += a_dyn * dt;
            estimated_distance += fabs(estimated_velocity) * dt;
        }

        // 环节 D：物理唤醒线检查 -> 调用轻量级 AI 模型推理
        if (max_accel >= ACCEL_THRESHOLD) {
            signal_t signal;
            numpy::signal_from_buffer(
                buffer, EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE, &signal);
            ei_impulse_result_t result = { 0 };
            EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

            if (res == EI_IMPULSE_OK) {
                // 提取模型预测的各类置信度
                float s_z         = result.classification[0].value;
                float s_huaquan   = result.classification[1].value;
                float s_qianci    = result.classification[2].value;
                float s_shangtiao = result.classification[3].value;
                float s_zaosheng  = result.classification[4].value;

                // 串口防刷屏限流输出
                static uint32_t last_debug_ms = 0;
                bool show_debug = (millis() - last_debug_ms >= 1000);
                
                if (show_debug) {
                    last_debug_ms = millis();
                    Serial.println("\n[🔍 调试输出]");
                    Serial.printf("🤖 AI 置信度 -> Z字:%.2f | 画圈:%.2f | 前刺:%.2f | 上挑:%.2f | 噪声:%.2f\n", 
                                  s_z, s_huaquan, s_qianci, s_shangtiao, s_zaosheng);
                    Serial.printf("📐 物理校验 -> 加速度:%.2f | Y轴力:%.2f | 位移:%.2f\n", 
                                  max_accel, max_abs_y, estimated_distance);
                }

                // 环节 E：技能冷却与噪声筛查
                bool cooldown_ok = (millis() - last_trigger_ms > COOLDOWN_MS);
                bool not_noise   = (s_zaosheng < NOISE_SUPPRESS);

                if (cooldown_ok && not_noise) {
                    float scores[4] = {s_z, s_huaquan, s_qianci, s_shangtiao};
                    int   best_idx   = -1;
                    float best_score = TRIGGER_THRESHOLD; // 必须大于设定的及格线

                    for (int i = 0; i < 4; i++) {
                        if (scores[i] > best_score) {
                            best_score = scores[i];
                            best_idx   = i;
                        }
                    }

                    // 环节 F：物理硬约束二次校验 (解决AI捷径学习导致的前刺/上挑误判)
                    if (best_idx == IDX_QIANCI) {
                        if (max_abs_y < QIANCI_AXIS_THRESHOLD || estimated_distance < QIANCI_DISTANCE_THRESHOLD)
                            best_idx = -1; // 物理指标不达标，一票否决
                    } else if (best_idx == IDX_SHANGTIAO) {
                        if (max_accel < SHANGTIAO_FORCE_THRESHOLD || estimated_distance < SHANGTIAO_DISTANCE_THRESHOLD)
                            best_idx = -1;
                    }

                    // 环节 G：所有校验通过，移交状态机执行动作
                    if (best_idx >= 0) {
                        handle_gesture(best_idx);
                    }
                }
            }
        }
    }

    // 阻塞等待剩余时间，补齐 10ms 采样周期
    while (micros() < next_tick) { }
}
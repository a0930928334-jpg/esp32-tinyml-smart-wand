// ============================================================
//  Nano —— 魔法小屋控制中心（双 360度舵机 终极版）
//  ──────────────────────────────
//  接线确认：
//    红外接收头 OUT    → D2
//    室内LED 正极      → D13
//    大门舵机(360度)   → D9
//    窗帘舵机(360度)   → D10
//    两个舵机VCC       → 独立5V电源正极
//    两个舵机GND       → 独立5V电源负极 + Nano的GND (共地)
// ============================================================

#include <Arduino.h>
#include <IRremote.h>
#include <Servo.h>

// ---------- 引脚定义 ----------
#define RECV_PIN          2
#define LED_PIN           13
#define SERVO_DOOR_PIN    9
#define SERVO_CURTAIN_PIN 10

// ---------- 真实的红外编码（针对你收到的倒序码） ----------
#define CODE_LIGHT_ON      0x80000055UL  // 画圈
#define CODE_LIGHT_OFF     0x40000055UL  // Z字
#define CODE_CURTAIN_OPEN  0x20000055UL  // 上挑→画圈
#define CODE_CURTAIN_CLOSE 0xC0000055UL  // 上挑→Z字
#define CODE_DOOR_OPEN     0xA0000055UL  // 前刺→画圈
#define CODE_DOOR_CLOSE    0x60000055UL  // 前刺→Z字

// ============================================================
// 🎯 调参区
// ============================================================
// 大门舵机（360度）
#define DOOR_FORWARD       180    // 大门正转速度
#define DOOR_BACKWARD      0      // 大门反转速度
#define DOOR_TIME_MS       100   // 大门动作时长（测试后修改）

// 窗帘舵机（360度）
#define CURTAIN_FORWARD    180    // 窗帘正转速度
#define CURTAIN_BACKWARD   0      // 窗帘反转速度
#define CURTAIN_TIME_MS    1000   // 窗帘动作时长（测试后修改）

// 去重防抖
#define DEBOUNCE_MS        2000
// ============================================================

// ---------- 状态变量 ----------
Servo    servoDoor;
Servo    servoCurtain;

bool     curtain_busy    = false;
uint32_t curtain_stop_ms = 0;

bool     door_busy       = false;
uint32_t door_stop_ms    = 0;

uint32_t last_code_val   = 0;
uint32_t last_code_ms    = 0;

// ============================================================
//  非阻塞自动停止检查（每帧调用）
// ============================================================
void update_curtain() {
    if (curtain_busy && millis() >= curtain_stop_ms) {
        servoCurtain.detach();   // 彻底切断信号防止蠕动
        curtain_busy = false;
        Serial.println("🛑 窗帘动作完成，已停机");
    }
}

void update_door() {
    if (door_busy && millis() >= door_stop_ms) {
        servoDoor.detach();      // 彻底切断信号防止蠕动
        door_busy = false;
        Serial.println("🛑 大门动作完成，已停机");
    }
}

// ============================================================
//  动作触发逻辑
// ============================================================
void start_curtain(int direction, const char* name) {
    if (curtain_busy) {
        Serial.println("⚠️ 窗帘正在运行中，忽略重复指令");
        return;
    }
    if (!servoCurtain.attached()) servoCurtain.attach(SERVO_CURTAIN_PIN);
    
    servoCurtain.write(direction);
    curtain_busy    = true;
    curtain_stop_ms = millis() + CURTAIN_TIME_MS;

    Serial.print("🪟 ");
    Serial.print(name);
    Serial.print("，预计转动 ");
    Serial.print(CURTAIN_TIME_MS);
    Serial.println(" ms...");
}

void start_door(int direction, const char* name) {
    if (door_busy) {
        Serial.println("⚠️ 大门正在运行中，忽略重复指令");
        return;
    }
    if (!servoDoor.attached()) servoDoor.attach(SERVO_DOOR_PIN);
    
    servoDoor.write(direction);
    door_busy    = true;
    door_stop_ms = millis() + DOOR_TIME_MS;

    Serial.print("🚪 ");
    Serial.print(name);
    Serial.print("，预计转动 ");
    Serial.print(DOOR_TIME_MS);
    Serial.println(" ms...");
}

// ============================================================
//  指令执行逻辑
// ============================================================
void execute(uint32_t code) {
    switch (code) {
        case CODE_LIGHT_ON:
            digitalWrite(LED_PIN, HIGH);
            Serial.println("✨ 指令识别：画圈 → 💡 灯亮");
            break;

        case CODE_LIGHT_OFF:
            digitalWrite(LED_PIN, LOW);
            Serial.println("⚡ 指令识别：Z字 → 🌑 灯灭");
            break;

        case CODE_CURTAIN_OPEN:
            Serial.println("🌟 指令识别：上挑+画圈 → 开窗帘");
            start_curtain(CURTAIN_FORWARD, "打开窗帘");
            break;

        case CODE_CURTAIN_CLOSE:
            Serial.println("🌟 指令识别：上挑+Z字 → 关窗帘");
            start_curtain(CURTAIN_BACKWARD, "关闭窗帘");
            break;

        case CODE_DOOR_OPEN:
            Serial.println("🗡️ 指令识别：前刺+画圈 → 开门");
            start_door(DOOR_FORWARD, "打开大门");
            break;

        case CODE_DOOR_CLOSE:
            Serial.println("🗡️ 指令识别：前刺+Z字 → 关门");
            start_door(DOOR_BACKWARD, "关闭大门");
            break;

        default:
            // 直接忽略未知编码，保持静默
            break;
    }
}

// ============================================================
void setup() {
    Serial.begin(9600);

    // 引脚初始化
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // 360度舵机不需要在上电时赋予初始角度，保持未连接状态即可
    // 动作时会自动 attach

    // 红外接收初始化
    IrReceiver.begin(RECV_PIN, DISABLE_LED_FEEDBACK);

    Serial.println("===========================================");
    Serial.println("✅ 魔法小屋控制系统 (双360舵机版) 已启动");
    Serial.println("等待魔杖施法中...");
    Serial.println("===========================================");
}

// ============================================================
void loop() {
    // 实时检查两个舵机是否到点该停了
    update_curtain();
    update_door();

    if (IrReceiver.decode()) {
        uint32_t code = IrReceiver.decodedIRData.decodedRawData;

        if (code != 0x00000000) {
            // 防重复触发逻辑
            bool is_dup = (code == last_code_val && (millis() - last_code_ms < DEBOUNCE_MS));

            if (!is_dup) {
                last_code_val = code;
                last_code_ms  = millis();
                
                // 为了让控制台更干净，这里也可以选择只在已知编码时打印捕获信息
                // 但保留这里可以让你知道确实收到了信号，只是遇到未知时不报错
                // 如果你连捕获信息都不想看，可以把这两行也删掉：
                // Serial.print("\n📡 信号捕获: 0x");
                // Serial.println(code, HEX);

                execute(code);
            }
        }
        IrReceiver.resume();
    }
}
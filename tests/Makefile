# Host unit tests: no Arduino toolchain needed.
#   make -C tests          build and run
#   make -C tests clean
# C++11 matches the oldest toolchain the headers must support (avr-gcc).

CXX      ?= g++
CXXFLAGS ?= -std=c++11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Werror
INCLUDES := -I../firmware/common -I../firmware/wand-esp32s3/src
HEADERS  := ../firmware/common/ir_codes.h ../firmware/common/timing.h \
            ../firmware/wand-esp32s3/src/gesture_logic.h

.PHONY: test clean

test: host_tests
	./host_tests

host_tests: host_tests.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INCLUDES) host_tests.cpp -o $@ -lm

clean:
	rm -f host_tests

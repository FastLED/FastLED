
#ifdef FASTLED_STUB_IMPL  // Only use this if explicitly defined.

#include "led_sysdefs_stub.h"

#include <chrono>
#include <thread>

void pinMode(uint8_t pin, uint8_t mode) {
    // Empty stub as we don't actually ever write anything
}

uint32_t millis() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

uint32_t micros() {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void delay(int ms) {
    std::this_thread::sleep_for (std::chrono::milliseconds(ms));
}

#endif  // FASTLED_STUB_IMPL
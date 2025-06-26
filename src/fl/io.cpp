#include "io.h"

#include <stddef.h>
#include "fl/stdint.h"

// Platform-specific print implementations
#ifdef __EMSCRIPTEN__
#include "platforms/wasm/print_wasm.h"
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
#include "platforms/print_native.h"
#elif defined(ESP32) || defined(ESP8266)
#include "platforms/esp/print_esp.h"
#elif defined(STM32F1) || defined(STM32F4) || defined(STM32H7) || defined(ARDUINO_GIGA)
#include "platforms/arm/print_stm32.h"
#elif defined(__IMXRT1062__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
#include "platforms/arm/print_teensy.h"
#elif defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(ARDUINO_NRF52_DK)
#include "platforms/arm/print_nrf52.h"
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
#include "platforms/avr/print_avr.h"
#else
#include "platforms/print_arduino.h"
#endif

namespace fl {

void print(const char* str) {
    if (!str) return;

#ifdef __EMSCRIPTEN__
    print_wasm(str);
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    print_native(str);
#elif defined(ESP32) || defined(ESP8266)
    print_esp(str);
#elif defined(STM32F1) || defined(STM32F4) || defined(STM32H7) || defined(ARDUINO_GIGA)
    print_stm32(str);
#elif defined(__IMXRT1062__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
    print_teensy(str);
#elif defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(ARDUINO_NRF52_DK)
    print_nrf52(str);
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    print_avr(str);
#else
    print_arduino(str);
#endif
}

void println(const char* str) {
    if (!str) return;

#ifdef __EMSCRIPTEN__
    println_wasm(str);
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    println_native(str);
#elif defined(ESP32) || defined(ESP8266)
    println_esp(str);
#elif defined(STM32F1) || defined(STM32F4) || defined(STM32H7) || defined(ARDUINO_GIGA)
    println_stm32(str);
#elif defined(__IMXRT1062__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
    println_teensy(str);
#elif defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(ARDUINO_NRF52_DK)
    println_nrf52(str);
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    println_avr(str);
#else
    println_arduino(str);
#endif
}

} // namespace fl

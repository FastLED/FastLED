#include "io.h"

#include <stddef.h>
#include "fl/stdint.h"

// Platform-specific I/O implementations
#ifdef __EMSCRIPTEN__
#include "platforms/wasm/io_wasm.h"
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
#include "platforms/io_native.h"
#elif defined(ESP32) || defined(ESP8266)
#include "platforms/esp/io_esp.h"
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
#include "platforms/avr/io_avr.h"
#else
#include "platforms/io_arduino.h"
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
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    print_avr(str);
#else
    // Use generic Arduino print for all other platforms including:
    // - STM32 (STM32F1, STM32F4, STM32H7, ARDUINO_GIGA)
    // - Teensy (__IMXRT1062__, __MK20DX128__, __MK20DX256__, __MK64FX512__, __MK66FX1M0__)
    // - NRF (NRF52, NRF52832, NRF52840, ARDUINO_NRF52_DK)
    // - All other Arduino-compatible platforms
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
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    println_avr(str);
#else
    // Use generic Arduino print for all other platforms including:
    // - STM32 (STM32F1, STM32F4, STM32H7, ARDUINO_GIGA)
    // - Teensy (__IMXRT1062__, __MK20DX128__, __MK20DX256__, __MK64FX512__, __MK66FX1M0__)
    // - NRF (NRF52, NRF52832, NRF52840, ARDUINO_NRF52_DK)  
    // - All other Arduino-compatible platforms
    println_arduino(str);
#endif
}

int available() {
#ifdef __EMSCRIPTEN__
    return available_wasm();
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    return available_native();
#elif defined(ESP32) || defined(ESP8266)
    return available_esp();
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    return available_avr();
#else
    // Use generic Arduino input for all other platforms including:
    // - STM32 (STM32F1, STM32F4, STM32H7, ARDUINO_GIGA)
    // - Teensy (__IMXRT1062__, __MK20DX128__, __MK20DX256__, __MK64FX512__, __MK66FX1M0__)
    // - NRF (NRF52, NRF52832, NRF52840, ARDUINO_NRF52_DK)
    // - All other Arduino-compatible platforms
    return available_arduino();
#endif
}

int read() {
#ifdef __EMSCRIPTEN__
    return read_wasm();
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    return read_native();
#elif defined(ESP32) || defined(ESP8266)
    return read_esp();
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    return read_avr();
#else
    // Use generic Arduino input for all other platforms including:
    // - STM32 (STM32F1, STM32F4, STM32H7, ARDUINO_GIGA)
    // - Teensy (__IMXRT1062__, __MK20DX128__, __MK20DX256__, __MK64FX512__, __MK66FX1M0__)
    // - NRF (NRF52, NRF52832, NRF52840, ARDUINO_NRF52_DK)
    // - All other Arduino-compatible platforms
    return read_arduino();
#endif
}

} // namespace fl

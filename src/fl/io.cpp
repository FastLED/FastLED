#include "io.h"

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
#elif defined(__MKL26Z64__)
// Teensy LC has special handling to avoid _write linker issues
#include "platforms/io_teensy_lc.h"
#elif defined(__IMXRT1062__) || defined(__MK66FX1M0__) || defined(__MK64FX512__) || defined(__MK20DX256__) || defined(__MK20DX128__)
// All other Teensy platforms use lightweight implementation to avoid Serial library bloat
#include "platforms/io_teensy.h"
#else
#include "platforms/io_arduino.h"
#endif

namespace fl {

#ifdef FASTLED_TESTING
// Static storage for injected handlers using lazy initialization to avoid global constructors
static print_handler_t& get_print_handler() {
    static print_handler_t handler;
    return handler;
}

static println_handler_t& get_println_handler() {
    static println_handler_t handler;
    return handler;
}

static available_handler_t& get_available_handler() {
    static available_handler_t handler;
    return handler;
}

static read_handler_t& get_read_handler() {
    static read_handler_t handler;
    return handler;
}
#endif

void print(const char* str) {
    if (!str) return;

#ifdef FASTLED_TESTING
    // Check for injected handler first
    if (get_print_handler()) {
        get_print_handler()(str);
        return;
    }
#endif

#ifdef __EMSCRIPTEN__
    print_wasm(str);
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    print_native(str);
#elif defined(ESP32) || defined(ESP8266)
    print_esp(str);
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    print_avr(str);
#elif defined(__MKL26Z64__)
    // Teensy LC uses special no-op functions to avoid _write linker issues
    print_teensy_lc(str);
#elif defined(__IMXRT1062__) || defined(__MK66FX1M0__) || defined(__MK64FX512__) || defined(__MK20DX256__) || defined(__MK20DX128__)
    // All other Teensy platforms use lightweight implementation 
    print_teensy(str);
#else
    // Use generic Arduino print for all other platforms including:
    // - STM32 (STM32F1, STM32F4, STM32H7, ARDUINO_GIGA)
    // - NRF (NRF52, NRF52832, NRF52840, ARDUINO_NRF52_DK)
    // - All other Arduino-compatible platforms
    print_arduino(str);
#endif
}

void println(const char* str) {
    if (!str) return;

#ifdef FASTLED_TESTING
    // Check for injected handler first
    if (get_println_handler()) {
        get_println_handler()(str);
        return;
    }
#endif

#ifdef __EMSCRIPTEN__
    println_wasm(str);
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    println_native(str);
#elif defined(ESP32) || defined(ESP8266)
    println_esp(str);
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    println_avr(str);
#elif defined(__MKL26Z64__)
    // Teensy LC uses special no-op functions to avoid _write linker issues
    println_teensy_lc(str);
#elif defined(__IMXRT1062__) || defined(__MK66FX1M0__) || defined(__MK64FX512__) || defined(__MK20DX256__) || defined(__MK20DX128__)
    // All other Teensy platforms use lightweight implementation
    println_teensy(str);
#else
    // Use generic Arduino print for all other platforms including:
    // - STM32 (STM32F1, STM32F4, STM32H7, ARDUINO_GIGA)
    // - NRF (NRF52, NRF52832, NRF52840, ARDUINO_NRF52_DK)  
    // - All other Arduino-compatible platforms
    println_arduino(str);
#endif
}

int available() {
#ifdef FASTLED_TESTING
    // Check for injected handler first
    if (get_available_handler()) {
        return get_available_handler()();
    }
#endif

#ifdef __EMSCRIPTEN__
    return available_wasm();
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    return available_native();
#elif defined(ESP32) || defined(ESP8266)
    return available_esp();
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    return available_avr();
#elif defined(__MKL26Z64__)
    // Teensy LC uses special no-op functions to avoid _write linker issues
    return available_teensy_lc();
#elif defined(__IMXRT1062__) || defined(__MK66FX1M0__) || defined(__MK64FX512__) || defined(__MK20DX256__) || defined(__MK20DX128__)
    // All other Teensy platforms use lightweight implementation
    return available_teensy();
#else
    // Use generic Arduino input for all other platforms including:
    // - STM32 (STM32F1, STM32F4, STM32H7, ARDUINO_GIGA)
    // - NRF (NRF52, NRF52832, NRF52840, ARDUINO_NRF52_DK)
    // - All other Arduino-compatible platforms
    return available_arduino();
#endif
}

int read() {
#ifdef FASTLED_TESTING
    // Check for injected handler first
    if (get_read_handler()) {
        return get_read_handler()();
    }
#endif

#ifdef __EMSCRIPTEN__
    return read_wasm();
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    return read_native();
#elif defined(ESP32) || defined(ESP8266)
    return read_esp();
#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
    return read_avr();
#elif defined(__MKL26Z64__)
    // Teensy LC uses special no-op functions to avoid _write linker issues
    return read_teensy_lc();
#elif defined(__IMXRT1062__) || defined(__MK66FX1M0__) || defined(__MK64FX512__) || defined(__MK20DX256__) || defined(__MK20DX128__)
    // All other Teensy platforms use lightweight implementation
    return read_teensy();
#else
    // Use generic Arduino input for all other platforms including:
    // - STM32 (STM32F1, STM32F4, STM32H7, ARDUINO_GIGA)
    // - NRF (NRF52, NRF52832, NRF52840, ARDUINO_NRF52_DK)
    // - All other Arduino-compatible platforms
    return read_arduino();
#endif
}

#ifdef FASTLED_TESTING

// Inject function handlers for testing
void inject_print_handler(const print_handler_t& handler) {
    get_print_handler() = handler;
}

void inject_println_handler(const println_handler_t& handler) {
    get_println_handler() = handler;
}

void inject_available_handler(const available_handler_t& handler) {
    get_available_handler() = handler;
}

void inject_read_handler(const read_handler_t& handler) {
    get_read_handler() = handler;
}

// Clear all injected handlers (restores default behavior)
void clear_io_handlers() {
    get_print_handler() = print_handler_t{};
    get_println_handler() = println_handler_t{};
    get_available_handler() = available_handler_t{};
    get_read_handler() = read_handler_t{};
}

// Clear individual handlers
void clear_print_handler() {
    get_print_handler() = print_handler_t{};
}

void clear_println_handler() {
    get_println_handler() = println_handler_t{};
}

void clear_available_handler() {
    get_available_handler() = available_handler_t{};
}

void clear_read_handler() {
    get_read_handler() = read_handler_t{};
}

#endif // FASTLED_TESTING

} // namespace fl

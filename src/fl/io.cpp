#include "io.h"

// Include size_t for strlen_p function and uint8_t/uint32_t for Teensy
#include <stddef.h>
#include "fl/stdint.h"

// Platform-specific includes for native output (before Arduino fallbacks)
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#elif defined(_WIN32)
#include <io.h>  // for _write
#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__)
#include <unistd.h>  // for write
#endif

// ESP32 platforms - use native ESP-IDF logging
#if defined(ESP32) || defined(ESP8266)
#include "esp_log.h"
#endif

// STM32 platforms - use native UART or ITM
#if defined(STM32F1) || defined(STM32F4) || defined(STM32H7) || defined(ARDUINO_GIGA)
// STM32 ITM debug output
#ifdef __cplusplus
extern "C" {
#endif
// ITM register definitions for debug output
#define ITM_PORT8(n)          (*(volatile unsigned char *)(0xE0000000 + 4*(n)))
#define ITM_PORT16(n)         (*(volatile unsigned short*)(0xE0000000 + 4*(n)))
#define ITM_PORT32(n)         (*(volatile unsigned long *)(0xE0000000 + 4*(n)))
#define DEMCR                 (*(volatile unsigned long *)(0xE000EDFC))
#define ITM_TCR               (*(volatile unsigned long *)(0xE0000E80))
#define ITM_TER               (*(volatile unsigned long *)(0xE0000E00))
#define DWT_CTRL              (*(volatile unsigned long *)(0xE0001000))
#ifdef __cplusplus
}
#endif
#endif

// nRF52 platforms - use native UART
#if defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(ARDUINO_NRF52_DK)
// nRF52 UART register definitions
#define NRF_UART0_BASE        0x40002000UL
#define UART_ENABLE_OFFSET    0x500
#define UART_PSELTXD_OFFSET   0x50C
#define UART_STARTTX_OFFSET   0x008
#define UART_TXD_OFFSET       0x51C
#define UART_EVENTS_TXDRDY_OFFSET 0x11C
#endif

// Teensy platforms - use native USB or UART
#if defined(__IMXRT1062__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
// Teensy has usb_serial_putchar for direct USB output
extern "C" {
int usb_serial_putchar(uint8_t c);
int usb_serial_write(const void *buffer, uint32_t size);
}
#endif

// AVR platforms - use native UART registers
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR)
#include <avr/io.h>
// UART register access for direct output
#ifdef UDR0
#define UART_UDR UDR0
#define UART_UCSRA UCSR0A
#define UART_UDRE UDRE0
#elif defined(UDR)
#define UART_UDR UDR
#define UART_UCSRA UCSRA
#define UART_UDRE UDRE
#endif
#endif

namespace fl {

// Helper function to calculate string length without depending on strlen
// Only used for Teensy platforms
#if defined(__IMXRT1062__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
static size_t strlen_p(const char* str) {
    if (!str) return 0;
    size_t len = 0;
    while (*str++) len++;
    return len;
}
#endif

// Helper function for ESP32 native logging
#if defined(ESP32) || defined(ESP8266)
static const char* FL_TAG = "FastLED";
#endif

// Helper function for STM32 ITM output
#if defined(STM32F1) || defined(STM32F4) || defined(STM32H7) || defined(ARDUINO_GIGA)
static void itm_putchar(char c) {
    // Check if ITM is enabled and port 0 is enabled
    if ((ITM_TCR & 1) && (ITM_TER & 1)) {
        while ((ITM_PORT8(0) & 1) == 0); // Wait until port is available
        ITM_PORT8(0) = c;
    }
}
#endif

// Helper function for nRF52 UART output
#if defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(ARDUINO_NRF52_DK)
static void nrf_uart_putchar(char c) {
    volatile uint32_t* uart_base = (volatile uint32_t*)NRF_UART0_BASE;
    // Check if UART is enabled
    if (*(uart_base + (UART_ENABLE_OFFSET / 4)) != 0) {
        *(uart_base + (UART_TXD_OFFSET / 4)) = c;
        *(uart_base + (UART_STARTTX_OFFSET / 4)) = 1;
        // Wait for transmission to complete
        while (*(uart_base + (UART_EVENTS_TXDRDY_OFFSET / 4)) == 0);
        *(uart_base + (UART_EVENTS_TXDRDY_OFFSET / 4)) = 0;
    }
}
#endif

// Helper function for AVR UART output
#if defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR) && defined(UART_UDR)
static void avr_uart_putchar(char c) {
    // Wait for empty transmit buffer
    while (!(UART_UCSRA & (1 << UART_UDRE)));
    // Put data into buffer, sends the data
    UART_UDR = c;
}
#endif

void print(const char* str) {
    if (!str) return;

#ifdef __EMSCRIPTEN__
    // WASM: Use JavaScript console.log (via printf)
    ::printf("%s", str);

#elif defined(FASTLED_TESTING) || defined(__linux__) || defined(__APPLE__) || defined(_WIN32)
    // Native/Testing: Use direct system calls to stderr
    // Calculate length without strlen()
    size_t len = 0;
    const char* p = str;
    while (*p++) len++;
    
#ifdef _WIN32
    // Windows version
    _write(2, str, len);  // 2 = stderr
#else
    // Unix/Linux version
    ::write(2, str, len);  // 2 = stderr
#endif

#elif defined(ESP32) || defined(ESP8266)
    // ESP32/ESP8266: Use native ESP-IDF logging
    // This avoids Arduino Serial dependency and uses ESP's native UART
    ESP_LOGI(FL_TAG, "%s", str);

#elif defined(STM32F1) || defined(STM32F4) || defined(STM32H7) || defined(ARDUINO_GIGA)
    // STM32: Use ITM debug output first, then fall back to Serial
    // ITM output goes directly to SWO pin and can be captured by debuggers
    const char* p = str;
    bool itm_success = false;
    
    // Try ITM output first
    if ((ITM_TCR & 1) && (ITM_TER & 1)) {
        while (*p) {
            itm_putchar(*p++);
        }
        itm_success = true;
    }
    
    // If ITM failed and Arduino Serial is available, use it as fallback
    if (!itm_success) {
        #ifdef ARDUINO_H
        if (Serial) {
            Serial.print(str);
        }
        #endif
    }

#elif defined(__IMXRT1062__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__)
    // Teensy: Use native USB serial first
    if (usb_serial_write(str, strlen_p(str)) >= 0) {
        // Success with native USB
    } else {
        // Fallback to Arduino Serial if available
        #ifdef ARDUINO_H
        if (Serial) {
            Serial.print(str);
        }
        #endif
    }

#elif defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(ARDUINO_NRF52_DK)
    // nRF52: Use native UART first
    const char* p = str;
    volatile uint32_t* uart_base = (volatile uint32_t*)NRF_UART0_BASE;
    
    if (*(uart_base + (UART_ENABLE_OFFSET / 4)) != 0) {
        // UART is enabled, use native output
        while (*p) {
            nrf_uart_putchar(*p++);
        }
    } else {
        // Fallback to Arduino Serial if available
        #ifdef ARDUINO_H
        if (Serial) {
            Serial.print(str);
        }
        #endif
    }

#elif defined(__AVR__) && !defined(ARDUINO_ARCH_MEGAAVR) && defined(UART_UDR)
    // AVR: Use native UART registers first (if UART is initialized)
    const char* p = str;
    
    // Check if UART seems to be initialized (basic check)
    if (UART_UCSRA != 0xFF) {  // 0xFF usually indicates uninitialized
        while (*p) {
            avr_uart_putchar(*p++);
        }
    } else {
        // Fallback to Arduino Serial if available
        #ifdef ARDUINO_H
        if (Serial) {
            Serial.print(str);
        }
        #endif
    }

#else
    // Generic Arduino platforms and final fallback
    // Only use Serial if Arduino.h has been included and Serial is available
    #ifdef ARDUINO_H
    if (Serial) {
        Serial.print(str);
    }
    #endif
    // If no Serial available, output goes nowhere (silent)
    // This prevents crashes on platforms where Serial isn't initialized
#endif
}

void println(const char* str) {
    if (!str) return;
    print(str);
    print("\n");
}

} // namespace fl

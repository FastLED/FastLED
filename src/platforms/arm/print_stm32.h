#pragma once

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

namespace fl {

// Helper function for STM32 ITM output
static inline void itm_putchar(char c) {
    // Check if ITM is enabled and port 0 is enabled
    if ((ITM_TCR & 1) && (ITM_TER & 1)) {
        while ((ITM_PORT8(0) & 1) == 0); // Wait until port is available
        ITM_PORT8(0) = c;
    }
}

inline void print_stm32(const char* str) {
    if (!str) return;
    
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
}

inline void println_stm32(const char* str) {
    if (!str) return;
    print_stm32(str);
    print_stm32("\n");
}

} // namespace fl

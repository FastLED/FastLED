#ifndef __INC_LED_SYSDEFS_STUB_H
#define __INC_LED_SYSDEFS_STUB_H

#ifndef FASTLED_STUB_IMPL
#define FASTLED_STUB_IMPL
#endif

#include "fl/stdint.h"

#ifndef F_CPU
#define F_CPU 1000000000
#endif // F_CPU

#ifndef FASTLED_HAS_MILLIS
#define FASTLED_HAS_MILLIS 1
#endif // FASTLED_HAS_MILLIS

#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#endif

#define FASTLED_USE_PROGMEM 0
#define INTERRUPT_THRESHOLD 0

#define digitalPinToBitMask(P) ( 0 )
#define digitalPinToPort(P) ( 0 )
#define portOutputRegister(P) ( 0 )
#define portInputRegister(P) ( 0 )

#ifndef INPUT
#define INPUT  0
#endif

#ifndef OUTPUT
#define OUTPUT 1
#endif

#ifndef INPUT_PULLUP
#define INPUT_PULLUP 2
#endif

#if INPUT != 0
#error "INPUT is not 0"
#endif
#if OUTPUT != 1
#error "OUTPUT is not 1"
#endif
#if INPUT_PULLUP != 2
#error "INPUT_PULLUP is not 2"
#endif

typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;

extern "C" {
    void pinMode(uint8_t pin, uint8_t mode);

    uint32_t millis(void);
    uint32_t micros(void);

    void delay(int ms);
    void yield(void);
}

#endif // __INC_LED_SYSDEFS_STUB_H

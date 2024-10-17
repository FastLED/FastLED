#ifndef __INC_LED_SYSDEFS_STUB_H
#define __INC_LED_SYSDEFS_STUB_H

#ifndef FASTLED_STUB_IMPL
#define FASTLED_STUB_IMPL
#endif

#include <stdint.h>

#ifndef F_CPU
#define F_CPU 1000000000
#endif // F_CPU

#ifndef FASTLED_HAS_MILLIS
#define FASTLED_HAS_MILLIS 1
#endif // FASTLED_HAS_MILLIS


#define FASTLED_USE_PROGMEM 0
#define FASTLED_ALLOW_INTERRUPTS 1
#define INTERRUPT_THRESHOLD 0

#define digitalPinToBitMask(P) ( 0 )
#define digitalPinToPort(P) ( 0 )
#define portOutputRegister(P) ( 0 )
#define portInputRegister(P) ( 0 )

#define INPUT  0
#define OUTPUT 1

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
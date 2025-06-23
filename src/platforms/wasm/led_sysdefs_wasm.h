#pragma once

#ifndef FASTLED_STUB_IMPL
#define FASTLED_STUB_IMPL
#endif

#include "platforms/wasm/compiler/Arduino.h"
#include "fl/stdint.h"

#ifndef F_CPU
#define F_CPU 1000000000
#endif // F_CPU

#ifndef FASTLED_HAS_MILLIS
#define FASTLED_HAS_MILLIS 1
#endif // FASTLED_HAS_MILLIS

#ifdef FASTLED_ALLOW_INTERRUPTS
#undef FASTLED_ALLOW_INTERRUPTS
#endif

#define FASTLED_USE_PROGMEM 0
#define FASTLED_ALLOW_INTERRUPTS 1
#define INTERRUPT_THRESHOLD 0

#define digitalPinToBitMask(P) (0)
#define digitalPinToPort(P) (0)
#define portOutputRegister(P) (0)
#define portInputRegister(P) (0)

#define INPUT 0
#define OUTPUT 1

// These are fake and any number can be used.
#define MOSI 9
#define MISO 8
#define SCK 7


typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;

extern "C" {

// #ifndef SKETCH_COMPILE
// void pinMode(uint8_t pin, uint8_t mode);
// #endif

// uint32_t millis(void);
// uint32_t micros(void);

void delay(int ms);
void yield(void);
}

#pragma once

#ifndef INTERRUPT_THRESHOLD
#define INTERRUPT_THRESHOLD 1
#endif

// Default to allowing interrupts
#ifndef FASTLED_ALLOW_INTERRUPTS
#define FASTLED_ALLOW_INTERRUPTS 1
#endif

#if FASTLED_ALLOW_INTERRUPTS == 1
#define FASTLED_ACCURATE_CLOCK
#endif

// Use ARM CMSIS functions for interrupt control
#define cli()  __disable_irq();
#define sei() __enable_irq();

// MGM240 specific defines
#ifndef F_CPU
#define F_CPU 78000000L  // 78MHz default for MGM240
#endif

// Disable PROGMEM for ARM platforms
#ifndef FASTLED_USE_PROGMEM
#define FASTLED_USE_PROGMEM 0
#endif

// Type definitions for register access
typedef volatile uint32_t RwReg;  // ARM Cortex-M33 uses 32-bit registers
typedef volatile uint32_t RoReg;  // Read-only register

// Arduino compatibility - stub out pin manipulation functions
// These are not used when hardware fastpin is available
#ifndef digitalPinToBitMask
#define digitalPinToBitMask(P) (1 << (P))
#endif

#ifndef digitalPinToPort
#define digitalPinToPort(P) ((P) / 8)
#endif

#ifndef portOutputRegister
#define portOutputRegister(P) ((volatile uint32_t*)(0x40000000 + (P) * 0x400))
#endif

#ifndef portInputRegister
#define portInputRegister(P) ((volatile uint32_t*)(0x40000000 + (P) * 0x400))
#endif

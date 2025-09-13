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

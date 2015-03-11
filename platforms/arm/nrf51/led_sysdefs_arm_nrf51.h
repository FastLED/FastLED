#ifndef __LED_SYSDEFS_ARM_NRF51
#define __LED_SYSDEFS_ARM_NRF51

#ifndef NRF51
#define NRF51
#endif

#define LED_TIMER NRF_TIMER1
#define FASTLED_NO_PINMAP
#define FASTLED_HAS_CLOCKLESS

#define FASTLED_ARM

#ifndef F_CPU
#define F_CPU 16000000
#endif

#include <stdint.h>
#include "nrf51.h"
#include "core_cm0.h"

typedef volatile uint32_t RoReg;
typedef volatile uint32_t RwReg;
typedef uint32_t prog_uint32_t;
typedef uint8_t boolean;

#define PROGMEM
#define NO_PROGMEM
#define NEED_CXX_BITS

#define cli()  __disable_irq();
#define sei() __enable_irq();

#endif

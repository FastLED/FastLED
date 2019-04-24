#ifndef __LED_SYSDEFS_ARM_NRF52
#define __LED_SYSDEFS_ARM_NRF52


#define FASTLED_ARM

#ifndef F_CPU
    #define F_CPU 64000000 // the NRF52 series has a 64MHz CPU
#endif

// #define FASTLED_HAS_MILLIS
#define FASTLED_USE_PROGMEM 0 // nRF52 series have flat memory model
#define FASTLED_FORCE_SOFTWARE_SPI

#include <nrfx.h>

typedef __I  uint32_t RoReg;
typedef __IO uint32_t RwReg;

#define cli()  __disable_irq();
#define sei() __enable_irq();

#endif

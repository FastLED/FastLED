#ifndef __INC_LED_SYSDEFS_H
#define __INC_LED_SYSDEFS_H

#include "fastled_config.h"

#if defined(NRF51) || defined(__RFduino__)
#include "platforms/arm/nrf51/led_sysdefs_arm_nrf51.h"
#elif defined(__MK20DX128__) || defined(__MK20DX256__)
// Include k20/T3 headers
#include "platforms/arm/k20/led_sysdefs_arm_k20.h"
#elif defined(__MKL26Z64__)
// Include k26/T-LC headers
#include "platforms/arm/k26/led_sysdefs_arm_k26.h"
#elif defined(__SAM3X8E__)
// Include sam/due headers
#include "platforms/arm/sam/led_sysdefs_arm_sam.h"
#elif defined(STM32F10X_MD)
#include "platforms/arm/stm32/led_sysdefs_arm_stm32.h"
#else
// AVR platforms
#include "platforms/avr/led_sysdefs_avr.h"
#endif

#ifndef FASTLED_NAMESPACE_BEGIN
#define FASTLED_NAMESPACE_BEGIN
#define FASTLED_NAMESPACE_END
#define FASTLED_USING_NAMESPACE
#endif

// Arduino.h needed for convenience functions digitalPinToPort/BitMask/portOutputRegister and the pinMode methods.
#ifdef ARDUINO
#include<Arduino.h>
#endif

#define CLKS_PER_US (F_CPU/1000000)

#endif

#pragma once

#include "fastled_config.h"




/// @file platforms.h
/// Determines which platforms headers to include


#if defined(NRF51)
#include "platforms/arm/nrf51/fastled_arm_nrf51.h"
#elif defined(NRF52_SERIES)
#include "platforms/arm/nrf52/fastled_arm_nrf52.h"
#elif defined(__MK20DX128__) || defined(__MK20DX256__)
// Teensy 3.0/3.1/3.2 (MK20DX Cortex-M4)
#include "platforms/arm/teensy/teensy31_32/fastled_arm_k20.h"
#elif defined(__MK66FX1M0__) || defined(__MK64FX512__)
// Teensy 3.5/3.6 (MK64/MK66 Cortex-M4)
#include "platforms/arm/teensy/teensy36/fastled_arm_k66.h"
#elif defined(__MKL26Z64__)
// Teensy LC (MKL26Z64 Cortex-M0+)
#include "platforms/arm/teensy/teensy_lc/fastled_arm_kl26.h"
#elif defined(__IMXRT1062__)
// Teensy 4.0/4.1 (IMXRT1062 Cortex-M7)
#include "platforms/arm/teensy/teensy4_common/fastled_arm_mxrt1062.h"
#elif defined(__SAM3X8E__)
// Include sam/due headers
#include "platforms/arm/sam/fastled_arm_sam.h"
#elif defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F1xx) || defined(STM32F2XX) || defined(STM32F4)
#include "platforms/arm/stm32/fastled_arm_stm32.h"
#elif defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || defined(__SAMD21E18A__)
#include "platforms/arm/d21/fastled_arm_d21.h"
#elif defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
#include "platforms/arm/d51/fastled_arm_d51.h"
#elif defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2350)
// RP2350 (Raspberry Pi Pico 2)
#include "platforms/arm/rp/rp2350/fastled_arm_rp2350.h"
#elif defined(ARDUINO_ARCH_RP2040) || defined(PICO_RP2040)
// RP2040 (Raspberry Pi Pico etc)
#include "platforms/arm/rp/rp2040/fastled_arm_rp2040.h"
#elif defined(ESP8266)
#include "platforms/esp/8266/fastled_esp8266.h"
#elif defined(ESP32)
#include "platforms/esp/32/core/fastled_esp32.h"
#elif defined(ARDUINO_ARCH_APOLLO3)
#include "platforms/apollo3/fastled_apollo3.h"
#elif defined(ARDUINO_ARCH_RENESAS) || defined(ARDUINO_ARCH_RENESAS_UNO) || defined(ARDUINO_ARCH_RENESAS_PORTENTA)
#include "platforms/arm/renesas/fastled_arm_renesas.h"
#elif defined(ARDUINO_GIGA) || defined(ARDUINO_GIGA_M7)
#include "platforms/arm/giga/fastled_arm_giga.h"
#elif defined(ARDUINO_ARCH_SILABS)
// Silicon Labs MGM240 (EFR32MG24 SoC) - Arduino Nano Matter, SparkFun Thing Plus Matter
// Also supports Seeed Xiao MG24 Sense and other EFR32MG24-based boards
#include "platforms/arm/mgm240/fastled_arm_mgm240.h"
#elif defined(__x86_64__) || defined(FASTLED_STUB_IMPL)

// stub platform for testing (on cpu)
#include "platforms/stub/fastled_stub.h"
#else
// AVR platforms
#include "platforms/avr/fastled_avr.h"
#endif

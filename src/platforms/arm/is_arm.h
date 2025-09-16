#pragma once

#ifndef FASTLED_IS_ARM
#if defined(__SAM3X8E__)     || defined(__MK20DX128__) || defined(__MK20DX256__)     || defined(__MKL26Z64__) || defined(__IMXRT1062__)     || defined(ARDUINO_ARCH_RENESAS_UNO)     || defined(STM32F1) || defined(STM32F4)     || defined(ARDUINO_GIGA) || defined(ARDUINO_GIGA_M7)     || defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52)     || defined(NRF52840_XXAA) || defined(ARDUINO_ARCH_APOLLO3)     || defined(FASTLED_APOLLO3) || defined(ARDUINO_ARCH_RP2040)     || defined(TARGET_RP2040)     || defined(PICO_32BIT) || defined(ARDUINO_RASPBERRY_PI_PICO)     || defined(ARDUINO_ARCH_SILABS)     || defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E17A__) || defined(__SAMD21E18A__)     || defined(__SAMD51G19A__) || defined(__SAMD51J19A__) || defined(__SAME51J19A__) || defined(__SAMD51P19A__) || defined(__SAMD51P20A__)
#define FASTLED_IS_ARM
#endif
#endif  // FASTLED_IS_ARM

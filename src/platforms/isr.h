/*
  FastLED — Cross-Platform ISR Platform Dispatch Header
  -----------------------------------------------------
  Dispatches ISR implementation to platform-specific headers.
  Each platform header provides inline implementations that call fl::platform::* functions.

  Uses compiler-defined builtin macros for platform detection to avoid
  circular dependency issues with FL_IS_* macros from is_platform.h.

  License: MIT (FastLED)
*/

#pragma once

#include "fl/isr.h"

// ok no namespace fl - dispatch header includes platform-specific implementations

// Platform detection using compiler builtins
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    #include "platforms/stub/isr_stub.hpp"
#elif defined(ESP32)
    #include "platforms/esp/32/isr_esp32.hpp"
#elif defined(TEENSYDUINO) || defined(__MKL26Z64__) || defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK64FX512__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
    #include "platforms/arm/teensy/isr_teensy.hpp"
#elif defined(STM32F10X_MD) || defined(__STM32F1__) || defined(STM32F1) || defined(STM32F1xx) || \
      defined(STM32F2XX) || defined(STM32F2xx) || \
      defined(STM32F4) || defined(STM32F4xx) || \
      defined(STM32F7) || defined(STM32F7xx) || \
      defined(STM32L4) || defined(STM32L4xx) || \
      defined(STM32H7) || defined(STM32H7xx) || \
      defined(STM32G4) || defined(STM32G4xx) || \
      defined(STM32U5) || defined(STM32U5xx)
    #include "platforms/arm/stm32/isr_stm32.hpp"
#elif defined(NRF52_SERIES) || defined(ARDUINO_ARCH_NRF52) || defined(NRF52832_XXAA) || defined(NRF52833_XXAA) || defined(NRF52840_XXAA)
    #include "platforms/arm/nrf52/isr_nrf52.hpp"
#elif defined(__AVR__)
    #include "platforms/avr/isr_avr.hpp"
#elif defined(ARDUINO_ARCH_RP2040) || defined(PICO_BOARD)
    #include "platforms/arm/rp/isr_rp2040.hpp"
#elif defined(ARDUINO_ARCH_SAMD) || defined(__SAMD21__) || defined(__SAMD51__)
    #include "platforms/arm/samd/isr_samd.hpp"
#elif defined(__SAM3X8E__) || defined(ARDUINO_SAM_DUE)
    #include "platforms/arm/sam/isr_sam.hpp"
#else
    #include "platforms/isr_null.hpp"
#endif

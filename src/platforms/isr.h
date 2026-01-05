/*
  FastLED — Cross-Platform ISR Platform Dispatch Header
  -----------------------------------------------------
  Dispatches ISR implementation to platform-specific headers.
  Each platform header provides inline implementations that call fl::platform::* functions.

  Uses standardized FL_IS_* macros from is_platform.h for platform detection.

  License: MIT (FastLED)
*/

#pragma once

#include "platforms/is_platform.h"

// ok no namespace fl - dispatch header includes platform-specific implementations

// Platform detection using FL_IS_* macros
#if defined(FL_IS_STUB)
    #include "platforms/stub/isr_stub.hpp"
#elif defined(FL_IS_WASM)
    #include "platforms/wasm/isr_wasm.hpp"
#elif defined(FL_IS_ESP32)
    #include "platforms/esp/32/isr_esp32.hpp"
#elif defined(FL_IS_TEENSY)
    #include "platforms/arm/teensy/isr_teensy.hpp"
#elif defined(FL_IS_STM32)
    #include "platforms/arm/stm32/isr_stm32.hpp"
#elif defined(FL_IS_NRF52)
    #include "platforms/arm/nrf52/isr_nrf52.hpp"
#elif defined(FL_IS_AVR)
    // AVR platform: Only ATmega chips have Timer1 hardware support
    // ATtiny chips fall back to null implementation (no compatible Timer1)
    #include "platforms/avr/isr_avr.hpp"
    // If isr_avr.hpp didn't define the implementation (ATtiny case), include null
    #ifndef FL_ISR_AVR_IMPLEMENTED
        #include "platforms/isr_null.hpp"
    #endif
#elif defined(FL_IS_RP)
    #include "platforms/arm/rp/isr_rp.hpp"
#elif defined(FL_IS_SAMD)
    #include "platforms/arm/samd/isr_samd.hpp"
#elif defined(FL_IS_SAM)
    #include "platforms/arm/sam/isr_sam.hpp"
#else
    #include "platforms/isr_null.hpp"
#endif

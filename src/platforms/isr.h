/*
  FastLED — Cross-Platform ISR Platform Dispatch Header
  -----------------------------------------------------
  Dispatches ISR implementation to platform-specific headers.
  Each platform header provides inline implementations that call fl::platform::* functions.

  License: MIT (FastLED)
*/

#pragma once

#include "fl/isr.h"

// ok no namespace fl - dispatch header includes platform-specific implementations

// Platform detection
#if defined(STUB_PLATFORM) || defined(FASTLED_STUB_IMPL)
    #include "platforms/stub/isr_stub.h"
#elif defined(ESP32)
    #include "platforms/esp/32/isr_esp32.h"
#elif defined(FL_IS_TEENSY)
    #include "platforms/arm/teensy/isr_teensy.h"
#elif defined(FL_IS_STM32)
    #include "platforms/arm/stm32/isr_stm32.h"
#elif defined(FL_IS_NRF52)
    #include "platforms/arm/nrf52/isr_nrf52.h"
#elif defined(FL_IS_AVR)
    #include "platforms/avr/isr_avr.h"
#elif defined(FL_IS_RP2040)
    #include "platforms/arm/rp/isr_rp2040.h"
#elif defined(FL_IS_SAMD)
    #include "platforms/arm/samd/isr_samd.h"
#else
    #include "platforms/isr_null.h"
#endif

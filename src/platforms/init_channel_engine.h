#pragma once

// IWYU pragma: public

// ok no namespace fl - Platform dispatch header only
// This header dispatches to platform-specific implementations

#if defined(ESP32)
    #include "platforms/esp/32/init_channel_engine.h"
#include "platforms/arm/stm32/is_stm32.h"
#include "platforms/arm/teensy/is_teensy.h"

#elif defined(FL_IS_STM32)
    #include "platforms/arm/stm32/init_channel_engine.h"
#elif defined(FL_IS_TEENSY_4X)
    #include "platforms/arm/teensy/teensy4_common/init_channel_engine.h"
#elif defined(PICO_RP2040) || defined(PICO_RP2350) || defined(ARDUINO_ARCH_RP2040) || defined(ARDUINO_ARCH_RP2350)
    #include "platforms/arm/rp/rpcommon/init_channel_engine.h"
#elif defined(ARDUINO_SAMD51) || defined(__SAMD51__) || defined(__SAMD51J19A__) || defined(__SAMD51J20A__) || defined(__SAMD51G19A__) || defined(ADAFRUIT_FEATHER_M4_EXPRESS) || defined(ADAFRUIT_METRO_M4_EXPRESS)
    #include "platforms/arm/d51/init_channel_engine.h"
#elif defined(ARDUINO_SAMD_ZERO) || defined(ADAFRUIT_FEATHER_M0) || defined(ARDUINO_SAMD_FEATHER_M0) || defined(ARDUINO_SAM_ZERO) || defined(__SAMD21G18A__) || defined(__SAMD21J18A__) || defined(__SAMD21E18A__)
    #include "platforms/arm/d21/init_channel_engine.h"
#elif defined(NRF52) || defined(NRF52832) || defined(NRF52840) || defined(ARDUINO_NRF52_ADAFRUIT)
    #include "platforms/arm/nrf52/init_channel_engine.h"
#else
    #include "platforms/shared/init_channel_engine.h"
#endif

/// @file fl.system.analog+.cpp
/// @brief Unity build entry-point for `fl::analogRead`, split out of
/// `fl.system+.cpp` (which every sketch links) so the platform ADC driver is
/// referenced only when a sketch reads an analog pin. On ESP32 the IDF
/// one-shot ADC reference extracted `libesp_adc.a(adc_common.c.obj)`, whose
/// boot-time constructor runs ADC self-calibration and links the efuse
/// calibration code into every sketch (FastLED #4796).

#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/system/arduino.h"
// IWYU pragma: end_keep

#include "fl/system/analog/_build.cpp.hpp"

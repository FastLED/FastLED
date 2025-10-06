/// @file    OctoWS2811Demo.ino
/// @brief   OctoWS2811Demo example with platform detection
/// @example OctoWS2811Demo.ino

// Platform detection: OctoWS2811 requires Teensy 3.x or Teensy 4.x
// (Teensy LC is not supported due to hardware limitations)
#if defined(__MK20DX128__) || defined(__MK20DX256__) || defined(__MK66FX1M0__) || defined(__IMXRT1062__)
#include "OctoWS2811Demo.h"
#else
#include "platforms/sketch_fake.hpp"
#endif

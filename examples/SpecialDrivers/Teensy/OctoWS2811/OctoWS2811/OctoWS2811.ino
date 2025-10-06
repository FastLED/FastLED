/// @file    OctoWS2811.ino
/// @brief   OctoWS2811 example with platform detection
/// @example OctoWS2811.ino

// Platform detection logic
#if defined(__arm__) && defined(TEENSYDUINO) && !defined(__MKL26Z64__)  // Teensy LC
#include "OctoWS2811_impl.h"
#else
#include "platforms/sketch_fake.hpp"
#endif

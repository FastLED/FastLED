/// @file    OctoWS2811Demo.ino
/// @brief   OctoWS2811Demo example with platform detection
/// @example OctoWS2811Demo.ino

// Platform detection logic
#if defined(__arm__) && defined(TEENSYDUINO)
#include "OctoWS2811Demo.h"
#else
platforms/sketch_fake.hpp
#endif

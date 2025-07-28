/// @file    Pintest.ino
/// @brief   Pintest example with platform detection
/// @example Pintest.ino

// Platform detection logic
#if defined(__AVR__) || (defined(__arm__) && defined(TEENSYDUINO))
#include "Pintest.h"
#else
platforms/sketch_fake.hpp
#endif


/// @file    SmartMatrix.ino
/// @brief   SmartMatrix example with platform detection
/// @example SmartMatrix.ino

// Platform detection logic
#if defined(__arm__) && defined(TEENSYDUINO)
#include "SmartMatrix.h"
#else
#include "platforms/sketch_fake.hpp"
#endif

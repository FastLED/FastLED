
/// @file    SmartMatrix.ino
/// @brief   SmartMatrix example with platform detection
/// @example SmartMatrix.ino

// Platform detection logic
#if defined(__arm__) && defined(TEENSYDUINO) && defined(SmartMatrix_h)
#include "SmartMatrixSketch.h"
#else
#include "platforms/sketch_fake.hpp"
#endif

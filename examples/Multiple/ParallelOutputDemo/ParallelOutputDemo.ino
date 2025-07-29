/// @file    ParallelOutputDemo.ino
/// @brief   ParallelOutputDemo example with platform detection
/// @example ParallelOutputDemo.ino

// Platform detection logic
#if defined(CORE_TEENSY) && defined(TEENSYDUINO) && defined(HAS_PORTDC)
#include "ParallelOutputDemo.h"
#else
#include "platforms/sketch_fake.hpp"
#endif

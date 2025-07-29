/// @file    PJRCSpectrumAnalyzer.ino
/// @brief   PJRCSpectrumAnalyzer example with platform detection
/// @example PJRCSpectrumAnalyzer.ino

// Platform detection logic
#if defined(__arm__) && defined(TEENSYDUINO)
#include "PJRCSpectrumAnalyzer.h"
#else
#include "platforms/sketch_fake.hpp"
#endif

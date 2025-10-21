// @filter: (platform is teensy) and (memory is high)

/// @file    PJRCSpectrumAnalyzer.ino
/// @brief   PJRCSpectrumAnalyzer example with platform detection
/// @example PJRCSpectrumAnalyzer.ino

// Platform detection logic
// Requires lots of memory for 60x32 LED matrix + Audio library + FFT
// Only supported on Teensy models with OctoWS2811 and sufficient RAM:
//   - Teensy 3.5 (__MK64FX512__)
//   - Teensy 3.6 (__MK66FX1M0__)
//   - Teensy 4.0/4.1 (__IMXRT1062__)
// NOT supported on: Teensy 3.0/3.1/3.2 (insufficient RAM), Teensy LC (no OctoWS2811)

#include "PJRCSpectrumAnalyzer.h"

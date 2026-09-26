/// Massive parallel output on Teensy 4.0/4.1 via the ObjectFLED driver.
///
/// Features:
/// - DMA-driven multi-strip LED control (minimal CPU overhead)
/// - Per-strip color correction and temperature
/// - Automatic chipset timing (WS2812, SK6812, WS2811, etc.)
///
/// @author Kurt Funderburg (original ObjectFLED)
/// @reddit: reddit.com/u/Tiny_Structure_7
/// FastLED integration by Zach Vorhies

// @filter: (platform is teensy) and (board is teensy40 or teensy41)

#include "./TeensyMassiveParallel.h"

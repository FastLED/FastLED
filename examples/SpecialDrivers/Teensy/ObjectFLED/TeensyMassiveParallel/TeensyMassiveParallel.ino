/// Massive parallel output example using BulkClockless<OFLED> for Teensy 4.0/4.1.
///
/// This example demonstrates the new BulkClockless API with OFLED (ObjectFLED) peripheral,
/// supporting up to 42 parallel strips on Teensy 4.1 or 16 strips on Teensy 4.0.
///
/// Key Features:
/// - DMA-driven multi-strip LED control (minimal CPU overhead)
/// - Per-strip color correction and temperature
/// - Automatic chipset timing (WS2812, SK6812, WS2811, etc.)
/// - Up to 23,000 pixels at 60fps (42 strips × 550 LEDs each)
///
/// @author Kurt Funderburg (original ObjectFLED)
/// @reddit: reddit.com/u/Tiny_Structure_7
/// FastLED integration by Zach Vorhies

// @filter: (platform is teensy) and (target is Teensy40 or Teensy41)

#include "./TeensyMassiveParallel.h"

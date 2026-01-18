/// ⚠️ DEPRECATED EXAMPLE - BulkClockless API has been removed
///
/// This example demonstrates the obsolete BulkClockless API which has been
/// superseded by the Channel/ChannelEngine API.
///
/// ⚠️ WARNING: This example will NOT compile in current FastLED versions.
///
/// Migration Path:
/// - The BulkClockless API has been replaced by the Channel API
/// - See fl/channels/channel.h for the new API documentation
/// - New examples using the Channel API will be added in future releases
///
/// Original Features (now deprecated):
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

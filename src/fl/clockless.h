#pragma once

/// @file fl/clockless.h
/// @brief Top-level anchor for BulkClockless API - includes all components
///
/// This header provides a single entry point for the BulkClockless API, which allows
/// managing multiple LED strips using shared hardware peripherals (LCD_I80, RMT, I2S, SPI).
///
/// @par Directory Structure:
/// - `fl/clockless.h` - This file (top-level anchor)
/// - `fl/clockless/base.h` - Base template class
/// - `fl/clockless/bulk_strip.h` - Per-strip descriptor
/// - `fl/clockless/peripheral_tags.h` - Peripheral type tags (LCD_I80, RMT, I2S, SPI_BULK)
/// - `fl/clockless/cpu_fallback.h` - CPU fallback implementation
///
/// @par Platform Specializations:
/// - `platforms/esp/32/drivers/rmt/bulk_rmt.h` - RMT peripheral (ESP32)
/// - `platforms/esp/32/drivers/i2s/bulk_i2s.h` - I2S peripheral (ESP32/S3)
/// - `platforms/esp/32/drivers/lcd_cam/bulk_lcd_i80.h` - LCD_I80 peripheral (ESP32-S3/P4)
/// - `platforms/esp/32/drivers/parlio/bulk_clockless_parlio.h` - PARLIO peripheral (ESP32-P4)
///
/// @par Example Usage:
/// @code
/// #include "fl/clockless.h"
///
/// CRGB strip1[100], strip2[100];
/// auto& bulk = FastLED.addBulkLeds<Chipset::WS2812, RMT>({
///     {2, strip1, 100, ScreenMap()},
///     {4, strip2, 100, ScreenMap()}
/// });
/// bulk.setCorrection(TypicalLEDStrip);
/// bulk.get(2)->setTemperature(Tungsten100W);  // Per-strip override
/// FastLED.show();
/// @endcode
///
/// @see fl::BulkClockless
/// @see fl::BulkStrip
/// @see fl::BulkStripConfig

// Include components in correct dependency order
#include "fl/clockless/peripheral_tags.h"      // Must be first (defines types used by others)
#include "fl/chipsets/chipset_timing_config.h" // Runtime chipset timing configuration
#include "fl/clockless/bulk_strip.h"           // Must be before base.h (forward declared in base.h)
#include "fl/clockless/base.h"                 // Base template class
// #include "fl/clockless/cpu_fallback.h"      // TODO: CPU fallback not yet fully implemented

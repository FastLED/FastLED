#pragma once

// ok no namespace fl
// IWYU pragma: private

/// @file bus_traits.h
/// @brief Legacy ESP32-S3 I2S LCD_CAM trait placeholder.
///
/// Confusing legacy name: this driver uses the LCD_CAM peripheral in I2S mode
/// to drive clockless LED strips on ESP32-S3. The "I2S" name is retained here
/// for backward compatibility — the actual modern S3 clockless LCD_CAM driver
/// is now selected through the portable `Bus::FLEX_IO` slot.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_I2S_LCD_CAM

#include "platforms/esp/32/drivers/i2s/channel_driver_i2s.h"

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_I2S_LCD_CAM

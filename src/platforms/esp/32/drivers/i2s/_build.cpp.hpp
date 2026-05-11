// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\i2s/ directory
/// Includes all implementation files in alphabetical order
///
/// Phase 5c of #2428: I2S is never a platform default (LCD_CAM clockless on
/// ESP32-S3 is a niche / experimental driver). When the user opts in to
/// `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY=1`, this TU becomes empty and the
/// I2S driver is dropped from the binary.

#include "fl/channels/bus.h"  // brings in FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY

#if !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY

#include "platforms/esp/32/drivers/i2s/channel_driver_i2s.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/clockless_i2s_esp32.cpp.hpp"

#include "platforms/esp/32/drivers/i2s/i2s_esp32dev.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/i2s/i2s_lcd_cam_peripheral_mock.cpp.hpp"

#include "platforms/esp/32/drivers/i2s/wave8_encoder_i2s.cpp.hpp"

// BusTraits<Bus::I2S> specialization.
#include "platforms/esp/32/drivers/i2s/bus_traits.h"

#endif  // !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY

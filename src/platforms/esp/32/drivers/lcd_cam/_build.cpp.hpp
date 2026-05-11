// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\lcd_cam/ directory
/// Includes all implementation files in alphabetical order
///
/// Phase 5c of #2428: LCD_RGB is never a platform default. When the user
/// opts in to `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY=1`, this TU becomes
/// empty and the LCD_RGB driver is dropped from the binary.

#include "fl/channels/bus.h"  // brings in FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY

#if !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY

#include "platforms/esp/32/drivers/lcd_cam/channel_driver_lcd_rgb.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/lcd_cam/lcd_rgb_peripheral_mock.cpp.hpp"

// BusTraits<Bus::LCD_RGB> specialization.
#include "platforms/esp/32/drivers/lcd_cam/bus_traits.h"

#endif  // !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY

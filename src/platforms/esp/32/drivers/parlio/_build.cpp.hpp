// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms\esp\32\drivers\parlio/ directory
/// Includes all implementation files in alphabetical order
///
/// Phase 5c of #2428: PARLIO is the platform-default clockless driver on
/// ESP32-P4 / C6 / H2 / C5. On those variants the driver TU is always
/// compiled (legacy `addLeds<NEOPIXEL>` pre-binds to it). On all other
/// ESP32 variants PARLIO is an alternate driver, gated by
/// `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY`.

#include "fl/channels/bus.h"  // brings in FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY
#include "platforms/is_platform.h"

#if !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY || \
    defined(FL_IS_ESP_32P4) || defined(FL_IS_ESP_32C6) || \
    defined(FL_IS_ESP_32H2) || defined(FL_IS_ESP_32C5)

#include "platforms/esp/32/drivers/parlio/channel_driver_parlio.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/parlio_engine.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/parlio_peripheral_esp.cpp.hpp"
#include "platforms/esp/32/drivers/parlio/parlio_peripheral_mock.cpp.hpp"

// BusTraits<Bus::PARLIO> specialization.
#include "platforms/esp/32/drivers/parlio/bus_traits.h"

#endif  // legacy mode OR PARLIO-default platform

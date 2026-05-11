// IWYU pragma: private

/// @file _build.hpp
/// @brief Unity build header for platforms/esp/32/drivers/rmt/ directory
/// Includes all RMT driver implementations
///
/// Phase 5c of #2428: RMT is the platform-default clockless driver on every
/// ESP32 variant EXCEPT P4 / C6 / H2 / C5 (which default to PARLIO). On
/// RMT-default platforms this TU is always compiled (legacy
/// `addLeds<NEOPIXEL>` pre-binds to it). On PARLIO-default platforms RMT
/// is an alternate driver, gated by `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY`.

#include "fl/channels/bus.h"  // brings in FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY
#include "platforms/is_platform.h"

#if !FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY || \
    !(defined(FL_IS_ESP_32P4) || defined(FL_IS_ESP_32C6) || \
      defined(FL_IS_ESP_32H2) || defined(FL_IS_ESP_32C5))

// begin sub directory includes
#include "platforms/esp/32/drivers/rmt/rmt_4/_build.cpp.hpp"
#include "platforms/esp/32/drivers/rmt/rmt_5/_build.cpp.hpp"

#endif  // legacy mode OR RMT-default platform

#pragma once

/// @file fl/net/network_detector.h
/// @brief Cross-platform facade for runtime network activity detection.
///
/// The full implementation lives at
/// `src/platforms/esp/32/drivers/rmt/rmt_5/network_detector.h` and only compiles
/// when `FASTLED_RMT5` is set. This facade exposes a `fl::NetworkDetector` type
/// with the `isAnyNetworkActive()` accessor on every platform so that
/// platform-neutral code (such as `fl/channels/manager.cpp.hpp` and
/// `fl/channels/driver.cpp.hpp`) can perform a single runtime branch without
/// having to ifdef around the RMT5 gate.
///
/// On platforms / build configurations where the real detector is not
/// available, `isAnyNetworkActive()` returns false. That matches the
/// pre-existing behavior for ESP32-P4 (which has no radio at all) and for all
/// non-ESP32 platforms (where the deep-yield rationale from #2254 does not
/// apply in the first place).
///
/// Cost on platforms where the real implementation is present is the ~1-10 us
/// the ESP-IDF API takes per call (see the detailed docs on the
/// implementation header). On every other platform the call collapses to a
/// trivial `return false` and is folded into a constant by the optimizer.

#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
// IWYU pragma: begin_keep
#include "platforms/esp/32/feature_flags/enabled.h"  // ok platform headers
// IWYU pragma: end_keep
#endif

// Use the real RMT5 implementation when available; otherwise fall through to
// the stub below. The real header is itself wrapped in `#if FASTLED_RMT5`, so
// including it on RMT4-only / non-ESP32 builds is harmless (the class
// definition simply disappears).
#if defined(FL_IS_ESP32) && FASTLED_RMT5
// IWYU pragma: begin_keep
#include "platforms/esp/32/drivers/rmt/rmt_5/network_detector.h"  // ok platform headers
// IWYU pragma: end_keep
#define FL_NETWORK_DETECTOR_HAS_REAL_IMPL 1
#else
#define FL_NETWORK_DETECTOR_HAS_REAL_IMPL 0
#endif

#if !FL_NETWORK_DETECTOR_HAS_REAL_IMPL

#include "fl/stl/noexcept.h"

namespace fl {
namespace net {

/// @brief Stub NetworkDetector for platforms without the real implementation.
///
/// Returns false for every accessor. This is the correct answer on:
///   - ESP32-P4 (no radio silicon at all)
///   - Non-ESP32 platforms (no FreeRTOS / WiFi / BT, deep-yield concern N/A)
///   - ESP32 builds that disable the RMT5 driver (FASTLED_RMT5=0)
///
/// Behavior matches the existing FL_IS_ESP_32P4 carve-out in the channel
/// wait loops (refs #2493, #2815).
class NetworkDetector {
  public:
    static bool isWiFiActive() FL_NOEXCEPT { return false; }
    static bool isWiFiConnected() FL_NOEXCEPT { return false; }
    static bool isEthernetActive() FL_NOEXCEPT { return false; }
    static bool isEthernetConnected() FL_NOEXCEPT { return false; }
    static bool isBluetoothActive() FL_NOEXCEPT { return false; }
    static bool isAnyNetworkActive() FL_NOEXCEPT { return false; }
    static bool isAnyNetworkConnected() FL_NOEXCEPT { return false; }

  private:
    NetworkDetector() FL_NOEXCEPT = delete;
    ~NetworkDetector() FL_NOEXCEPT = delete;
    NetworkDetector(const NetworkDetector &) FL_NOEXCEPT = delete;
    NetworkDetector &operator=(const NetworkDetector &) FL_NOEXCEPT = delete;
};

} // namespace net
} // namespace fl

// Back-compat: the existing channel wait sites and the real RMT5 impl both
// expose the type as fl::NetworkDetector. Keep that name working in the
// no-real-impl case via a using alias, so this PR is a pure lint fix with
// no consumer churn.
namespace fl {
using NetworkDetector = ::fl::net::NetworkDetector;
} // namespace fl

#endif // !FL_NETWORK_DETECTOR_HAS_REAL_IMPL

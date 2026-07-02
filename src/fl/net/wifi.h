/// @file wifi.h
/// @brief fl::net::wifi — platform-neutral WiFi connection API
///        (FastLED#3576 Phase 6).
///
/// FastLED previously had no reusable connection manager: the only
/// credential-taking API was OTA-scoped (`fl::net::OTA::beginWiFi`) and
/// examples hand-rolled `esp_wifi_*` calls. This header is the fl-level
/// surface for bringing a network interface up so the rest of
/// `fl::net` (fetch, HTTP server, remote RPC) has something to run on.
///
/// All calls are asynchronous and non-blocking: `connectSta()` starts a
/// join and returns; poll `status()` / `isConnected()`. On platforms
/// without WiFi (host builds, WiFi-less MCUs like ESP32-H2) every call
/// is a no-op stub that reports `Status::UNSUPPORTED`.
///
/// Bluetooth connections live in the sibling `fl::net::ble` API
/// (GATT transport: `createTransport` / `queryStatus`).

#pragma once

#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"
#include "fl/stl/int.h"

#include "fl/stl/has_include.h"
#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32
#if FL_HAS_INCLUDE("soc/soc_caps.h")
// IWYU pragma: begin_keep
#include "soc/soc_caps.h"  // ok platform headers - SOC_WIFI_SUPPORTED for FL_WIFI_AVAILABLE
// IWYU pragma: end_keep
#endif
#endif

/// 1 when a real WiFi implementation is compiled in; 0 → stubs.
#if defined(FL_IS_ESP32) && defined(SOC_WIFI_SUPPORTED) && SOC_WIFI_SUPPORTED && \
    FL_HAS_INCLUDE(<esp_wifi.h>)
#define FL_WIFI_AVAILABLE 1
#else
#define FL_WIFI_AVAILABLE 0
#endif

namespace fl {
namespace net {
namespace wifi {

/// @brief Connection state, advanced by the platform's event handlers.
enum class Status : u8 {
    UNSUPPORTED = 0, ///< No WiFi on this platform (stub build)
    IDLE,            ///< WiFi not started
    CONNECTING,      ///< STA join in progress
    CONNECTED,       ///< STA has an IP
    FAILED,          ///< STA join gave up (bad credentials / no AP)
    AP_ACTIVE,       ///< SoftAP is up (no STA connection)
};

/// @brief Human-readable name for a Status value.
const char* toString(Status s) FL_NO_EXCEPT;

/// @brief Start an asynchronous station-mode join.
///
/// Returns immediately; poll status()/isConnected(). Calling again with
/// different credentials restarts the join. Coexists with an active
/// SoftAP (APSTA mode).
/// @return false if the WiFi driver could not be started at all.
bool connectSta(const char* ssid, const char* password) FL_NO_EXCEPT;

/// @brief Bring up a SoftAP.
/// @param channel 1-13; pass 0 for the default (channel 1).
/// @return false if the AP could not be started.
bool startAp(const char* ssid, const char* password, u8 channel) FL_NO_EXCEPT;

/// @brief Stop WiFi entirely (STA and AP).
void stop() FL_NO_EXCEPT;

/// @brief Current connection state.
Status status() FL_NO_EXCEPT;

/// @brief True when the station interface has an IP.
bool isConnected() FL_NO_EXCEPT;

/// @brief Dotted-quad IP of the station interface ("" until connected).
fl::string ipAddress() FL_NO_EXCEPT;

/// @brief Dotted-quad IP of the SoftAP interface ("" when AP is down).
fl::string apIpAddress() FL_NO_EXCEPT;

} // namespace wifi
} // namespace net
} // namespace fl

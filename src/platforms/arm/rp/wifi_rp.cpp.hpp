// IWYU pragma: private

/// @file wifi_rp.cpp.hpp
/// @brief CYW43-backed RP2350W implementation of fl::net::wifi.
///
/// Arduino-Pico supplies the CYW43/lwIP peripheral through its WiFi library.
/// beginNoBlock() starts an association without waiting for DHCP, which keeps
/// the fl::net::wifi API non-blocking just as it is on ESP32.

#include "fl/net/wifi.h"
#include "platforms/arm/rp/is_rp.h"

#if FL_WIFI_AVAILABLE && defined(FL_IS_RP2350)

#include "fl/stl/cstdio.h"
#include "fl/stl/noexcept.h"

// IWYU pragma: begin_keep
#include <WiFi.h>
// IWYU pragma: end_keep

namespace fl {
namespace net {
namespace wifi {

namespace {

struct WifiState {
    bool sta_requested = false;
    bool ap_active = false;
};

// FL_LINT_ALLOW_GLOBAL(platform WiFi state is a singleton supplied by Arduino-Pico)
WifiState g_state;

Status currentStatus() FL_NO_EXCEPT {
    if (WiFi.isConnected()) {
        return Status::CONNECTED;
    }
    if (g_state.sta_requested) {
        const int link_status = WiFi.status();
        if (link_status == WL_CONNECT_FAILED ||
            link_status == WL_NO_SSID_AVAIL ||
            link_status == WL_CONNECTION_LOST ||
            link_status == WL_NO_MODULE) {
            return Status::FAILED;
        }
        return Status::CONNECTING;
    }
    return g_state.ap_active ? Status::AP_ACTIVE : Status::IDLE;
}

fl::string ipToString(IPAddress ip) FL_NO_EXCEPT {
    char buffer[16];
    fl::snprintf(buffer, sizeof(buffer), "%u.%u.%u.%u",
                 static_cast<unsigned>(ip[0]),
                 static_cast<unsigned>(ip[1]),
                 static_cast<unsigned>(ip[2]),
                 static_cast<unsigned>(ip[3]));
    return fl::string(buffer);
}

} // namespace

bool connectSta(const char* ssid, const char* password) FL_NO_EXCEPT {
    if (ssid == nullptr || *ssid == '\0') {
        return false;
    }

    WiFi.mode(WIFI_STA);
    if (password != nullptr && *password != '\0') {
        WiFi.beginNoBlock(ssid, password);
    } else {
        WiFi.beginNoBlock(ssid);
    }

    // Arduino-Pico returns WL_IDLE_STATUS while the CYW43 link is still
    // coming up, so beginNoBlock's return value cannot distinguish that
    // healthy intermediate state from a startup failure. Report initiation
    // success and let status() surface a driver/link failure asynchronously.
    g_state.sta_requested = true;
    g_state.ap_active = false;
    return true;
}

bool startAp(const char* ssid, const char* password, u8 channel) FL_NO_EXCEPT {
    if (ssid == nullptr || *ssid == '\0') {
        return false;
    }

    WiFi.mode(WIFI_AP);
    const int result = (password != nullptr && *password != '\0')
                           ? WiFi.beginAP(ssid, password, channel)
                           : WiFi.beginAP(ssid, channel);
    if (result != WL_CONNECTED) {
        return false;
    }

    g_state.sta_requested = false;
    g_state.ap_active = true;
    return true;
}

void stop() FL_NO_EXCEPT {
    WiFi.end();
    g_state.sta_requested = false;
    g_state.ap_active = false;
}

Status status() FL_NO_EXCEPT {
    return currentStatus();
}

bool isConnected() FL_NO_EXCEPT {
    return currentStatus() == Status::CONNECTED;
}

fl::string ipAddress() FL_NO_EXCEPT {
    return isConnected() ? ipToString(WiFi.localIP()) : fl::string();
}

fl::string apIpAddress() FL_NO_EXCEPT {
    return g_state.ap_active ? ipToString(WiFi.softAPIP()) : fl::string();
}

} // namespace wifi
} // namespace net
} // namespace fl

#endif // FL_WIFI_AVAILABLE && FL_IS_RP2350

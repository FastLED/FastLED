// IWYU pragma: private

/// @file wifi_esp32.cpp.hpp
/// @brief ESP-IDF implementation of the fl::net::wifi connection API
///        (FastLED#3576 Phase 6). Stubs for other platforms live in
///        fl/net/wifi.cpp.hpp.
///
/// STA joins are asynchronous: connectSta() kicks the driver and
/// returns; WIFI_EVENT / IP_EVENT handlers advance the Status machine
/// (CONNECTING → CONNECTED on GOT_IP; a bounded retry loop on
/// disconnect, then FAILED). SoftAP can coexist (APSTA).

#include "fl/net/wifi.h"

#if FL_WIFI_AVAILABLE

#include "fl/log/log.h"
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {
namespace net {
namespace wifi {

namespace {

// Bounded auto-reconnect before declaring FAILED. Enough to ride out a
// slow AP boot on the dual-device bench without wedging the caller.
constexpr int kMaxStaRetries = 6;

struct WifiState {
    Status status = Status::IDLE;
    esp_netif_t *sta_netif = nullptr;
    esp_netif_t *ap_netif = nullptr;
    bool driver_started = false;
    bool ap_active = false;
    bool sta_requested = false;
    bool handlers_registered = false;
    int retries = 0;
};

// FL_LINT_ALLOW_GLOBAL(referenced from a C event-handler callback registered with the IDF event loop; zero-init POD keeps status readable before begin)
WifiState g_state;

void wifiEventHandler(void *, esp_event_base_t base, i32 id, void *) {
    if (base == WIFI_EVENT) {
        if (id == WIFI_EVENT_STA_START) {
            esp_wifi_connect();
        } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
            if (g_state.sta_requested && g_state.retries < kMaxStaRetries) {
                ++g_state.retries;
                g_state.status = Status::CONNECTING;
                esp_wifi_connect();
            } else if (g_state.sta_requested) {
                g_state.status = Status::FAILED;
            }
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        g_state.retries = 0;
        g_state.status = Status::CONNECTED;
    }
}

/// One-time driver + netif + event plumbing. Idempotent.
bool ensureDriver() FL_NO_EXCEPT {
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        FL_WARN_F("wifi: esp_netif_init failed: %s", esp_err_to_name(err));
        return false;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        FL_WARN_F("wifi: event loop create failed: %s", esp_err_to_name(err));
        return false;
    }
    if (!g_state.handlers_registered) {
        if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                &wifiEventHandler, nullptr,
                                                nullptr) != ESP_OK ||
            esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                &wifiEventHandler, nullptr,
                                                nullptr) != ESP_OK) {
            FL_WARN_F("wifi: event handler registration failed");
            return false;
        }
        g_state.handlers_registered = true;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        FL_WARN_F("wifi: esp_wifi_init failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool applyMode() FL_NO_EXCEPT {
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (g_state.sta_requested && g_state.ap_active) {
        mode = WIFI_MODE_APSTA;
    } else if (g_state.sta_requested) {
        mode = WIFI_MODE_STA;
    } else if (g_state.ap_active) {
        mode = WIFI_MODE_AP;
    }
    esp_err_t err = esp_wifi_set_mode(mode);
    if (err != ESP_OK) {
        FL_WARN_F("wifi: set_mode failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

fl::string netifIp(esp_netif_t *netif) FL_NO_EXCEPT {
    if (netif == nullptr) {
        return fl::string();
    }
    esp_netif_ip_info_t info;
    if (esp_netif_get_ip_info(netif, &info) != ESP_OK ||
        info.ip.addr == 0) {
        return fl::string();
    }
    char buf[16];
    esp_ip4addr_ntoa(&info.ip, buf, sizeof(buf));
    return fl::string(buf);
}

} // anonymous namespace

bool connectSta(const char *ssid, const char *password) FL_NO_EXCEPT {
    if (ssid == nullptr || *ssid == '\0') {
        return false;
    }
    if (!ensureDriver()) {
        return false;
    }
    if (g_state.sta_netif == nullptr) {
        g_state.sta_netif = esp_netif_create_default_wifi_sta();
        if (g_state.sta_netif == nullptr) {
            FL_WARN_F("wifi: STA netif creation failed");
            return false;
        }
    }

    wifi_config_t cfg;
    fl::memset(&cfg, 0, sizeof(cfg));
    fl::strncpy(reinterpret_cast<char *>(cfg.sta.ssid), ssid, // ok reinterpret cast - IDF field is uint8_t[]
                sizeof(cfg.sta.ssid) - 1);
    if (password != nullptr) {
        fl::strncpy(reinterpret_cast<char *>(cfg.sta.password), password, // ok reinterpret cast - IDF field is uint8_t[]
                    sizeof(cfg.sta.password) - 1);
    }

    g_state.sta_requested = true;
    g_state.retries = 0;
    g_state.status = Status::CONNECTING;
    if (!applyMode()) {
        return false;
    }
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &cfg);
    if (err != ESP_OK) {
        FL_WARN_F("wifi: STA set_config failed: %s", esp_err_to_name(err));
        return false;
    }
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        FL_WARN_F("wifi: start failed: %s", esp_err_to_name(err));
        return false;
    }
    if (g_state.driver_started) {
        // Driver was already running (mode change) — STA_START will not
        // re-fire, so kick the join directly.
        esp_wifi_connect();
    }
    g_state.driver_started = true;
    return true;
}

bool startAp(const char *ssid, const char *password, u8 channel) FL_NO_EXCEPT {
    if (ssid == nullptr || *ssid == '\0') {
        return false;
    }
    if (!ensureDriver()) {
        return false;
    }
    if (g_state.ap_netif == nullptr) {
        g_state.ap_netif = esp_netif_create_default_wifi_ap();
        if (g_state.ap_netif == nullptr) {
            FL_WARN_F("wifi: AP netif creation failed");
            return false;
        }
    }

    wifi_config_t cfg;
    fl::memset(&cfg, 0, sizeof(cfg));
    fl::strncpy(reinterpret_cast<char *>(cfg.ap.ssid), ssid, // ok reinterpret cast - IDF field is uint8_t[]
                sizeof(cfg.ap.ssid) - 1);
    cfg.ap.ssid_len = static_cast<u8>(fl::strlen(ssid));
    cfg.ap.channel = (channel >= 1 && channel <= 13) ? channel : 1;
    cfg.ap.max_connection = 4;
    if (password != nullptr && fl::strlen(password) >= 8) {
        fl::strncpy(reinterpret_cast<char *>(cfg.ap.password), password, // ok reinterpret cast - IDF field is uint8_t[]
                    sizeof(cfg.ap.password) - 1);
        cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        cfg.ap.authmode = WIFI_AUTH_OPEN;
    }

    g_state.ap_active = true;
    if (!applyMode()) {
        g_state.ap_active = false;
        return false;
    }
    esp_err_t err = esp_wifi_set_config(WIFI_IF_AP, &cfg);
    if (err != ESP_OK) {
        FL_WARN_F("wifi: AP set_config failed: %s", esp_err_to_name(err));
        g_state.ap_active = false;
        return false;
    }
    err = esp_wifi_start();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        FL_WARN_F("wifi: start failed: %s", esp_err_to_name(err));
        g_state.ap_active = false;
        return false;
    }
    g_state.driver_started = true;
    if (g_state.status == Status::IDLE ||
        g_state.status == Status::UNSUPPORTED) {
        g_state.status = Status::AP_ACTIVE;
    }
    return true;
}

void stop() FL_NO_EXCEPT {
    g_state.sta_requested = false;
    g_state.ap_active = false;
    if (g_state.driver_started) {
        esp_wifi_stop();
        g_state.driver_started = false;
    }
    g_state.status = Status::IDLE;
}

Status status() FL_NO_EXCEPT {
    return g_state.status;
}

bool isConnected() FL_NO_EXCEPT {
    return g_state.status == Status::CONNECTED;
}

fl::string ipAddress() FL_NO_EXCEPT {
    return netifIp(g_state.sta_netif);
}

fl::string apIpAddress() FL_NO_EXCEPT {
    return netifIp(g_state.ap_netif);
}

} // namespace wifi
} // namespace net
} // namespace fl

#endif // FL_WIFI_AVAILABLE

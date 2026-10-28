// IWYU pragma: private

/// @file platforms/esp/8266/ota/ota_impl.cpp.hpp
/// @brief ESP8266 OTA backend, built on the Arduino core's `ArduinoOTA`.
///
/// ESP8266 OTA is provided by the core's verified `ArduinoOTA.h` (shipped in
/// `libraries/ArduinoOTA`), so this backend links to it rather than hand-rolling
/// the flash/HTTP protocol. It is selected at the preprocessor boundary
/// (`FL_OTA_HAS_PLATFORM_IMPL` in `platforms/ota.cpp.hpp`) and lives in its own
/// component translation unit (`fl/build/platforms.esp.8266.ota+.cpp`, mirroring
/// the ESP32 backend, #4726) so `ArduinoOTA`/WiFi are linked only when a sketch
/// uses `fl::net::OTA` — `IOTA::create()` is the strong reference that pulls it.

#include "fl/stl/noexcept.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP8266)

#include "fl/stl/has_include.h"

// Verified-header gate: only build the backend when the core actually ships
// ArduinoOTA (never guess). WiFi.h ships with the same core.
#if FL_HAS_INCLUDE(<ArduinoOTA.h>) && FL_HAS_INCLUDE(<WiFi.h>)
#define FL_ESP8266_OTA_SUPPORTED

#include "fl/log/log.h"
#include "fl/net/ota/ota.h"  // fl::net::ota::Service
#include "fl/stl/cstring.h"
#include "fl/stl/function.h"
#include "fl/stl/int.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "platforms/ota.h"

// IWYU pragma: begin_keep
#include <ArduinoOTA.h>
#include <WiFi.h>
// IWYU pragma: end_keep

namespace fl {
namespace platforms {

/// ESP8266 OTA via the Arduino core's ArduinoOTA (Arduino-IDE OTA on port 3232).
///
/// The ESP8266 core has no HTTP web-OTA server in FastLED's stack, so
/// `getFailedServices()` reports `HTTP_FAILED` (Web OTA unavailable) while
/// Arduino-IDE OTA, Wi-Fi setup and callbacks are fully functional.
class Esp8266OTA : public IOTA {
public:
    Esp8266OTA() FL_NO_EXCEPT
        : mBeforeReboot(nullptr)
        , mApFallbackEnabled(false)
        , mArduinoOtaStarted(false)
        , mFailedServices(0) {}

    ~Esp8266OTA() override = default;

    bool beginWiFi(const char* hostname, const char* password, const char* ssid,
                   const char* wifi_pass) FL_NO_EXCEPT override {
        mFailedServices = 0;
        mHostname = hostname ? hostname : "";
        mPassword = password ? password : "";

        // ESP8266 station mode; AP+STA when an AP fallback was requested.
        WiFi.mode(mApFallbackEnabled && !mApSsid.empty() ? WIFI_AP_STA : WIFI_STA);
        if (!mHostname.empty()) {
            WiFi.hostname(mHostname.c_str());
        }
        if (ssid) {
            WiFi.begin(ssid, wifi_pass);  // async: returns immediately
        }
        if (mApFallbackEnabled && !mApSsid.empty()) {
            WiFi.softAP(mApSsid.c_str(), mApPass.empty() ? nullptr : mApPass.c_str());
        }

        startArduinoOta();
        // No web OTA on this platform.
        mFailedServices |= static_cast<u8>(fl::net::ota::Service::HTTP_FAILED);
        return true;
    }

    bool begin(const char* hostname, const char* password) FL_NO_EXCEPT override {
        mFailedServices = 0;
        mHostname = hostname ? hostname : "";
        mPassword = password ? password : "";
        if (!mHostname.empty()) {
            WiFi.hostname(mHostname.c_str());
        }
        startArduinoOta();
        mFailedServices |= static_cast<u8>(fl::net::ota::Service::HTTP_FAILED);
        return true;
    }

    bool enableApFallback(const char* ap_ssid, const char* ap_pass) FL_NO_EXCEPT override {
        if (!ap_ssid || !*ap_ssid) {
            return false;
        }
        if (ap_pass && fl::strlen(ap_pass) > 0 && fl::strlen(ap_pass) < 8) {
            return false;  // WPA2 requires >= 8 chars
        }
        mApFallbackEnabled = true;
        mApSsid = ap_ssid;
        mApPass = ap_pass ? ap_pass : "";
        return true;
    }

    void onProgress(fl::function<void(size_t, size_t)> callback) FL_NO_EXCEPT override {
        mProgressCb = fl::move(callback);
    }

    void onError(fl::function<void(const char*)> callback) FL_NO_EXCEPT override {
        mErrorCb = fl::move(callback);
    }

    void onState(fl::function<void(u8)> callback) FL_NO_EXCEPT override {
        mStateCb = fl::move(callback);
    }

    void onBeforeReboot(void (*callback)()) FL_NO_EXCEPT override {
        mBeforeReboot = callback;
    }

    void poll() FL_NO_EXCEPT override {
        if (mArduinoOtaStarted) {
            ArduinoOTA.handle();
        }
    }

    bool isConnected() const FL_NO_EXCEPT override {
        return WiFi.status() == WL_CONNECTED;
    }

    u8 getFailedServices() const FL_NO_EXCEPT override { return mFailedServices; }

private:
    void startArduinoOta() FL_NO_EXCEPT {
        ArduinoOTA.setHostname(mHostname.empty() ? "esp8266" : mHostname.c_str());
        if (!mPassword.empty()) {
            ArduinoOTA.setPassword(mPassword.c_str());
        }
        ArduinoOTA.onStart([this]() {
            if (mStateCb) {
                mStateCb(static_cast<u8>(0));  // started
            }
        });
        ArduinoOTA.onProgress([this](fl::u32 written, fl::u32 total) {
            if (mProgressCb) {
                mProgressCb(static_cast<size_t>(written), static_cast<size_t>(total));
            }
        });
        ArduinoOTA.onEnd([this]() {
            if (mBeforeReboot) {
                mBeforeReboot();
            }
            if (mStateCb) {
                mStateCb(static_cast<u8>(2));  // finished
            }
        });
        ArduinoOTA.onError([this](ota_error_t) {
            if (mErrorCb) {
                mErrorCb("ArduinoOTA update failed");
            }
            if (mStateCb) {
                mStateCb(static_cast<u8>(3));  // error
            }
        });
        ArduinoOTA.begin();
        mArduinoOtaStarted = true;
    }

    fl::string mHostname;
    fl::string mPassword;
    fl::string mApSsid;
    fl::string mApPass;
    fl::function<void(size_t, size_t)> mProgressCb;
    fl::function<void(const char*)> mErrorCb;
    fl::function<void(u8)> mStateCb;
    void (*mBeforeReboot)();
    bool mApFallbackEnabled;
    bool mArduinoOtaStarted;
    u8 mFailedServices;
};

// Strong definitions; `platforms/ota.cpp.hpp` skips its weak/null pair on
// ESP8266 (FL_OTA_HAS_PLATFORM_IMPL == 1), so this TU owns the factory.
fl::shared_ptr<IOTA> platform_create_ota() FL_NO_EXCEPT {
    return fl::make_shared<Esp8266OTA>();
}

fl::shared_ptr<IOTA> IOTA::create() FL_NO_EXCEPT {
    return platform_create_ota();
}

}  // namespace platforms
}  // namespace fl

#endif  // FL_HAS_INCLUDE(<ArduinoOTA.h>) && FL_HAS_INCLUDE(<WiFi.h>)
#endif  // FL_IS_ESP8266

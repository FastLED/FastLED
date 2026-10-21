
/// @file platforms/ota.cpp
/// Platform-agnostic OTA implementation and null/default implementation

#include "platforms/ota.h"
#include "platforms/esp/is_esp.h"
#include "platforms/esp/esp_version.h"
#include "fl/log/log.h"
#include "fl/stl/noexcept.h"

// Skip the null stub and the factory when the real ESP32 OTA implementation
// is compiled (platforms/esp/32/ota/ota_impl.cpp.hpp, which defines both).
// Must match FL_ESP_OTA_SUPPORTED there. The ESP32 backend lives in its own
// translation unit (fl/build/platforms.esp.32.ota+.cpp) so the WiFi stack it
// references links only when a sketch uses fl::net::OTA (FastLED #4726);
// IOTA::create() is the strong reference that pulls that unit in.
#if defined(FL_IS_ESP32) && ESP_IDF_VERSION_4_OR_HIGHER && \
    !ESP_IDF_VERSION_6_OR_HIGHER && \
    !defined(FL_IS_ESP_32H2) && !defined(FL_IS_ESP_32P4)
#define FL_OTA_HAS_PLATFORM_IMPL 1
#else
#define FL_OTA_HAS_PLATFORM_IMPL 0
#endif

namespace fl {
namespace platforms {

// ============================================================================
// Null OTA Implementation (No-op for unsupported platforms)
// ============================================================================

class NullOTA : public IOTA {
public:
    NullOTA() = default;
    ~NullOTA() override = default;

    bool beginWiFi(const char*, const char*, const char*, const char*) FL_NO_EXCEPT override {
        FL_WARN("OTA not supported on this platform");
        return false;  // Not supported
    }

    bool begin(const char*, const char*) FL_NO_EXCEPT override {
        FL_WARN("OTA not supported on this platform");
        return false;  // Not supported
    }

    bool enableApFallback(const char*, const char*) FL_NO_EXCEPT override {
        FL_WARN("OTA not supported on this platform");
        return false;  // Not supported
    }

    void onProgress(fl::function<void(size_t, size_t)>) FL_NO_EXCEPT override {
        FL_WARN("OTA not supported on this platform");
        // No-op
    }

    void onError(fl::function<void(const char*)>) FL_NO_EXCEPT override {
        FL_WARN("OTA not supported on this platform");
        // No-op
    }

    void onState(fl::function<void(u8)>) FL_NO_EXCEPT override {
        FL_WARN("OTA not supported on this platform");
        // No-op
    }

    void onBeforeReboot(void (*callback)()) FL_NO_EXCEPT override {
        FL_WARN("OTA not supported on this platform");
        // No-op
        (void)callback;  // Suppress unused parameter warning
    }

    void poll() FL_NO_EXCEPT override {
        FL_WARN("OTA not supported on this platform");
        // No-op
    }

    bool isConnected() const FL_NO_EXCEPT override {
        FL_WARN("OTA not supported on this platform");
        return false;  // Not supported
    }

    u8 getFailedServices() const FL_NO_EXCEPT override {
        return 0;  // No services on unsupported platforms
    }
};

// ============================================================================
// Default Implementation (for platforms without OTA support)
// ============================================================================

// Guarded out when a platform-specific implementation exists (e.g., ESP32).
// Unity builds place both in the same TU, so we use conditional compilation.
#if !FL_OTA_HAS_PLATFORM_IMPL
fl::shared_ptr<IOTA> platform_create_ota() FL_NO_EXCEPT {
    return fl::make_shared<NullOTA>();
}
#endif

// ============================================================================
// Factory Method Implementation
// ============================================================================

#if !FL_OTA_HAS_PLATFORM_IMPL
fl::shared_ptr<IOTA> IOTA::create() FL_NO_EXCEPT {
    return platform_create_ota();
}
#endif

}  // namespace platforms
}  // namespace fl

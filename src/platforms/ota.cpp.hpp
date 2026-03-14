
/// @file platforms/ota.cpp
/// Platform-agnostic OTA implementation and null/default implementation

#include "platforms/ota.h"
#include "platforms/esp/is_esp.h"
#include "platforms/esp/esp_version.h"
#include "fl/system/log.h"

// Skip null stub when the real ESP32 OTA implementation will be compiled.
// In unity builds both files end up in the same TU, so FL_LINK_WEAK cannot
// resolve the duplicate; we guard the null factory out instead.
#if defined(FL_IS_ESP32) && defined(ESP_IDF_VERSION_4_OR_HIGHER) && \
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

    bool beginWiFi(const char*, const char*, const char*, const char*) override {
        FL_WARN("OTA not supported on this platform");
        return false;  // Not supported
    }

    bool begin(const char*, const char*) override {
        FL_WARN("OTA not supported on this platform");
        return false;  // Not supported
    }

    bool enableApFallback(const char*, const char*) override {
        FL_WARN("OTA not supported on this platform");
        return false;  // Not supported
    }

    void onProgress(fl::function<void(size_t, size_t)>) override {
        FL_WARN("OTA not supported on this platform");
        // No-op
    }

    void onError(fl::function<void(const char*)>) override {
        FL_WARN("OTA not supported on this platform");
        // No-op
    }

    void onState(fl::function<void(u8)>) override {
        FL_WARN("OTA not supported on this platform");
        // No-op
    }

    void onBeforeReboot(void (*callback)()) override {
        FL_WARN("OTA not supported on this platform");
        // No-op
        (void)callback;  // Suppress unused parameter warning
    }

    void poll() override {
        FL_WARN("OTA not supported on this platform");
        // No-op
    }

    bool isConnected() const override {
        FL_WARN("OTA not supported on this platform");
        return false;  // Not supported
    }

    u8 getFailedServices() const override {
        return 0;  // No services on unsupported platforms
    }
};

// ============================================================================
// Default Implementation (for platforms without OTA support)
// ============================================================================

// Guarded out when a platform-specific implementation exists (e.g., ESP32).
// Unity builds place both in the same TU, so we use conditional compilation.
#if !FL_OTA_HAS_PLATFORM_IMPL
fl::shared_ptr<IOTA> platform_create_ota() {
    return fl::make_shared<NullOTA>();
}
#endif

// ============================================================================
// Factory Method Implementation
// ============================================================================

fl::shared_ptr<IOTA> IOTA::create() {
    return platform_create_ota();
}

}  // namespace platforms
}  // namespace fl

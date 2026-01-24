/// @file platforms/ota.cpp
/// Platform-agnostic OTA implementation and null/default implementation

#include "platforms/ota.h"
#include "fl/warn.h"

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

    void onState(fl::function<void(uint8_t)>) override {
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

    uint8_t getFailedServices() const override {
        return 0;  // No services on unsupported platforms
    }
};

// ============================================================================
// Weak Default Implementation
// ============================================================================

// Weak default factory - returns null implementation
// Platform-specific code (e.g., ESP32) will override this with strong linkage
FL_LINK_WEAK
fl::shared_ptr<IOTA> platform_create_ota() {
    return fl::make_shared<NullOTA>();
}

// ============================================================================
// Factory Method Implementation
// ============================================================================

fl::shared_ptr<IOTA> IOTA::create() {
    return platform_create_ota();
}

}  // namespace platforms
}  // namespace fl

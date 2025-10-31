/// @file fl/ota.cpp
/// OTA (Over-The-Air) update implementation - wrapper around platform interface

#include "fl/ota.h"
#include "platforms/ota.h"

namespace fl {

// ============================================================================
// OTA Wrapper Implementation
// ============================================================================

OTA::OTA() : impl_(nullptr) {
    // Lazy initialization - impl_ will be created on first method call
}

OTA::~OTA() {
    // shared_ptr handles cleanup automatically
}

bool OTA::beginWiFi(const char* hostname, const char* password,
                    const char* ssid, const char* wifi_pass) {
    if (!impl_) {
        impl_ = platforms::IOTA::create();
    }
    return impl_->beginWiFi(hostname, password, ssid, wifi_pass);
}

bool OTA::beginEthernet(const char* hostname, const char* password) {
    if (!impl_) {
        impl_ = platforms::IOTA::create();
    }
    return impl_->beginEthernet(hostname, password);
}

bool OTA::begin(const char* hostname, const char* password) {
    if (!impl_) {
        impl_ = platforms::IOTA::create();
    }
    return impl_->begin(hostname, password);
}

void OTA::enableApFallback(const char* ap_ssid, const char* ap_pass) {
    if (!impl_) {
        impl_ = platforms::IOTA::create();
    }
    impl_->enableApFallback(ap_ssid, ap_pass);
}

void OTA::onProgress(ProgressCallback callback) {
    if (!impl_) {
        impl_ = platforms::IOTA::create();
    }
    impl_->onProgress(callback);
}

void OTA::onError(ErrorCallback callback) {
    if (!impl_) {
        impl_ = platforms::IOTA::create();
    }
    impl_->onError(callback);
}

void OTA::onState(StateCallback callback) {
    if (!impl_) {
        impl_ = platforms::IOTA::create();
    }
    impl_->onState(callback);
}

void OTA::poll() {
    if (!impl_) {
        impl_ = platforms::IOTA::create();
    }
    impl_->poll();
}

}  // namespace fl

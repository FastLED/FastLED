/// @file fl/ota.cpp
/// OTA (Over-The-Air) update implementation - wrapper around platform interface

#include "fl/ota.h"
#include "platforms/ota.h"

namespace fl {

// ============================================================================
// OTA Wrapper Implementation
// ============================================================================

OTA::OTA() : mImpl(nullptr) {
    // Lazy initialization - mImpl will be created on first method call
}

OTA::~OTA() {
    // shared_ptr handles cleanup automatically
}

bool OTA::beginWiFi(const char* hostname, const char* password,
                    const char* ssid, const char* wifi_pass) {
    if (!mImpl) {
        mImpl = platforms::IOTA::create();
    }
    return mImpl->beginWiFi(hostname, password, ssid, wifi_pass);
}

bool OTA::begin(const char* hostname, const char* password) {
    if (!mImpl) {
        mImpl = platforms::IOTA::create();
    }
    return mImpl->begin(hostname, password);
}

bool OTA::enableApFallback(const char* ap_ssid, const char* ap_pass) {
    if (!mImpl) {
        mImpl = platforms::IOTA::create();
    }
    return mImpl->enableApFallback(ap_ssid, ap_pass);
}

void OTA::onProgress(ProgressCallback callback) {
    if (!mImpl) {
        mImpl = platforms::IOTA::create();
    }
    mImpl->onProgress(callback);
}

void OTA::onError(ErrorCallback callback) {
    if (!mImpl) {
        mImpl = platforms::IOTA::create();
    }
    mImpl->onError(callback);
}

void OTA::onState(StateCallback callback) {
    if (!mImpl) {
        mImpl = platforms::IOTA::create();
    }
    mImpl->onState(callback);
}

void OTA::onBeforeReboot(void (*callback)()) {
    if (!mImpl) {
        mImpl = platforms::IOTA::create();
    }
    mImpl->onBeforeReboot(callback);
}

void OTA::poll() {
    if (!mImpl) {
        mImpl = platforms::IOTA::create();
    }
    mImpl->poll();
}

bool OTA::isConnected() const {
    if (!mImpl) {
        return false;
    }
    return mImpl->isConnected();
}

uint8_t OTA::getFailedServices() const {
    if (!mImpl) {
        return 0;
    }
    return mImpl->getFailedServices();
}

}  // namespace fl

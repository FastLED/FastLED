/// @file ota_stub.h
/// @brief Stub implementation of OTA interface for testing
///
/// This provides a testable implementation of the OTA interface that doesn't
/// require actual network hardware or services. All operations are simulated
/// and can be controlled/inspected for testing purposes.

#ifndef FL_OTA_STUB_H
#define FL_OTA_STUB_H

#include "fl/ota.h"
#include "fl/vector.h"

namespace fl {

/// @brief Stub OTA implementation for testing
///
/// This implementation simulates OTA functionality without requiring actual
/// network hardware. It tracks all state changes, callbacks, and operations
/// for test validation.
class OTAStub : public OTA {
public:
    OTAStub()
        : state_(OTAState::IDLE)
        , transport_(OTATransport::NONE)
        , web_enabled_(true)
        , ide_enabled_(true)
        , mdns_enabled_(true)
        , ap_enabled_(false)
        , running_(false)
        , poll_count_(0) {}

    virtual ~OTAStub() {}

    // === Transport initialization ===

    bool beginWiFi(
        const char* hostname,
        const char* ota_password,
        const char* ssid,
        const char* psk,
        uint32_t timeout_ms = 12000
    ) override {
        if (!hostname || !ota_password || !ssid || !psk) {
            notifyError_("Invalid parameters");
            return false;
        }

        setHostname_(hostname);
        setPassword_(ota_password);
        wifi_ssid_ = ssid;
        wifi_psk_ = psk;
        wifi_timeout_ms_ = timeout_ms;
        transport_ = OTATransport::WIFI;
        running_ = true;

        // Simulate successful connection
        ip_address_ = "192.168.1.100";
        
        return true;
    }

    bool beginEthernet(
        const char* hostname,
        const char* ota_password,
        uint32_t timeout_ms = 12000
    ) override {
        if (!hostname || !ota_password) {
            notifyError_("Invalid parameters");
            return false;
        }

        setHostname_(hostname);
        setPassword_(ota_password);
        eth_timeout_ms_ = timeout_ms;
        transport_ = OTATransport::ETHERNET;
        running_ = true;

        // Simulate successful connection
        ip_address_ = "192.168.1.101";
        
        return true;
    }

    bool beginNetworkOnly(
        const char* hostname,
        const char* ota_password
    ) override {
        if (!hostname || !ota_password) {
            notifyError_("Invalid parameters");
            return false;
        }

        setHostname_(hostname);
        setPassword_(ota_password);
        transport_ = OTATransport::CUSTOM;
        running_ = true;

        // Simulate existing network
        ip_address_ = "192.168.1.102";
        
        return true;
    }

    // === Runtime control ===

    void poll() override {
        poll_count_++;
        // Stub implementation - no actual polling needed
    }

    // === Feature toggles ===

    void enableApFallback(const char* ssid, const char* pass = nullptr) override {
        ap_enabled_ = true;
        if (ssid) {
            ap_ssid_ = ssid;
        }
        if (pass) {
            ap_pass_ = pass;
        }
    }

    void disableWeb() override {
        web_enabled_ = false;
    }

    void disableArduinoIDE() override {
        ide_enabled_ = false;
    }

    void disableMDNS() override {
        mdns_enabled_ = false;
    }

    // === Callbacks ===

    void onProgress(fl::function<void(size_t, size_t)> callback) override {
        progress_callback_ = callback;
    }

    void onError(fl::function<void(const char*)> callback) override {
        error_callback_ = callback;
    }

    void onState(fl::function<void(OTAState)> callback) override {
        state_callback_ = callback;
    }

    // === Status queries ===

    OTAState getState() const override {
        return state_;
    }

    OTATransport getTransport() const override {
        return transport_;
    }

    const char* getHostname() const override {
        return hostname_.c_str();
    }

    fl::optional<fl::string> getIpAddress() const override {
        if (ip_address_.empty()) {
            return fl::nullopt;
        }
        return fl::make_optional(ip_address_);
    }

    bool isRunning() const override {
        return running_;
    }

    // === Test helper methods ===

    /// @brief Simulate OTA update start
    void simulateUpdateStart() {
        setState_(OTAState::STARTING);
    }

    /// @brief Simulate OTA update progress
    /// @param written Bytes written so far
    /// @param total Total bytes to write
    void simulateUpdateProgress(size_t written, size_t total) {
        setState_(OTAState::IN_PROGRESS);
        if (progress_callback_) {
            progress_callback_(written, total);
        }
    }

    /// @brief Simulate OTA update success
    void simulateUpdateSuccess() {
        setState_(OTAState::SUCCESS);
    }

    /// @brief Simulate OTA update failure
    /// @param error_msg Error message
    void simulateUpdateError(const char* error_msg) {
        setState_(OTAState::ERROR);
        notifyError_(error_msg);
    }

    /// @brief Get number of times poll() was called
    size_t getPollCount() const {
        return poll_count_;
    }

    /// @brief Check if Web UI is enabled
    bool isWebEnabled() const {
        return web_enabled_;
    }

    /// @brief Check if Arduino IDE OTA is enabled
    bool isArduinoIDEEnabled() const {
        return ide_enabled_;
    }

    /// @brief Check if mDNS is enabled
    bool isMDNSEnabled() const {
        return mdns_enabled_;
    }

    /// @brief Check if AP fallback is enabled
    bool isApFallbackEnabled() const {
        return ap_enabled_;
    }

    /// @brief Get Wi-Fi SSID (for testing)
    const fl::string& getWiFiSSID() const {
        return wifi_ssid_;
    }

    /// @brief Get Wi-Fi password (for testing)
    const fl::string& getWiFiPassword() const {
        return wifi_psk_;
    }

    /// @brief Get AP SSID (for testing)
    const fl::string& getApSSID() const {
        return ap_ssid_;
    }

    /// @brief Reset stub state
    void reset() {
        state_ = OTAState::IDLE;
        transport_ = OTATransport::NONE;
        web_enabled_ = true;
        ide_enabled_ = true;
        mdns_enabled_ = true;
        ap_enabled_ = false;
        running_ = false;
        poll_count_ = 0;
        hostname_.clear();
        ota_password_.clear();
        wifi_ssid_.clear();
        wifi_psk_.clear();
        ap_ssid_.clear();
        ap_pass_.clear();
        ip_address_.clear();
        progress_callback_ = fl::function<void(size_t, size_t)>();
        error_callback_ = fl::function<void(const char*)>();
        state_callback_ = fl::function<void(OTAState)>();
    }

private:
    void setHostname_(const char* hostname) {
        if (hostname) {
            hostname_ = hostname;
        } else {
            hostname_ = "esp-fastled";
        }
    }

    void setPassword_(const char* password) {
        if (password) {
            ota_password_ = password;
        } else {
            ota_password_.clear();
        }
    }

    void setState_(OTAState new_state) {
        state_ = new_state;
        if (state_callback_) {
            state_callback_(new_state);
        }
    }

    void notifyError_(const char* error_msg) {
        if (error_callback_ && error_msg) {
            error_callback_(error_msg);
        }
    }

    // State
    OTAState state_;
    OTATransport transport_;
    bool web_enabled_;
    bool ide_enabled_;
    bool mdns_enabled_;
    bool ap_enabled_;
    bool running_;
    size_t poll_count_;

    // Configuration
    fl::string hostname_;
    fl::string ota_password_;
    fl::string wifi_ssid_;
    fl::string wifi_psk_;
    uint32_t wifi_timeout_ms_;
    uint32_t eth_timeout_ms_;
    fl::string ap_ssid_;
    fl::string ap_pass_;
    fl::string ip_address_;

    // Callbacks
    fl::function<void(size_t, size_t)> progress_callback_;
    fl::function<void(const char*)> error_callback_;
    fl::function<void(OTAState)> state_callback_;
};

}  // namespace fl

#endif // FL_OTA_STUB_H

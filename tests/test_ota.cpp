// Unit tests for OTA functionality using stub implementation

#include "test.h"
#include "FastLED.h"
#include "platforms/stub/ota_stub.h"

using namespace fl;

TEST_CASE("OTA - Basic WiFi initialization") {
    OTAStub ota;
    
    bool result = ota.beginWiFi(
        "test-device",
        "test-password",
        "TestSSID",
        "TestPass"
    );
    
    CHECK(result);
    CHECK(ota.isRunning());
    CHECK(ota.getTransport() == OTATransport::WIFI);
    CHECK(fl::string(ota.getHostname()) == "test-device");
    
    // Verify Wi-Fi configuration was stored
    CHECK(ota.getWiFiSSID() == "TestSSID");
    CHECK(ota.getWiFiPassword() == "TestPass");
    
    // Verify IP address was assigned
    auto ip = ota.getIpAddress();
    CHECK(ip.has_value());
    CHECK(!(*ip).empty());
}

TEST_CASE("OTA - Basic Ethernet initialization") {
    OTAStub ota;
    
    bool result = ota.beginEthernet(
        "test-device-eth",
        "test-password"
    );
    
    CHECK(result);
    CHECK(ota.isRunning());
    CHECK(ota.getTransport() == OTATransport::ETHERNET);
    CHECK(fl::string(ota.getHostname()) == "test-device-eth");
    
    // Verify IP address was assigned
    auto ip = ota.getIpAddress();
    CHECK(ip.has_value());
}

TEST_CASE("OTA - Network only initialization") {
    OTAStub ota;
    
    bool result = ota.beginNetworkOnly(
        "test-device-net",
        "test-password"
    );
    
    CHECK(result);
    CHECK(ota.isRunning());
    CHECK(ota.getTransport() == OTATransport::CUSTOM);
    CHECK(fl::string(ota.getHostname()) == "test-device-net");
}

TEST_CASE("OTA - Invalid parameters") {
    OTAStub ota;
    
    // Test nullptr hostname
    bool result1 = ota.beginWiFi(
        nullptr,
        "password",
        "SSID",
        "pass"
    );
    CHECK(!result1);
    
    // Test nullptr password
    bool result2 = ota.beginWiFi(
        "hostname",
        nullptr,
        "SSID",
        "pass"
    );
    CHECK(!result2);
    
    // Test nullptr SSID
    bool result3 = ota.beginWiFi(
        "hostname",
        "password",
        nullptr,
        "pass"
    );
    CHECK(!result3);
}

TEST_CASE("OTA - Feature toggles") {
    OTAStub ota;
    
    // Initially all features should be enabled
    CHECK(ota.isWebEnabled());
    CHECK(ota.isArduinoIDEEnabled());
    CHECK(ota.isMDNSEnabled());
    CHECK(!(ota.isApFallbackEnabled()));
    
    // Disable features
    ota.disableWeb();
    CHECK(!(ota.isWebEnabled()));
    
    ota.disableArduinoIDE();
    CHECK(!(ota.isArduinoIDEEnabled()));
    
    ota.disableMDNS();
    CHECK(!(ota.isMDNSEnabled()));
    
    // Enable AP fallback
    ota.enableApFallback("AP-SSID", "AP-Pass");
    CHECK(ota.isApFallbackEnabled());
    CHECK(fl::string(ota.getApSSID().c_str()) == "AP-SSID");
}

TEST_CASE("OTA - Progress callback") {
    OTAStub ota;
    
    size_t last_written = 0;
    size_t last_total = 0;
    int callback_count = 0;
    
    ota.onProgress([&](size_t written, size_t total) {
        last_written = written;
        last_total = total;
        callback_count++;
    });
    
    // Simulate progress updates
    ota.simulateUpdateProgress(100, 1000);
    CHECK(last_written == 100);
    CHECK(last_total == 1000);
    CHECK(callback_count == 1);
    
    ota.simulateUpdateProgress(500, 1000);
    CHECK(last_written == 500);
    CHECK(last_total == 1000);
    CHECK(callback_count == 2);
    
    ota.simulateUpdateProgress(1000, 1000);
    CHECK(last_written == 1000);
    CHECK(last_total == 1000);
    CHECK(callback_count == 3);
}

TEST_CASE("OTA - Error callback") {
    OTAStub ota;
    
    fl::string last_error;
    int callback_count = 0;
    
    ota.onError([&](const char* error_msg) {
        if (error_msg) {
            last_error = error_msg;
        }
        callback_count++;
    });
    
    // Simulate error
    ota.simulateUpdateError("Test error message");
    CHECK(fl::string(last_error.c_str()) == "Test error message");
    CHECK(callback_count == 1);
    CHECK(ota.getState() == OTAState::ERROR);
}

TEST_CASE("OTA - State callback") {
    OTAStub ota;
    
    OTAState last_state = OTAState::IDLE;
    int callback_count = 0;
    
    ota.onState([&](OTAState state) {
        last_state = state;
        callback_count++;
    });
    
    // Simulate state transitions
    ota.simulateUpdateStart();
    CHECK(last_state == OTAState::STARTING);
    CHECK(callback_count == 1);
    
    ota.simulateUpdateProgress(100, 1000);
    CHECK(last_state == OTAState::IN_PROGRESS);
    CHECK(callback_count == 2);
    
    ota.simulateUpdateSuccess();
    CHECK(last_state == OTAState::SUCCESS);
    CHECK(callback_count == 3);
}

TEST_CASE("OTA - Poll tracking") {
    OTAStub ota;
    
    CHECK(ota.getPollCount() == 0);
    
    ota.poll();
    CHECK(ota.getPollCount() == 1);
    
    ota.poll();
    ota.poll();
    CHECK(ota.getPollCount() == 3);
}

TEST_CASE("OTA - State transitions") {
    OTAStub ota;
    
    CHECK(ota.getState() == OTAState::IDLE);
    
    ota.simulateUpdateStart();
    CHECK(ota.getState() == OTAState::STARTING);
    
    ota.simulateUpdateProgress(50, 100);
    CHECK(ota.getState() == OTAState::IN_PROGRESS);
    
    ota.simulateUpdateSuccess();
    CHECK(ota.getState() == OTAState::SUCCESS);
}

TEST_CASE("OTA - Reset functionality") {
    OTAStub ota;
    
    // Set up OTA with full configuration
    ota.beginWiFi("test-host", "test-pass", "TestSSID", "TestPass");
    ota.disableWeb();
    ota.enableApFallback("AP-Test", "AP-Pass");
    ota.poll();
    ota.poll();
    
    // Verify configuration is set
    CHECK(ota.isRunning());
    CHECK(ota.getPollCount() > 0);
    
    // Reset
    ota.reset();
    
    // Verify everything is reset
    CHECK(!(ota.isRunning()));
    CHECK(ota.getPollCount() == 0);
    CHECK(ota.getState() == OTAState::IDLE);
    CHECK(ota.getTransport() == OTATransport::NONE);
    CHECK(ota.isWebEnabled());
    CHECK(!(ota.isApFallbackEnabled()));
}

TEST_CASE("OTA - Multiple callback invocations") {
    OTAStub ota;
    
    int progress_count = 0;
    int error_count = 0;
    int state_count = 0;
    
    ota.onProgress([&](size_t, size_t) { progress_count++; });
    ota.onError([&](const char*) { error_count++; });
    ota.onState([&](OTAState) { state_count++; });
    
    // Simulate full update cycle
    ota.simulateUpdateStart();
    CHECK(state_count == 1);
    
    ota.simulateUpdateProgress(25, 100);
    CHECK(progress_count == 1);
    CHECK(state_count == 2);
    
    ota.simulateUpdateProgress(50, 100);
    CHECK(progress_count == 2);
    
    ota.simulateUpdateProgress(75, 100);
    CHECK(progress_count == 3);
    
    ota.simulateUpdateSuccess();
    CHECK(state_count == 5);
    
    // No errors should have occurred
    CHECK(error_count == 0);
}

TEST_CASE("OTA - AP fallback configuration") {
    OTAStub ota;
    
    // Enable AP without password
    ota.enableApFallback("OpenAP", nullptr);
    CHECK(ota.isApFallbackEnabled());
    CHECK(fl::string(ota.getApSSID().c_str()) == "OpenAP");
    
    // Reset and test with password
    ota.reset();
    ota.enableApFallback("SecureAP", "SecurePass");
    CHECK(ota.isApFallbackEnabled());
    CHECK(fl::string(ota.getApSSID().c_str()) == "SecureAP");
}

TEST_CASE("OTA - IP address assignment") {
    OTAStub ota;
    
    // Before initialization, no IP should be available
    auto ip1 = ota.getIpAddress();
    CHECK(!(ip1.has_value()));
    
    // After WiFi init, IP should be assigned
    ota.beginWiFi("test", "pass", "SSID", "Pass");
    auto ip2 = ota.getIpAddress();
    CHECK(ip2.has_value());
    CHECK(!(*ip2).empty());
    
    // After reset, IP should be cleared
    ota.reset();
    auto ip3 = ota.getIpAddress();
    CHECK(!(ip3.has_value()));
}

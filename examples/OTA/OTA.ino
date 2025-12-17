/// @file OTA.ino
/// @brief Minimal OTA (Over-The-Air) update example for ESP32
/// @filter: (platform is esp32)
///
/// This example demonstrates the FastLED OTA functionality for ESP32 devices.
/// It provides both Arduino IDE OTA and web-based firmware updates.
///
///
/// PLATFORMIO REQUIREMENTS:
/// Add these lines to your platformio.ini:
///
/// [env:esp32]
/// platform = espressif32
/// board = esp32dev
/// framework = arduino
/// lib_deps =
///     fastled/FastLED
/// upload_protocol = esptool  ; or 'espota' for OTA uploads
/// upload_port = <device>.local  ; replace <device> with your hostname for OTA uploads
/// monitor_speed = 115200
///
/// FEATURES:
/// - One-line setup for Wi-Fi (async mode)
/// - Arduino IDE OTA support (port 3232)
/// - Web-based OTA at http://<hostname>.local/
/// - mDNS hostname registration
/// - Progress/error callbacks
/// - Low overhead: ~73µs per poll() when idle
/// - Non-blocking Wi-Fi connection with visual feedback
///
/// USAGE:
/// 1. Configure Wi-Fi credentials below
/// 2. Upload via USB initially
/// 3. Access web interface at http://my-device.local/
/// 4. Or use Arduino IDE OTA (Tools > Port > <hostname>)
///
/// SECURITY:
/// - Web UI uses Basic Auth (username: "admin", password: OTA_PASS)
/// - Arduino IDE OTA uses MD5 hashed password
/// - Use only on trusted networks or configure HTTPS
///
/// TROUBLESHOOTING:
///
/// 1. Cannot find device at <hostname>.local
///    - Check that device is on same network as your computer
///    - Some routers block mDNS/Bonjour - try device IP address instead
///    - Windows: Install Bonjour service (comes with iTunes or Apple apps)
///    - Linux: Install avahi-daemon for mDNS support
///    - Alternative: Check serial monitor for IP address and use that
///
/// 2. Arduino IDE cannot see OTA port
///    - Wait ~30 seconds after device boots for mDNS registration
///    - Verify firewall allows UDP port 3232 and mDNS port 5353
///    - Check Arduino IDE > Tools > Port menu - device may show as network port
///    - Try restarting Arduino IDE to refresh port list
///
/// 3. Web interface shows "401 Unauthorized"
///    - Username is "admin", password is value of OTA_PASS
///    - Browser may cache credentials - try private/incognito window
///    - Clear browser auth cache or try different browser
///
/// 4. OTA upload fails or times out
///    - Verify firmware file is valid .bin for ESP32
///    - Check that device has enough free flash space
///    - Ensure stable power supply (USB power may brownout during flash)
///    - Try smaller firmware or disable debug symbols to reduce size
///    - Check serial monitor for error messages during upload
///
/// 5. Wi-Fi connection fails
///    - Verify SSID and password are correct (case-sensitive)
///    - Check Wi-Fi band: ESP32 only supports 2.4GHz (not 5GHz)
///    - Some ESP32 boards have external antenna - verify antenna connected
///    - Move device closer to router to test signal strength
///    - Check serial monitor for Wi-Fi error codes
///
/// 6. Device reboots during OTA update
///    - Power supply issue - use quality USB cable and power source
///    - Add capacitor (100-1000µF) between VIN/3.3V and GND
///    - Reduce LED count or brightness during OTA to lower current draw
///
/// 7. ESP32-C3 compilation fails with "assembler not found"
///    - PlatformIO RISC-V toolchain corruption (not OTA code issue)
///    - Fix: pio system prune && pio pkg install --platform espressif32
///    - Alternative: Delete ~/.platformio/packages/toolchain-riscv32-esp
///    - See: https://github.com/platformio/platform-espressif32/issues/1224
///
/// 8. Performance: LED animations stutter or slow
///    - poll() overhead is <0.5% at 60 FPS (~73µs per call)
///    - If stuttering occurs, reduce FastLED.show() call frequency
///    - Web server runs in separate task (zero main loop impact)
///    - Consider moving LED code to separate FreeRTOS task for isolation
///
/// 9. Network setup guidance
///    - ESP32 variants (ESP32, S3, S2, C3, C6): All support Wi-Fi
///    - Ethernet users: Initialize ETH manually, then call ota.begin()
///    - Example: ETH.begin(/* PHY config */); while(!ETH.linkUp()) delay(100); ota.begin(...);

#include <Arduino.h>
#include <FastLED.h>
#include "fl/ota.h"
#include "fl/stl/sstream.h"

// ====== Configure your transport here ======
#define USE_WIFI         1

// ====== Your identifiers ======
static const char* HOSTNAME   = "my-device";
static const char* OTA_PASS   = "supersecret";     // used for both ArduinoIDE OTA (hashed) and Web UI Basic Auth

// ====== Wi-Fi creds (only used if USE_WIFI=1 and you call beginWiFi) ======
static const char* WIFI_SSID  = "MySSID";
static const char* WIFI_PASS  = "MyPass";

// ====== LED Configuration ======
#define NUM_LEDS 60
#define DATA_PIN 2
CRGB leds[NUM_LEDS];

fl::OTA ota;

void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("FastLED OTA Example");
  Serial.println("===================");

  // Initialize FastLED
  FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(50);

  // Optional: Enable AP fallback when Wi-Fi STA fails (must be called before beginWiFi)
  // ota.enableApFallback("MyDevice-Setup", "setup-only");

#if USE_WIFI
  // Start Wi-Fi (async mode - returns immediately)
  Serial.println("Starting Wi-Fi (async mode)...");
  ota.beginWiFi(HOSTNAME, OTA_PASS, WIFI_SSID, WIFI_PASS);
  Serial.println("Wi-Fi connecting in background...");
  Serial.println("LEDs will show connection status:");
  Serial.println("  - Pulsing blue: Connecting...");
  Serial.println("  - Solid green: Connected!");

  // Check for service initialization failures
  uint8_t failed = ota.getFailedServices();
  if (failed != 0) {
    Serial.println("\nWARNING: Some OTA services failed to initialize:");
    if (failed & (uint8_t)fl::OTAService::MDNS_FAILED) {
      Serial.println("  - mDNS failed: Device won't be discoverable at hostname.local");
      Serial.println("    Try accessing via IP address instead");
    }
    if (failed & (uint8_t)fl::OTAService::HTTP_FAILED) {
      Serial.println("  - HTTP server failed: Web OTA unavailable");
      Serial.println("    You can still use Arduino IDE OTA (port 3232)");
    }
    if (failed & (uint8_t)fl::OTAService::ARDUINO_OTA_FAILED) {
      Serial.println("  - ArduinoOTA failed: IDE OTA unavailable");
      Serial.println("    You can still use Web OTA interface");
    }
  }
#else
  // If your networking is already up (Wi-Fi or ETH), just start OTA services:
  // For Ethernet users: Call ETH.begin() first, then ota.begin()
  Serial.println("Starting OTA services (network assumed configured)...");
  if (ota.begin(HOSTNAME, OTA_PASS)) {
    Serial.println("OTA services started successfully");
  } else {
    Serial.println("OTA services failed to start");
  }

  // Check for service initialization failures
  uint8_t failed = ota.getFailedServices();
  if (failed != 0) {
    Serial.println("\nWARNING: Some OTA services failed to initialize:");
    if (failed & (uint8_t)fl::OTAService::MDNS_FAILED) {
      Serial.println("  - mDNS failed: Device won't be discoverable at hostname.local");
      Serial.println("    Try accessing via IP address instead");
    }
    if (failed & (uint8_t)fl::OTAService::HTTP_FAILED) {
      Serial.println("  - HTTP server failed: Web OTA unavailable");
      Serial.println("    You can still use Arduino IDE OTA (port 3232)");
    }
    if (failed & (uint8_t)fl::OTAService::ARDUINO_OTA_FAILED) {
      Serial.println("  - ArduinoOTA failed: IDE OTA unavailable");
      Serial.println("    You can still use Web OTA interface");
    }
  }
#endif

  // Optional callbacks for monitoring OTA progress:
  ota.onProgress([](size_t written, size_t total){
    //Serial.printf("OTA Progress: %u/%u (%d%%)\n", (unsigned)written, (unsigned)total, (int)((written * 100) / total));
    fl::sstream msg;
    msg << "OTA Progress: " << written << "/" << total << " (" << (int)((written * 100) / total) << "%)";
    Serial.println(msg.c_str());
  });
  ota.onError([](const char* error_msg){
    // Serial.printf("OTA error: %s\n", error_msg);
    fl::sstream msg;
    msg << "OTA error: " << error_msg;
    Serial.println(msg.c_str());
  });
  ota.onState([](uint8_t state){
    //Serial.printf("OTA state: %u\n", state);
    fl::sstream msg;
    msg << "OTA state: " << (int)state;
    Serial.println(msg.c_str());
  });

  // Callback invoked before device reboots after successful OTA update
  // Use this to save state, turn off LEDs, or perform cleanup
  ota.onBeforeReboot([](){
    Serial.println("OTA complete! Preparing to reboot...");
    Serial.println("Saving state and turning off LEDs...");

    // Turn off all LEDs before reboot
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();

    // In a real application, you might:
    // - Save LED configuration to EEPROM/preferences
    // - Save animation state
    // - Close file handles
    // - Notify other systems of impending reboot
  });

  Serial.println("Setup complete. Running LED animation...");
}

uint8_t hue = 0;
bool wifi_connected_notified = false;

void loop() {
  // Service OTA (low overhead: ~73µs when idle)
  ota.poll();

#if USE_WIFI
  // Async WiFi mode: Show connection status via LEDs
  if (!ota.isConnected()) {
    // Connecting: Show pulsing blue animation
    static uint8_t pulse = 0;
    uint8_t brightness = beatsin8(60, 30, 255);  // Pulse at 60 BPM
    fill_solid(leds, NUM_LEDS, CRGB::Blue);
    FastLED.setBrightness(brightness);
    FastLED.show();
    pulse++;
    delay(20);
    return;  // Skip normal animation while connecting
  } else if (!wifi_connected_notified) {
    // Just connected: Show success and print info
    wifi_connected_notified = true;
    Serial.println("\nWi-Fi connected!");
    Serial.print("Access web interface at: http://");
    Serial.print(HOSTNAME);
    Serial.println(".local/");

    // Brief celebration: Green flash
    fill_solid(leds, NUM_LEDS, CRGB::Green);
    FastLED.setBrightness(255);
    FastLED.show();
    delay(500);

    // Restore normal brightness for animation
    FastLED.setBrightness(50);
  }
#endif

  // Normal LED animation (rainbow)
  fill_rainbow(leds, NUM_LEDS, hue, 7);
  FastLED.show();

  hue++;

  // Small delay for animation speed
  delay(20);
}

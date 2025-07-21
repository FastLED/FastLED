/// @file    NetTest.ino
/// @brief   Simple networking test - prints status in loop
/// @example NetTest.ino
///
/// This sketch demonstrates basic FastLED networking capability by:
/// 1. Checking if networking is available
/// 2. Making simple HTTP request to fastled.io (non-blocking)
/// 3. Printing first 10 characters of response
/// 4. No LEDs or graphics - just serial output
///
/// Requirements:
/// - Platform with networking support (ESP32, native platforms)
/// - Compile with FASTLED_HAS_NETWORKING=1 to enable networking

// Enable FastLED networking functionality
#define FASTLED_HAS_NETWORKING 1

#include <Arduino.h>
#include <FastLED.h>

// Include FastLED networking headers
#include "fl/networking.h"
#include "fl/net/http/client.h"

using namespace fl;

// Simple counter for demonstration
uint32_t loop_count = 0;
uint32_t last_print_time = 0;
uint32_t last_http_test = 0;

// HTTP client and state
fl::shared_ptr<HttpClient> client;
bool http_request_attempted = false;

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial to initialize
    
    Serial.println("=== FastLED NetTest - Simple Version ===");
    Serial.println("DEBUG: Starting setup...");
    
    Serial.println("DEBUG: Checking networking availability...");
    
    // Check networking availability
    if (fl::has_networking()) {
        Serial.println("✓ Networking is available!");
        Serial.println("✓ FastLED compiled with networking support");
        
        Serial.println("DEBUG: Creating HTTP client...");
        // Create simple HTTP client
        client = HttpClient::create_simple_client();
        if (client) {
            Serial.println("✓ HTTP client created");
        } else {
            Serial.println("✗ Failed to create HTTP client");
        }
    } else {
        Serial.println("✗ Networking not available");
        Serial.println("  Compile with -DFASTLED_HAS_NETWORKING=1 to enable");
    }
    
    Serial.println();
    Serial.println("DEBUG: Setup complete, starting main loop...");
    Serial.println("Will attempt HTTP test every 10 seconds");
    Serial.println();
}

void loop() {
    uint32_t now = millis();
    
    // Print status every 2 seconds
    if (now - last_print_time >= 2000) {
        loop_count++;
        
        Serial.print("DEBUG: Loop iteration #");
        Serial.println(loop_count);
        
        // Print simple status
        Serial.print("Loop #");
        Serial.print(loop_count);
        Serial.print(" - Uptime: ");
        Serial.print(now / 1000);
        Serial.print("s");
        
        // Show networking status
        if (fl::has_networking()) {
            Serial.print(" - Networking: Available");
        } else {
            Serial.print(" - Networking: Disabled");
        }
        
        // Show HTTP request status
        if (http_request_attempted) {
            Serial.print(" - HTTP: Attempted");
        }
        
        Serial.println();
        
        last_print_time = now;
    }
    
    // Attempt HTTP test every 10 seconds
    if (client && !http_request_attempted && (now - last_http_test >= 10000)) {
        Serial.println("DEBUG: Starting HTTP test...");
        Serial.println("Making simple HTTP request to fastled.io...");
        
        Serial.println("DEBUG: About to call client->get()...");
        auto future = client->get("http://fastled.io");
        Serial.println("DEBUG: client->get() returned");
        
        if (future.valid()) {
            Serial.println("DEBUG: Future is valid");
            Serial.println("✓ HTTP request initiated successfully");
            http_request_attempted = true;
        } else {
            Serial.println("DEBUG: Future is invalid");
            Serial.println("✗ Invalid HTTP request");
        }
        
        last_http_test = now;
    }
    
    // Small delay to prevent busy-waiting
    delay(50);
}

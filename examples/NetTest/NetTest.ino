/// @file    NetTest.ino
/// @brief   Simple networking test with 2x2 LED matrix graphics
/// @example NetTest.ino
///
/// This sketch demonstrates basic FastLED networking capability with visual feedback:
/// 1. Sets up a 2x2 LED matrix (4 pixels total)
/// 2. Displays colorful patterns and animations
/// 3. Makes HTTP requests to test networking
/// 4. Shows network status via LED colors
///
/// Requirements:
/// - Platform with networking support (ESP32, native platforms)
/// - Compile with FASTLED_HAS_NETWORKING=1 to enable networking
/// - 4 WS2812B LEDs connected to data pin

// Enable FastLED networking functionality
#define FASTLED_HAS_NETWORKING 1

#include <Arduino.h>
#include <FastLED.h>

// Include FastLED networking headers
#include "fl/networking.h"
#include "fl/net/http/client.h"

using namespace fl;

// LED setup for 2x2 matrix
#define DATA_PIN     3
#define NUM_LEDS     4
#define LED_TYPE     WS2812B
#define COLOR_ORDER  GRB
#define BRIGHTNESS   128

// 2x2 Matrix layout:
// [0] [1]
// [2] [3]

CRGB leds[NUM_LEDS];

// Animation variables
uint8_t hue = 0;
uint8_t brightness_wave = 0;
uint32_t last_animation_update = 0;
uint8_t pattern_index = 0;
uint32_t pattern_change_time = 0;

// Networking variables
uint32_t loop_count = 0;
uint32_t last_print_time = 0;
uint32_t last_http_test = 0;
fl::shared_ptr<HttpClient> client;
bool http_request_attempted = false;
bool networking_available = false;

void setup() {
    Serial.begin(115200);
    delay(2000);  // Wait for serial to initialize
    
    Serial.println("=== FastLED NetTest with 2x2 LED Matrix ===");
    Serial.println("DEBUG: Starting setup...");
    
    // Initialize FastLED
    Serial.println("DEBUG: Initializing LEDs...");
    FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear();
    FastLED.show();
    
    // Startup animation - flash each corner
    Serial.println("DEBUG: Running startup animation...");
    for(int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Blue;
        FastLED.show();
        delay(200);
        leds[i] = CRGB::Black;
        FastLED.show();
        delay(100);
    }
    
    Serial.println("DEBUG: Checking networking availability...");
    
    // Check networking availability
    if (fl::has_networking()) {
        Serial.println("✓ Networking is available!");
        Serial.println("✓ FastLED compiled with networking support");
        networking_available = true;
        
        // Show green pattern for networking available
        fill_solid(leds, NUM_LEDS, CRGB::Green);
        FastLED.show();
        delay(500);
        
        Serial.println("DEBUG: Creating HTTP client...");
        client = HttpClient::create_simple_client();
        if (client) {
            Serial.println("✓ HTTP client created");
            // Bright green flash for success
            fill_solid(leds, NUM_LEDS, CRGB::Lime);
            FastLED.show();
            delay(300);
        } else {
            Serial.println("✗ Failed to create HTTP client");
            // Red flash for error
            fill_solid(leds, NUM_LEDS, CRGB::Red);
            FastLED.show();
            delay(500);
        }
    } else {
        Serial.println("✗ Networking not available");
        Serial.println("  Compile with -DFASTLED_HAS_NETWORKING=1 to enable");
        networking_available = false;
        
        // Show red pattern for no networking
        fill_solid(leds, NUM_LEDS, CRGB::Red);
        FastLED.show();
        delay(1000);
    }
    
    FastLED.clear();
    FastLED.show();
    
    Serial.println();
    Serial.println("DEBUG: Setup complete, starting main loop...");
    Serial.println("2x2 LED matrix patterns will cycle every 5 seconds");
    if (networking_available) {
        Serial.println("Will attempt HTTP test every 15 seconds");
    }
    Serial.println();
}

void updateLEDPattern() {
    uint32_t now = millis();
    
    // Change pattern every 5 seconds
    if (now - pattern_change_time >= 5000) {
        pattern_index = (pattern_index + 1) % 6;  // 6 different patterns
        pattern_change_time = now;
        Serial.print("Switching to pattern #");
        Serial.println(pattern_index);
    }
    
    // Update animation every 50ms
    if (now - last_animation_update >= 50) {
        hue += 2;
        brightness_wave += 4;
        
        switch(pattern_index) {
            case 0: // Rotating colors
                leds[0] = CHSV(hue, 255, 255);
                leds[1] = CHSV(hue + 64, 255, 255);
                leds[2] = CHSV(hue + 128, 255, 255);
                leds[3] = CHSV(hue + 192, 255, 255);
                break;
                
            case 1: // Breathing effect
                {
                    uint8_t brightness = (sin8(brightness_wave) / 2) + 127;
                    fill_solid(leds, NUM_LEDS, CHSV(hue / 2, 200, brightness));
                }
                break;
                
            case 2: // Diagonal sweep
                {
                    uint8_t pos = (hue / 16) % 4;
                    fill_solid(leds, NUM_LEDS, CRGB::Black);
                    leds[pos] = CHSV(hue, 255, 255);
                }
                break;
                
            case 3: // Opposite corners
                leds[0] = CHSV(hue, 255, 255);
                leds[1] = CRGB::Black;
                leds[2] = CRGB::Black;
                leds[3] = CHSV(hue, 255, 255);
                break;
                
            case 4: // Side to side
                if ((hue / 32) % 2 == 0) {
                    leds[0] = CHSV(hue, 255, 200);
                    leds[2] = CHSV(hue, 255, 200);
                    leds[1] = CRGB::Black;
                    leds[3] = CRGB::Black;
                } else {
                    leds[1] = CHSV(hue + 128, 255, 200);
                    leds[3] = CHSV(hue + 128, 255, 200);
                    leds[0] = CRGB::Black;
                    leds[2] = CRGB::Black;
                }
                break;
                
            case 5: // Rainbow gradient
                leds[0] = CHSV(hue, 255, 255);
                leds[1] = CHSV(hue + 85, 255, 255);
                leds[2] = CHSV(hue + 170, 255, 255);
                leds[3] = CHSV(hue + 255, 255, 255);
                break;
        }
        
        // Show network status with subtle overlay
        if (networking_available && http_request_attempted) {
            // Add a bit of white to show successful network activity
            for(int i = 0; i < NUM_LEDS; i++) {
                leds[i] += CRGB(20, 20, 20);  // Add white overlay
            }
        }
        
        FastLED.show();
        last_animation_update = now;
    }
}

void loop() {
    uint32_t now = millis();
    
    // Update LED patterns
    updateLEDPattern();
    
    // Print status every 3 seconds
    if (now - last_print_time >= 3000) {
        loop_count++;
        
        Serial.print("Loop #");
        Serial.print(loop_count);
        Serial.print(" - Uptime: ");
        Serial.print(now / 1000);
        Serial.print("s - Pattern: ");
        Serial.print(pattern_index);
        
        // Show networking status
        if (networking_available) {
            Serial.print(" - Network: OK");
            if (http_request_attempted) {
                Serial.print(" (HTTP tested)");
            }
        } else {
            Serial.print(" - Network: Disabled");
        }
        
        Serial.println();
        last_print_time = now;
    }
    
    // Attempt HTTP test every 15 seconds (if networking available)
    if (client && !http_request_attempted && (now - last_http_test >= 1000)) {
        Serial.println("Starting HTTP test (watching for LED brightness change)...");
        
        // Flash all LEDs white to indicate network activity
        CRGB original_colors[NUM_LEDS];
        for(int i = 0; i < NUM_LEDS; i++) {
            original_colors[i] = leds[i];
            leds[i] = CRGB::White;
        }
        FastLED.show();
        delay(100);
        
        // Restore colors and continue
        for(int i = 0; i < NUM_LEDS; i++) {
            leds[i] = original_colors[i];
        }
        FastLED.show();
        
        Serial.println("Making HTTP request to fastled.io...");
        auto future = client->get("http://fastled.io");
        
        if (future.valid()) {
            Serial.println("✓ HTTP request initiated successfully");
            http_request_attempted = true;
            
            // Success flash - brief green
            fill_solid(leds, NUM_LEDS, CRGB::Green);
            FastLED.show();
            delay(200);
        } else {
            Serial.println("✗ Invalid HTTP request");
            
            // Error flash - brief red
            fill_solid(leds, NUM_LEDS, CRGB::Red);
            FastLED.show();
            delay(500);
        }
        
        last_http_test = now;
    }
    
    // Small delay to prevent busy-waiting
    delay(20);
}

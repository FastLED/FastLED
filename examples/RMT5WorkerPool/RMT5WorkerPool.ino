/**
 * @file RMT5WorkerPool.ino
 * @brief Test example for RMT5 Worker Pool implementation
 * 
 * This example demonstrates the RMT5 worker pool functionality by creating
 * multiple LED strips that exceed the hardware RMT channel limit, showing
 * how the worker pool manages resource sharing transparently.
 * 
 * Hardware Requirements:
 * - ESP32 (any variant)
 * - Multiple LED strips connected to different pins
 * - For testing: Connect 3+ strips on ESP32-C3 (2 RMT channels) or 5+ strips on ESP32-S3 (4 RMT channels)
 * 
 * Expected Behavior:
 * - All LED strips should display synchronized animations
 * - No visible difference compared to direct RMT usage
 * - Worker pool manages RMT channel sharing automatically
 * - Async behavior preserved when N ≤ K (strips ≤ channels)
 * - Controlled queuing when N > K (strips > channels)
 */

#include <FastLED.h>

// Configuration - adjust for your hardware
#define NUM_LEDS_PER_STRIP 30
#define BRIGHTNESS 50

// Test with more strips than available RMT channels
// ESP32: 8 channels, ESP32-S3: 4 channels, ESP32-C3: 2 channels
#define NUM_STRIPS 6

// LED arrays for multiple strips
CRGB strip1[NUM_LEDS_PER_STRIP];
CRGB strip2[NUM_LEDS_PER_STRIP];
CRGB strip3[NUM_LEDS_PER_STRIP];
CRGB strip4[NUM_LEDS_PER_STRIP];
CRGB strip5[NUM_LEDS_PER_STRIP];
CRGB strip6[NUM_LEDS_PER_STRIP];

// Animation state
uint8_t hue = 0;
unsigned long lastUpdate = 0;
const unsigned long UPDATE_INTERVAL = 50; // 50ms = 20 FPS

// Forward declaration
void updateAnimation();

void setup() {
    Serial.begin(115200);
    Serial.println("RMT5 Worker Pool Test Starting...");
    
    // Initialize LED strips with compile-time constant pins
    // This will test the worker pool with 6 strips exceeding most ESP32 RMT limits
    FastLED.addLeds<WS2812B, 2, GRB>(strip1, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 4, GRB>(strip2, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 5, GRB>(strip3, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 18, GRB>(strip4, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 19, GRB>(strip5, NUM_LEDS_PER_STRIP);
    FastLED.addLeds<WS2812B, 21, GRB>(strip6, NUM_LEDS_PER_STRIP);
    
    FastLED.setBrightness(BRIGHTNESS);
    
    Serial.print("Initialized ");
    Serial.print(NUM_STRIPS);
    Serial.print(" LED strips with ");
    Serial.print(NUM_LEDS_PER_STRIP);
    Serial.println(" LEDs each");
    Serial.print("Total LEDs: ");
    Serial.println(NUM_STRIPS * NUM_LEDS_PER_STRIP);
    
    #if CONFIG_IDF_TARGET_ESP32
        Serial.println("Running on ESP32 (8 RMT channels available)");
    #elif CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
        Serial.println("Running on ESP32-S2/S3 (4 RMT channels available)");
    #elif CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32H2
        Serial.println("Running on ESP32-C3/H2 (2 RMT channels available)");
    #else
        Serial.println("Running on unknown ESP32 variant");
    #endif
    
    if (NUM_STRIPS > 8) {
        Serial.println("⚠️  Testing with more strips than maximum RMT channels - worker pool will queue some strips");
    } else {
        Serial.println("✅ Testing within RMT channel limits - all strips should run async");
    }
    
    // Initial display
    updateAnimation();
    FastLED.show();
    
    Serial.println("Setup complete. Starting animation loop...");
}

void loop() {
    unsigned long now = millis();
    
    if (now - lastUpdate >= UPDATE_INTERVAL) {
        lastUpdate = now;
        
        // Update animation
        updateAnimation();
        
        // Measure timing for worker pool performance
        unsigned long startTime = micros();
        FastLED.show();
        unsigned long endTime = micros();
        
        // Log performance periodically
        static int frameCount = 0;
        frameCount++;
        if (frameCount % 100 == 0) {
            Serial.print("Frame ");
            Serial.print(frameCount);
            Serial.print(": FastLED.show() took ");
            Serial.print(endTime - startTime);
            Serial.println(" µs");
        }
        
        // Advance animation
        hue++;
    }
    
    // Small delay to prevent busy-waiting
    delay(1);
}

void updateAnimation() {
    // Rainbow wave animation across all strips
    CRGB* stripArrays[] = {strip1, strip2, strip3, strip4, strip5, strip6};
    
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        for (int led = 0; led < NUM_LEDS_PER_STRIP; led++) {
            // Create a rainbow wave that moves across strips and LEDs
            uint8_t pixelHue = hue + (strip * 40) + (led * 8);
            stripArrays[strip][led] = CHSV(pixelHue, 255, 255);
        }
    }
}

/**
 * Performance Test Functions
 * These can be called from the Serial monitor for testing
 */

void testWorkerPoolPerformance() {
    Serial.println("=== Worker Pool Performance Test ===");
    
    const int TEST_ITERATIONS = 100;
    unsigned long totalTime = 0;
    
    CRGB* stripArrays[] = {strip1, strip2, strip3, strip4, strip5, strip6};
    
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        // Update all strips with different patterns
        for (int strip = 0; strip < NUM_STRIPS; strip++) {
            fill_rainbow(stripArrays[strip], NUM_LEDS_PER_STRIP, i * 10 + strip * 30, 7);
        }
        
        // Measure show() performance
        unsigned long startTime = micros();
        FastLED.show();
        unsigned long endTime = micros();
        
        totalTime += (endTime - startTime);
        
        delay(10); // Small delay between iterations
    }
    
    unsigned long avgTime = totalTime / TEST_ITERATIONS;
    Serial.print("Average FastLED.show() time over ");
    Serial.print(TEST_ITERATIONS);
    Serial.print(" iterations: ");
    Serial.print(avgTime);
    Serial.println(" µs");
    Serial.print("Effective frame rate: ");
    Serial.print(1000000.0 / avgTime);
    Serial.println(" FPS");
}

void testStripIndependence() {
    Serial.println("=== Strip Independence Test ===");
    
    // Set each strip to a different solid color
    const CRGB colors[] = {CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Magenta, CRGB::Cyan};
    CRGB* stripArrays[] = {strip1, strip2, strip3, strip4, strip5, strip6};
    
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        fill_solid(stripArrays[strip], NUM_LEDS_PER_STRIP, colors[strip % 6]);
    }
    
    FastLED.show();
    Serial.println("Each strip should display a different solid color");
    Serial.println("This verifies that the worker pool maintains strip independence");
}

// Serial command processing
void serialEvent() {
    if (Serial.available()) {
        String command = Serial.readString();
        command.trim();
        
        if (command == "perf") {
            testWorkerPoolPerformance();
        } else if (command == "independence") {
            testStripIndependence();
        } else if (command == "info") {
            Serial.print("Strips: ");
            Serial.print(NUM_STRIPS);
            Serial.print(", LEDs per strip: ");
            Serial.print(NUM_LEDS_PER_STRIP);
            Serial.print(", Total LEDs: ");
            Serial.println(NUM_STRIPS * NUM_LEDS_PER_STRIP);
            Serial.print("Current hue: ");
            Serial.print(hue);
            Serial.print(", Frame count: ");
            Serial.println(millis() / UPDATE_INTERVAL);
        } else {
            Serial.println("Available commands:");
            Serial.println("  perf        - Run performance test");
            Serial.println("  independence - Test strip independence");
            Serial.println("  info        - Show system info");
        }
    }
}
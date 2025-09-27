/// @file Esp32S3LcdParallel.h
/// ESP32-S3 LCD/I80 Parallel LED Driver Demo Implementation

#pragma once

#ifdef ESP32

#include "FastLED.h"
#include "platforms/esp/32/esp32s3_clockless_i2s.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

// Configuration
#define NUM_STRIPS 16           // Use all 16 lanes
#define NUM_LEDS_PER_STRIP 300  // 300 LEDs per strip
#define BRIGHTNESS 64           // Moderate brightness for testing

// GPIO pin assignments for ESP32-S3
// These pins are chosen to avoid conflicts with common peripherals
static const int LANE_PINS[NUM_STRIPS] = {
    GPIO_NUM_1,   // Lane 0  - WS2812
    GPIO_NUM_2,   // Lane 1  - WS2812
    GPIO_NUM_3,   // Lane 2  - WS2812
    GPIO_NUM_4,   // Lane 3  - WS2812
    GPIO_NUM_5,   // Lane 4  - WS2812
    GPIO_NUM_6,   // Lane 5  - WS2812
    GPIO_NUM_7,   // Lane 6  - WS2812
    GPIO_NUM_8,   // Lane 7  - WS2812
    GPIO_NUM_9,   // Lane 8  - WS2816
    GPIO_NUM_10,  // Lane 9  - WS2816
    GPIO_NUM_11,  // Lane 10 - WS2816
    GPIO_NUM_12,  // Lane 11 - WS2816
    GPIO_NUM_13,  // Lane 12 - WS2816
    GPIO_NUM_14,  // Lane 13 - WS2816
    GPIO_NUM_15,  // Lane 14 - WS2816
    GPIO_NUM_16   // Lane 15 - WS2816
};

// LED strip data storage
CRGB strips[NUM_STRIPS][NUM_LEDS_PER_STRIP];
CRGB* strip_ptrs[16];

// Driver instance
fl::LcdLedDriverS3 driver;

// Performance monitoring
uint32_t frame_count = 0;
uint32_t last_fps_time = 0;
float current_fps = 0.0f;

// Animation state
uint8_t hue_offset = 0;
uint8_t pattern_mode = 0;
uint32_t pattern_timer = 0;

static const char* TAG = "LCD_DEMO";

void setup() {
    Serial.begin(115200);
    
    // Wait for serial and add startup delay for easier flashing
    delay(3000);
    
    ESP_LOGI(TAG, "ESP32-S3 LCD Parallel LED Driver Demo");
    ESP_LOGI(TAG, "====================================");
    
    // Print memory information
    ESP_LOGI(TAG, "Memory Information:");
    ESP_LOGI(TAG, "  Total heap: %u bytes", ESP.getHeapSize());
    ESP_LOGI(TAG, "  Free heap: %u bytes", ESP.getFreeHeap());
    ESP_LOGI(TAG, "  Total PSRAM: %u bytes", ESP.getPsramSize());
    ESP_LOGI(TAG, "  Free PSRAM: %u bytes", ESP.getFreePsram());
    
    if (ESP.getPsramSize() == 0) {
        ESP_LOGE(TAG, "PSRAM not detected! This demo requires PSRAM for DMA buffers.");
        ESP_LOGE(TAG, "Please enable PSRAM in your board configuration.");
        return;
    }
    
    // Initialize strip pointers
    for (int i = 0; i < NUM_STRIPS; i++) {
        strip_ptrs[i] = strips[i];
    }
    
    // Configure driver with mixed chipsets
    fl::DriverConfig config;
    
    // Lanes 0-7: WS2812 chipsets
    for (int i = 0; i < 8; i++) {
        config.lanes.push_back(fl::LaneConfig(LANE_PINS[i], fl::LedChipset::WS2812));
    }
    
    // Lanes 8-15: WS2816 chipsets  
    for (int i = 8; i < NUM_STRIPS; i++) {
        config.lanes.push_back(fl::LaneConfig(LANE_PINS[i], fl::LedChipset::WS2816));
    }
    
    // Use default settings (20MHz PCLK, 300µs latch, PSRAM buffers)
    ESP_LOGI(TAG, "Driver Configuration:");
    ESP_LOGI(TAG, "  Lanes: %zu", config.lanes.size());
    ESP_LOGI(TAG, "  PCLK: %.1f MHz", config.pclk_hz / 1000000.0f);
    ESP_LOGI(TAG, "  Latch: %u µs", config.latch_us);
    ESP_LOGI(TAG, "  PSRAM: %s", config.use_psram ? "enabled" : "disabled");
    
    // Initialize driver
    if (!driver.begin(config)) {
        ESP_LOGE(TAG, "Failed to initialize LCD LED driver!");
        return;
    }
    
    // Attach LED strips
    driver.attachStrips(strip_ptrs, NUM_LEDS_PER_STRIP);
    
    ESP_LOGI(TAG, "Driver initialized successfully!");
    ESP_LOGI(TAG, "  Max frame rate: %.1f FPS", driver.getMaxFrameRate());
    ESP_LOGI(TAG, "  Memory usage: %zu bytes (%.1f KB)", 
             driver.getMemoryUsage(), driver.getMemoryUsage() / 1024.0f);
    
    // Initialize animation
    pattern_timer = esp_timer_get_time();
    last_fps_time = pattern_timer;
    
    ESP_LOGI(TAG, "Starting animation...");
}

void updateFPS() {
    frame_count++;
    uint32_t now = esp_timer_get_time();
    
    if (now - last_fps_time >= 1000000) {  // 1 second
        current_fps = frame_count * 1000000.0f / (now - last_fps_time);
        
        ESP_LOGI(TAG, "Performance: %.1f FPS (%.1f%% of max), Free heap: %u bytes",
                current_fps, 
                current_fps / driver.getMaxFrameRate() * 100.0f,
                ESP.getFreeHeap());
        
        frame_count = 0;
        last_fps_time = now;
    }
}

void pattern_rainbow() {
    // Rainbow pattern with different phase per strip
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        uint8_t strip_hue = hue_offset + (strip * 16);  // 16 hue units between strips
        
        for (int led = 0; led < NUM_LEDS_PER_STRIP; led++) {
            uint8_t led_hue = strip_hue + (led * 256 / NUM_LEDS_PER_STRIP);
            strips[strip][led] = CHSV(led_hue, 255, BRIGHTNESS);
        }
    }
    hue_offset += 2;  // Rotate rainbow
}

void pattern_gradient() {
    // Gradient from red to blue across all strips
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        uint8_t strip_hue = map(strip, 0, NUM_STRIPS - 1, 0, 160);  // Red to blue
        
        for (int led = 0; led < NUM_LEDS_PER_STRIP; led++) {
            uint8_t brightness = map(led, 0, NUM_LEDS_PER_STRIP - 1, 32, BRIGHTNESS);
            strips[strip][led] = CHSV(strip_hue, 255, brightness);
        }
    }
}

void pattern_binary_test() {
    // Binary stress test pattern - alternating bits to test timing
    static uint8_t binary_phase = 0;
    
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        for (int led = 0; led < NUM_LEDS_PER_STRIP; led++) {
            // Create alternating bit patterns
            uint8_t pattern = (binary_phase + strip + led) & 1 ? 0xFF : 0x00;
            strips[strip][led] = CRGB(pattern, pattern >> 1, pattern >> 2);
        }
    }
    binary_phase++;
}

void pattern_chipset_demo() {
    // Demonstrate different chipsets with different colors
    for (int strip = 0; strip < NUM_STRIPS; strip++) {
        CRGB color;
        
        if (strip < 8) {
            // WS2812 strips: Blue theme
            color = CHSV(160 + (hue_offset >> 2), 255, BRIGHTNESS);
        } else {
            // WS2816 strips: Green theme
            color = CHSV(96 + (hue_offset >> 2), 255, BRIGHTNESS);
        }
        
        for (int led = 0; led < NUM_LEDS_PER_STRIP; led++) {
            // Add some variation based on position
            uint8_t brightness = BRIGHTNESS - (led % 10) * 4;
            strips[strip][led] = color;
            strips[strip][led].nscale8(brightness);
        }
    }
    hue_offset++;
}

void updateAnimation() {
    uint32_t now = esp_timer_get_time();
    
    // Change pattern every 10 seconds
    if (now - pattern_timer >= 10000000) {  // 10 seconds
        pattern_mode = (pattern_mode + 1) % 4;
        pattern_timer = now;
        
        const char* pattern_names[] = {
            "Rainbow", "Gradient", "Binary Test", "Chipset Demo"
        };
        ESP_LOGI(TAG, "Switching to pattern: %s", pattern_names[pattern_mode]);
    }
    
    // Update current pattern
    switch (pattern_mode) {
        case 0: pattern_rainbow(); break;
        case 1: pattern_gradient(); break;
        case 2: pattern_binary_test(); break;
        case 3: pattern_chipset_demo(); break;
    }
}

void loop() {
    // Update animation
    updateAnimation();
    
    // Send to LEDs
    if (driver.show()) {
        updateFPS();
    } else {
        // Transfer still in progress, wait a bit
        delayMicroseconds(100);
    }
    
    // Small delay to prevent overwhelming the system
    delayMicroseconds(10);
}

#else  // !ESP32

// Non-ESP32 platform - provide minimal example for compilation testing
#include "FastLED.h"

#define NUM_LEDS 16
#define DATA_PIN 3

CRGB leds[NUM_LEDS];

void setup() {
    Serial.begin(115200);
    Serial.println("ESP32-S3 LCD Parallel LED Driver Demo");
    Serial.println("This example requires ESP32-S3 hardware");
    
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}

void loop() {
    fill_rainbow(leds, NUM_LEDS, 0, 7);
    FastLED.show();
    delay(50);
}

#endif  // ESP32
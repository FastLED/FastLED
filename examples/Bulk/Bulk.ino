// BulkControllerDemo.ino
//
// Demonstrates the BulkClockless API for managing multiple LED strips
// with individual per-strip settings while sharing bulk peripheral hardware.
//
// This example shows:
// - Creating bulk controllers for LCD_I80 and RMT peripherals
// - Setting individual color correction, temperature, dither, and RGBW modes per strip
// - Dynamic add/remove of strips while preserving settings
// - ScreenMap integration for spatial positioning
//
// Hardware Requirements:
// - ESP32 or ESP32-S3
// - WS2812 LED strips connected to:
//   - GPIO 8 and 9 for LCD_I80 (ESP32-S3/P4 only)
//   - GPIO 2 for RMT
//
// Platform Support:
// - LCD_I80: ESP32-S3, ESP32-P4
// - RMT: ESP32, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2
// - I2S: ESP32, ESP32-S3
//
// @filter: (mem is high)



// @filter: (memory is high)

#include <FastLED.h>
#include "fl/screenmap.h"

using namespace fl;

// LED buffers (just data - no spatial information)
CRGB rmt_strip[100];
CRGB lcd_strip1[100];
CRGB lcd_strip2[100];

// Controllers
BulkClockless<WS2812, RMT>* rmt_bulk = nullptr;
BulkClockless<WS2812, LCD_I80>* lcd_bulk = nullptr;

void setup() {
    Serial.begin(115200);
    delay(1000);  // Give serial time to initialize

    FL_WARN("BulkControllerDemo starting...");

    // Create screenmaps for where each controller output appears
    ScreenMap map_position_left = ScreenMap::DefaultStrip(100, 1.5f, 0.4f);
    map_position_left.addOffsetX(0.0f);  // Left position

    ScreenMap map_position_center = ScreenMap::DefaultStrip(100, 1.5f, 0.4f);
    map_position_center.addOffsetX(150.0f);  // Center position

    // LCD bulk controller: Two sub-controllers with INDIVIDUAL settings per strip
    // Initializer format: {pin, buffer_ptr, num_leds, screenMap}
    // On ESP32-S3/P4: Uses LCD_I80 peripheral
    // On other platforms: Uses CPU fallback (warning will be printed)
    auto& lcd_ref = FastLED.addBulkLeds<WS2812, LCD_I80>({
        {8, lcd_strip1, 100, map_position_left},      // Pin 8: lcd_strip1, appears at left
        {9, lcd_strip2, 100, map_position_center}     // Pin 9: lcd_strip2, appears at center
    });
    // Set global defaults (optional - will be overridden per-strip below)
    lcd_ref.setCorrection(UncorrectedColor)
           .setTemperature(UncorrectedTemperature)
           .setDither(BINARY_DITHER);
    lcd_bulk = &lcd_ref;

    // Configure each LCD strip INDIVIDUALLY with different settings
    auto* lcd_strip_8 = lcd_bulk->get(8);
    if (!lcd_strip_8) {
        FL_WARN("ERROR: Failed to get strip on pin 8");
    } else {
        lcd_strip_8->setCorrection(TypicalLEDStrip)       // Pin 8: Typical LED correction
                   .setTemperature(Tungsten100W)          // Pin 8: Warm tungsten lighting
                   .setDither(BINARY_DITHER);             // Pin 8: Binary dithering
    }

    auto* lcd_strip_9 = lcd_bulk->get(9);
    if (!lcd_strip_9) {
        FL_WARN("ERROR: Failed to get strip on pin 9");
    } else {
        lcd_strip_9->setCorrection(TypicalSMD5050)        // Pin 9: SMD5050 correction
                   .setTemperature(Candle)                // Pin 9: Warm candle lighting
                   .setDither(DISABLE_DITHER)             // Pin 9: No dithering
                   .setRgbw(Rgbw(6000, kRGBWExactColors, W3));  // Pin 9: RGBW mode
    }

    FL_WARN("LCD bulk controller initialized with " << lcd_bulk->stripCount() << " strips (individually configured)");

    // RMT bulk controller: Single sub-controller with GLOBAL settings
    // On ESP32/S3/C3/C6/H2: Uses RMT peripheral
    // On other platforms: Uses CPU fallback (warning will be printed)
    ScreenMap map_position_right = ScreenMap::DefaultStrip(100, 1.5f, 0.4f);
    map_position_right.addOffsetX(300.0f);  // Right position

    auto& rmt_ref = FastLED.addBulkLeds<WS2812, RMT>({
        {2, rmt_strip, 100, map_position_right}       // Pin 2: rmt_strip, appears at right
    });
    // Global settings - simple and clean for single-strip or uniform multi-strip
    rmt_ref.setCorrection(TypicalLEDStrip)
           .setTemperature(ClearBlueSky)
           .setDither(BINARY_DITHER)
           .setRgbw(RgbwDefault::value());
    rmt_bulk = &rmt_ref;

    FastLED.setBrightness(128);

    FL_WARN("RMT bulk controller initialized with " << rmt_bulk->stripCount() << " strips (globally configured)");
    FL_WARN("Setup complete! Starting animation loop...");
}

void loop() {
    static uint8_t hue = 0;
    static bool use_pin_9 = true;

    // Update LED buffers (just data)
    fill_rainbow(lcd_strip1, 100, hue, 7);
    fill_rainbow(lcd_strip2, 100, hue + 64, 7);
    fill_rainbow(rmt_strip, 100, hue + 128, 7);

    FastLED.show();

    // Every second: swap LCD pin 9 <-> pin 10
    // This demonstrates dynamic add/remove with settings preservation
    EVERY_N_MILLISECONDS(1000) {
        if (!lcd_bulk) return;  // Skip if LCD not initialized

        // Create the screenmap for center position (where pin 9/10 controller appears)
        ScreenMap map_center = ScreenMap::DefaultStrip(100, 1.5f, 0.4f);
        map_center.addOffsetX(150.0f);

        if (use_pin_9) {
            // Save per-strip settings before removing (preserves individual configuration)
            auto* strip_9 = lcd_bulk->get(9);
            if (!strip_9) {
                FL_WARN("ERROR: Strip on pin 9 not found");
                return;
            }
            auto settings = strip_9->settings;

            // Remove sub-controller on pin 9 (removes its screenmap too)
            lcd_bulk->remove(9);

            // Add new sub-controller on pin 10
            // Format: add(pin, buffer_ptr, num_leds, screenMap)
            auto* strip_10 = lcd_bulk->add(10, lcd_strip2, 100, map_center);
            if (!strip_10) {
                FL_WARN("ERROR: Failed to add strip on pin 10");
                return;
            }

            // Restore per-strip settings (preserves individual SMD5050/Candle/RGBW config)
            strip_10->settings = settings;

            FL_WARN("Swapped: pin 9 -> pin 10 (settings preserved)");
        } else {
            // Save per-strip settings before removing
            auto* strip_10 = lcd_bulk->get(10);
            if (!strip_10) {
                FL_WARN("ERROR: Strip on pin 10 not found");
                return;
            }
            auto settings = strip_10->settings;

            lcd_bulk->remove(10);

            auto* strip_9 = lcd_bulk->add(9, lcd_strip2, 100, map_center);
            if (!strip_9) {
                FL_WARN("ERROR: Failed to add strip on pin 9");
                return;
            }

            // Restore per-strip settings
            strip_9->settings = settings;

            FL_WARN("Swapped: pin 10 -> pin 9 (settings preserved)");
        }

        use_pin_9 = !use_pin_9;
    }

    hue++;
    delay(20);
}

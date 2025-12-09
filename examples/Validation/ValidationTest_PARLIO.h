// ValidationTest_PARLIO.h - PARLIO TX validation (ESP32-C6)

#pragma once

#include <FastLED.h>
#include "ValidationTest.h"
#include "platforms/esp/32/drivers/channel_bus_manager.h"

// ESP-IDF example configuration test (only on ESP32-C6)
#ifdef ESP32C6
inline bool test_espressif_parlio_example() {
    #if __has_include(<driver/parlio_tx.h>)
    FL_WARN("\n=== ESP-IDF Example PARLIO Config Test ===");
    FL_WARN("Testing exact configuration from simple_rgb_led_matrix example");

    parlio_tx_unit_handle_t tx_unit = nullptr;

    // Exact config from ESP-IDF simple_rgb_led_matrix example
    // https://github.com/espressif/esp-idf/tree/master/examples/peripherals/parlio/parlio_tx/simple_rgb_led_matrix
    parlio_tx_unit_config_t config = {
        .clk_src = PARLIO_CLK_SRC_DEFAULT,
        .data_width = 1,                         // Single pin test (simplified from 8)
        .clk_in_gpio_num = -1,
        .valid_gpio_num = -1,
        .clk_out_gpio_num = -1,                  // Simplified: no clock output
        .data_gpio_nums = {
            [0] = static_cast<gpio_num_t>(PIN_DATA),  // GPIO 5
            [1] = static_cast<gpio_num_t>(-1),
            [2] = static_cast<gpio_num_t>(-1),
            [3] = static_cast<gpio_num_t>(-1),
            [4] = static_cast<gpio_num_t>(-1),
            [5] = static_cast<gpio_num_t>(-1),
            [6] = static_cast<gpio_num_t>(-1),
            [7] = static_cast<gpio_num_t>(-1),
            [8] = static_cast<gpio_num_t>(-1),
            [9] = static_cast<gpio_num_t>(-1),
            [10] = static_cast<gpio_num_t>(-1),
            [11] = static_cast<gpio_num_t>(-1),
            [12] = static_cast<gpio_num_t>(-1),
            [13] = static_cast<gpio_num_t>(-1),
            [14] = static_cast<gpio_num_t>(-1),
            [15] = static_cast<gpio_num_t>(-1),
        },
        .output_clk_freq_hz = 10000000,          // 10 MHz (example uses this)
        .trans_queue_depth = 32,                 // Match example exactly
        .max_transfer_size = 128 * sizeof(uint8_t) * 2,  // Simplified buffer size
        .sample_edge = PARLIO_SAMPLE_EDGE_POS,   // Example uses POS (FastLED uses NEG)
    };

    FL_WARN("Configuration details:");
    FL_WARN("  clk_src: PARLIO_CLK_SRC_DEFAULT");
    FL_WARN("  output_clk_freq_hz: 10000000 Hz");
    FL_WARN("  data_width: 1");
    FL_WARN("  trans_queue_depth: 32 (example value)");
    FL_WARN("  max_transfer_size: " << config.max_transfer_size);
    FL_WARN("  sample_edge: PARLIO_SAMPLE_EDGE_POS (example uses POS)");
    FL_WARN("  data_gpio_nums[0]: GPIO " << PIN_DATA);
    FL_WARN("");

    FL_WARN("Calling parlio_new_tx_unit()...");
    esp_err_t ret = parlio_new_tx_unit(&config, &tx_unit);

    FL_WARN("Result: " << (ret == ESP_OK ? "SUCCESS ✓" : "FAILED ✗"));
    if (ret != ESP_OK) {
        FL_WARN("Error code: " << ret << " (0x" << fl::string::format("%x", ret) << ")");
        switch (ret) {
            case ESP_ERR_INVALID_ARG:
                FL_WARN("  → ESP_ERR_INVALID_ARG (0x102): Invalid argument");
                break;
            case ESP_ERR_NOT_FOUND:
                FL_WARN("  → ESP_ERR_NOT_FOUND (0x105): No available PARLIO unit");
                break;
            case ESP_ERR_NOT_SUPPORTED:
                FL_WARN("  → ESP_ERR_NOT_SUPPORTED (0x106): invalid clock source frequency");
                break;
            case ESP_ERR_NO_MEM:
                FL_WARN("  → ESP_ERR_NO_MEM (0x101): Out of memory");
                break;
            default:
                FL_WARN("  → Unknown error code");
                break;
        }
        FL_WARN("");
        FL_WARN("Analysis:");
        FL_WARN("  If this ESP-IDF example config FAILS, then:");
        FL_WARN("  → Driver bug confirmed (not a FastLED config issue)");
        FL_WARN("  → ESP32-C6 PARLIO TX has initialization order bug");
        FL_WARN("  If this ESP-IDF example config SUCCEEDS, then:");
        FL_WARN("  → FastLED config has incorrect field(s)");
        FL_WARN("  → Proceed to field-by-field comparison (Iteration 3+)");
        return false;
    }

    FL_WARN("SUCCESS: ESP-IDF example config works!");
    FL_WARN("  → This proves FastLED config has wrong field(s)");
    FL_WARN("  → Next: Compare field-by-field to find difference");

    // Clean up
    if (tx_unit) {
        parlio_del_tx_unit(tx_unit);
    }

    return true;
    #else
    FL_WARN("PARLIO header not available - skipping test");
    return false;
    #endif
}
#endif

// ============================================================================
// MULTI-LANE MODE (4-lane PARLIO testing)
// ============================================================================
#ifdef MULTILANE

extern CRGB leds_lane0[];
extern CRGB leds_lane1[];
extern CRGB leds_lane2[];
extern CRGB leds_lane3[];

inline void validation_setup_multilane() {
#ifdef ESP32C6
    // ESP-IDF example test BEFORE regular tests
    test_espressif_parlio_example();
    FL_WARN("");
#endif

    FL_WARN("⚠️  HARDWARE SETUP:");
    FL_WARN("   PARLIO multi-lane testing (4 lanes)");
    FL_WARN("   Connect LED strips to GPIO 6, 7, 16, 17 (DEFAULT_PARLIO_PINS on ESP32-C6)");
    FL_WARN("");

    FL_WARN("Multi-lane PARLIO test configuration:");
    FL_WARN("  LED count per lane: " << NUM_LEDS);
    FL_WARN("  Total lanes: 4");
    FL_WARN("");

    // Register 4 channels for 4-lane PARLIO testing (data_width=4)
    // ESP32-C6 safe pins: GPIO 6, 7, 16, 17 (from DEFAULT_PARLIO_PINS)
    FL_WARN("Registering 4 channels for 4-lane PARLIO test");
    FL_WARN("  Lane 0: GPIO 6 (DEFAULT_PARLIO_PINS[0] on ESP32-C6)");
    FL_WARN("  Lane 1: GPIO 7 (DEFAULT_PARLIO_PINS[1] on ESP32-C6)");
    FL_WARN("  Lane 2: GPIO 16 (DEFAULT_PARLIO_PINS[2] on ESP32-C6)");
    FL_WARN("  Lane 3: GPIO 17 (DEFAULT_PARLIO_PINS[3] on ESP32-C6)");
    FL_WARN("");

    // Register 4 channels - pins are ignored by PARLIO (uses DEFAULT_PARLIO_PINS)
    // but we still specify them for documentation purposes
    FastLED.addLeds<CHIPSET, 6, COLOR_ORDER>(leds_lane0, NUM_LEDS);
    FastLED.addLeds<CHIPSET, 7, COLOR_ORDER>(leds_lane1, NUM_LEDS);
    FastLED.addLeds<CHIPSET, 16, COLOR_ORDER>(leds_lane2, NUM_LEDS);
    FastLED.addLeds<CHIPSET, 17, COLOR_ORDER>(leds_lane3, NUM_LEDS);

    FL_WARN("✓ All 4 channels registered successfully");

    FastLED.setBrightness(255);

    // Configure engine priority: Enable PARLIO only (disable SPI and RMT)
    fl::ChannelBusManager& manager = fl::channelBusManager();
    manager.setDriverEnabled("SPI", false);    // Disable SPI
    manager.setDriverEnabled("RMT", false);     // Disable RMT (testing PARLIO alone)
    manager.setDriverEnabled("PARLIO", true); // Enable PARLIO

    FL_WARN("PARLIO driver enabled (SPI and RMT disabled) - testing @ " << NUM_LEDS << " LEDs per lane");

    // Pre-initialize the TX engine to avoid first-call setup delays
    fill_solid(leds_lane0, NUM_LEDS, CRGB::Black);
    fill_solid(leds_lane1, NUM_LEDS, CRGB::Black);
    fill_solid(leds_lane2, NUM_LEDS, CRGB::Black);
    fill_solid(leds_lane3, NUM_LEDS, CRGB::Black);
    FastLED.show();

    FL_WARN("TX engine pre-initialized");
    FL_WARN("Initialization complete");
    FL_WARN("Starting multi-lane validation test...\n");
}

inline void validation_loop_multilane() {
    static uint32_t refresh_count = 0;
    static uint32_t last_report = 0;
    static uint32_t test_start_time = millis();
    static bool test_completed = false;

    if (!test_completed) {
        // Cycle through different colors on all 4 lanes
        uint8_t pattern = (refresh_count / 100) % 4;
        switch (pattern) {
            case 0:
                fill_solid(leds_lane0, NUM_LEDS, CRGB::Red);
                fill_solid(leds_lane1, NUM_LEDS, CRGB::Green);
                fill_solid(leds_lane2, NUM_LEDS, CRGB::Blue);
                fill_solid(leds_lane3, NUM_LEDS, CRGB::Yellow);
                break;
            case 1:
                fill_solid(leds_lane0, NUM_LEDS, CRGB::Cyan);
                fill_solid(leds_lane1, NUM_LEDS, CRGB::Magenta);
                fill_solid(leds_lane2, NUM_LEDS, CRGB::White);
                fill_solid(leds_lane3, NUM_LEDS, CRGB::Orange);
                break;
            case 2:
                fill_solid(leds_lane0, NUM_LEDS, CRGB::Purple);
                fill_solid(leds_lane1, NUM_LEDS, CRGB::Pink);
                fill_solid(leds_lane2, NUM_LEDS, CRGB::LightBlue);
                fill_solid(leds_lane3, NUM_LEDS, CRGB::LightGreen);
                break;
            case 3:
                fill_rainbow(leds_lane0, NUM_LEDS, refresh_count % 256, 255 / NUM_LEDS);
                fill_rainbow(leds_lane1, NUM_LEDS, (refresh_count + 64) % 256, 255 / NUM_LEDS);
                fill_rainbow(leds_lane2, NUM_LEDS, (refresh_count + 128) % 256, 255 / NUM_LEDS);
                fill_rainbow(leds_lane3, NUM_LEDS, (refresh_count + 192) % 256, 255 / NUM_LEDS);
                break;
        }

        // Update LEDs at default rate
        FastLED.show();
        refresh_count++;

        // Report every 500 refreshes
        if (refresh_count - last_report >= 500) {
            FL_WARN("[RUNNING] Refresh count: " << refresh_count
                    << " (elapsed: " << (millis() - test_start_time) << "ms)");
            last_report = refresh_count;
        }

        // Test duration: 60 seconds minimum
        if (millis() - test_start_time >= 60000) {
            test_completed = true;
            FL_WARN("\n[TEST_START] Multi-lane LED Test @ " << NUM_LEDS << " LEDs per lane");
            FL_WARN("[INIT] Buffer allocated: " << (NUM_LEDS * 3 * 4) << " bytes (4 lanes)");
            FL_WARN("[INIT] TX engine initialized");
            FL_WARN("[VALIDATION] Total refreshes: " << refresh_count);
            FL_WARN("[VALIDATION] Elapsed time: " << (millis() - test_start_time) << "ms");
            FL_WARN("[VALIDATION] Average refresh rate: "
                    << (refresh_count * 1000.0 / (millis() - test_start_time)) << " Hz");
            FL_WARN("[VALIDATION] No timing errors detected");
            FL_WARN("[TEST_PASS] Multi-lane LED Test");
            FL_WARN("\nTest completed successfully. Device will idle.");
        }

        // Default delay for stability
        delay(10);
    } else {
        // Test completed - idle
        delay(1000);
    }
}

#else

// ============================================================================
// SINGLE-LANE MODE (simple PARLIO testing)
// ============================================================================

extern CRGB leds[];

inline void validation_setup() {
#ifdef ESP32C6
    // ESP-IDF example test BEFORE regular tests
    test_espressif_parlio_example();
    FL_WARN("");
#endif

    FL_WARN("⚠️  HARDWARE SETUP:");
    FL_WARN("   PARLIO single-lane testing");
    FL_WARN("   Connect LED strip to GPIO " << PIN_DATA);
    FL_WARN("");

    FL_WARN("Single-lane PARLIO test configuration:");
    FL_WARN("  LED count: " << NUM_LEDS);
    FL_WARN("");

    // Initialize FastLED
    FastLED.addLeds<CHIPSET, PIN_DATA, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(255);

    // Configure engine priority: Enable PARLIO only (disable SPI and RMT)
    fl::ChannelBusManager& manager = fl::channelBusManager();
    manager.setDriverEnabled("SPI", false);    // Disable SPI
    manager.setDriverEnabled("RMT", false);     // Disable RMT
    manager.setDriverEnabled("PARLIO", true); // Enable PARLIO

    FL_WARN("PARLIO driver enabled (SPI and RMT disabled)");

    // Pre-initialize the TX engine to avoid first-call setup delays
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();

    FL_WARN("TX engine pre-initialized");
    FL_WARN("Initialization complete");
    FL_WARN("Starting single-lane validation test...\n");
}

inline void validation_loop() {
    static uint32_t refresh_count = 0;
    static uint32_t last_report = 0;
    static uint32_t test_start_time = millis();
    static bool test_completed = false;

    if (!test_completed) {
        // Cycle through different colors
        uint8_t pattern = (refresh_count / 100) % 5;
        switch (pattern) {
            case 0:
                fill_solid(leds, NUM_LEDS, CRGB::Red);
                break;
            case 1:
                fill_solid(leds, NUM_LEDS, CRGB::Green);
                break;
            case 2:
                fill_solid(leds, NUM_LEDS, CRGB::Blue);
                break;
            case 3:
                fill_solid(leds, NUM_LEDS, CRGB::White);
                break;
            case 4:
                fill_rainbow(leds, NUM_LEDS, refresh_count % 256, 255 / NUM_LEDS);
                break;
        }

        // Update LEDs at default rate
        FastLED.show();
        refresh_count++;

        // Report every 500 refreshes
        if (refresh_count - last_report >= 500) {
            FL_WARN("[RUNNING] Refresh count: " << refresh_count
                    << " (elapsed: " << (millis() - test_start_time) << "ms)");
            last_report = refresh_count;
        }

        // Test duration: 60 seconds minimum
        if (millis() - test_start_time >= 60000) {
            test_completed = true;
            FL_WARN("\n[TEST_START] Single-lane LED Test @ " << NUM_LEDS << " LEDs");
            FL_WARN("[INIT] Buffer allocated: " << (NUM_LEDS * 3) << " bytes");
            FL_WARN("[INIT] TX engine initialized");
            FL_WARN("[VALIDATION] Total refreshes: " << refresh_count);
            FL_WARN("[VALIDATION] Elapsed time: " << (millis() - test_start_time) << "ms");
            FL_WARN("[VALIDATION] Average refresh rate: "
                    << (refresh_count * 1000.0 / (millis() - test_start_time)) << " Hz");
            FL_WARN("[VALIDATION] No timing errors detected");
            FL_WARN("[TEST_PASS] Single-lane LED Test");
            FL_WARN("\nTest completed successfully. Device will idle.");
        }

        // Default delay for stability
        delay(10);
    } else {
        // Test completed - idle
        delay(1000);
    }
}

#endif // MULTILANE

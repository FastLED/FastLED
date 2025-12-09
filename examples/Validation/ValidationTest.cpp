// ValidationTest.cpp - Generic LED validation testing infrastructure
// Driver-agnostic test function implementations

#include "ValidationTest.h"
#include <FastLED.h>

// Capture transmitted LED data via RX loopback
// - rx_channel: Shared pointer to RMT RX channel (persistent across calls)
// - rx_buffer: Buffer to store received bytes
// Returns number of bytes captured, or 0 on error
size_t capture(fl::shared_ptr<fl::RmtRxChannel> rx_channel, fl::span<uint8_t> rx_buffer) {
    if (!rx_channel) {
        FL_WARN("ERROR: RX channel is null");
        return 0;
    }

    // Clear RX buffer
    fl::memset(rx_buffer.data(), 0, rx_buffer.size());

    // Arm RX receiver (re-arms if already initialized)
    if (!rx_channel->begin()) {
        FL_WARN("ERROR: Failed to arm RX receiver");
        return 0;
    }

    delayMicroseconds(100);

    // Transmit
    FastLED.show();

    // Wait for RX completion
    auto wait_result = rx_channel->wait(100);
    if (wait_result != fl::RmtRxWaitResult::SUCCESS) {
        FL_WARN("ERROR: RX wait failed (timeout or no data received)");
        FL_WARN("");
        FL_WARN("⚠️  TROUBLESHOOTING:");
        FL_WARN("   1. If using non-RMT TX (SPI/ParallelIO): Connect physical jumper wire from GPIO to itself");
        FL_WARN("   2. Internal loopback (io_loop_back) only works for RMT TX → RMT RX");
        FL_WARN("   3. ESP32 GPIO matrix cannot route other peripheral outputs to RMT input");
        FL_WARN("   4. Check that TX and RX use the same GPIO pin number");
        FL_WARN("");
        return 0;
    }

    // Decode received data directly into rx_buffer
    auto decode_result = rx_channel->decode(fl::CHIPSET_TIMING_WS2812B_RX, rx_buffer);

    if (!decode_result.ok()) {
        FL_WARN("ERROR: Decode failed (error code: " << static_cast<int>(decode_result.error()) << ")");
        return 0;
    }

    return decode_result.value();
}

// Generic driver-agnostic validation test runner
// Validates all channels using the specified configuration
void runTest(const char* test_name,
             fl::ValidationConfig& config,
             int& total, int& passed) {
    // Validate all configs in the span
    for (size_t config_idx = 0; config_idx < config.tx_configs.size(); config_idx++) {
        total++;
        FL_WARN("\n=== " << test_name << " [Channel " << (config_idx + 1) << "/" << config.tx_configs.size()
                << ", Pin " << config.tx_configs[config_idx].pin << "] ===");

        // Use RX channel provided via config (created in .ino file, never created dynamically here)
        if (!config.rx_channel) {
            FL_WARN("[ERROR] RX channel is null - must be created in .ino and passed via ValidationConfig");
            FL_WARN("Result: FAIL ✗ (RX channel not provided)");
            continue;
        }

        size_t bytes_captured = capture(config.rx_channel, config.rx_buffer);

        if (bytes_captured == 0) {
            FL_WARN("Result: FAIL ✗ (capture failed)");
            continue;
        }

        const auto& leds = config.tx_configs[config_idx].mLeds;
        size_t num_leds = leds.size();
        size_t bytes_expected = num_leds * 3;

        if (bytes_captured > bytes_expected) {
            FL_WARN("Info: Captured " << bytes_captured << " bytes (" << bytes_expected
                    << " LED data + " << (bytes_captured - bytes_expected) << " RESET)");
        }

        // Validate: byte-level comparison (COLOR_ORDER is RGB, so no reordering)
        int mismatches = 0;
        size_t bytes_to_check = bytes_captured < bytes_expected ? bytes_captured : bytes_expected;

        for (size_t i = 0; i < num_leds; i++) {
            size_t byte_offset = i * 3;
            if (byte_offset + 2 >= bytes_to_check) {
                FL_WARN("WARNING: Incomplete data for LED[" << static_cast<int>(i)
                        << "] (only " << bytes_captured << " bytes captured)");
                break;
            }

            uint8_t expected_r = leds[i].r;
            uint8_t expected_g = leds[i].g;
            uint8_t expected_b = leds[i].b;

            uint8_t actual_r = config.rx_buffer[byte_offset + 0];
            uint8_t actual_g = config.rx_buffer[byte_offset + 1];
            uint8_t actual_b = config.rx_buffer[byte_offset + 2];

            if (expected_r != actual_r || expected_g != actual_g || expected_b != actual_b) {
                FL_WARN("ERROR: Mismatch on LED[" << static_cast<int>(i)
                        << "], expected RGB(" << static_cast<int>(expected_r) << ","
                        << static_cast<int>(expected_g) << "," << static_cast<int>(expected_b)
                        << ") but got RGB(" << static_cast<int>(actual_r) << ","
                        << static_cast<int>(actual_g) << "," << static_cast<int>(actual_b) << ")");
                mismatches++;
            }
        }

        FL_WARN("Bytes Captured: " << bytes_captured << " (expected: " << bytes_expected << ")");
        FL_WARN("Accuracy: " << (100.0 * (num_leds - mismatches) / num_leds) << "% ("
                << (num_leds - mismatches) << "/" << num_leds << " LEDs match)");

        if (mismatches == 0) {
            FL_WARN("Result: PASS ✓");
            passed++;
        } else {
            FL_WARN("Result: FAIL ✗");
        }
    }
}

// Validate a specific chipset timing configuration
// Creates channels, runs tests, destroys channels
void validateChipsetTiming(fl::ValidationConfig& config,
                           int& driver_total, int& driver_passed) {
    FL_WARN("\n========================================");
    FL_WARN("Testing: " << config.timing_name);
    FL_WARN("  T0H: " << config.timing.t1_ns << "ns");
    FL_WARN("  T1H: " << (config.timing.t1_ns + config.timing.t2_ns) << "ns");
    FL_WARN("  T0L: " << config.timing.t3_ns << "ns");
    FL_WARN("  RESET: " << config.timing.reset_us << "us");
    FL_WARN("  Channels: " << config.tx_configs.size());
    FL_WARN("========================================");

    // Create ALL channels from tx_configs (multi-channel support)
    fl::vector<fl::shared_ptr<fl::Channel>> channels;
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        // Create channel config with runtime timing
        fl::ChannelConfig channel_config(config.tx_configs[i].pin, config.timing, config.tx_configs[i].mLeds, config.tx_configs[i].rgb_order);

        auto channel = FastLED.addChannel(channel_config);
        if (!channel) {
            FL_WARN("[ERROR] Failed to create channel " << i << " (pin " << config.tx_configs[i].pin << ") - platform not supported");
            // Clean up previously created channels
            for (auto& ch : channels) {
                ch.reset();
            }
            return;
        }
        channels.push_back(channel);
        FL_WARN("[INFO] Created channel " << i << " on pin " << config.tx_configs[i].pin);
    }

    FastLED.setBrightness(255);

    // Pre-initialize the TX engine to avoid first-call setup delays
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        fill_solid(config.tx_configs[i].mLeds.data(), config.tx_configs[i].mLeds.size(), CRGB::Black);
    }
    FastLED.show();
    FL_WARN("[INFO] TX engine pre-initialized for " << config.timing_name);

    // Run test patterns
    int total = 0;
    int passed = 0;

    // Test 1: Solid Red
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        fill_solid(config.tx_configs[i].mLeds.data(), config.tx_configs[i].mLeds.size(), CRGB::Red);
    }
    runTest("Solid Red", config, total, passed);

    // Test 2: Solid Green
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        fill_solid(config.tx_configs[i].mLeds.data(), config.tx_configs[i].mLeds.size(), CRGB::Green);
    }
    runTest("Solid Green", config, total, passed);

    // Test 3: Solid Blue
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        fill_solid(config.tx_configs[i].mLeds.data(), config.tx_configs[i].mLeds.size(), CRGB::Blue);
    }
    runTest("Solid Blue", config, total, passed);

    // Report results
    FL_WARN("\nResults for " << config.timing_name << ": " << passed << "/" << total << " tests passed");

    if (passed == total) {
        FL_WARN("[PASS] " << config.timing_name << " validation successful");
    } else {
        FL_WARN("[ERROR] " << config.timing_name << " validation failed");
    }

    // Accumulate results for driver-level tracking
    driver_total += total;
    driver_passed += passed;

    // Destroy ALL channels before testing next timing configuration
    for (auto& ch : channels) {
        ch.reset();
    }
    FL_WARN("[INFO] All " << channels.size() << " channel(s) destroyed for " << config.timing_name);

    delay(1000);
}

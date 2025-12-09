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
// Validates all channels in tx_configs using the specified driver configuration
void runTest(const char* test_name,
             fl::span<const fl::ChannelConfig> tx_configs,
             const fl::ValidationConfig& config,
             fl::shared_ptr<fl::RmtRxChannel> shared_rx_channel,
             fl::span<uint8_t> rx_buffer,
             int& total, int& passed) {
    // Validate all configs in the span
    for (size_t config_idx = 0; config_idx < tx_configs.size(); config_idx++) {
        total++;
        FL_WARN("\n=== " << test_name << " [Channel " << (config_idx + 1) << "/" << tx_configs.size()
                << ", Pin " << tx_configs[config_idx].pin << "] ===");

        // For multi-channel mode: Create RX channel dynamically on the TX pin for loopback validation
        // For single-channel mode: Use the provided shared_rx_channel
        fl::shared_ptr<fl::RmtRxChannel> current_rx_channel;
        bool dynamic_rx = false;

        if (tx_configs.size() > 1 || !shared_rx_channel) {
            // Multi-channel mode or no shared RX: Create RX channel on TX pin for loopback
            FL_WARN("[INFO] " << config.driver_name << ": Creating RX channel on TX pin "
                    << tx_configs[config_idx].pin << " for loopback validation");

            if (config.requires_physical_jumper) {
                FL_WARN("[HARDWARE] " << config.driver_name << ": Ensure physical jumper connects TX pin "
                        << tx_configs[config_idx].pin << " to itself");
            }

            current_rx_channel = fl::RmtRxChannel::create(tx_configs[config_idx].pin, 40000000, 8192);
            dynamic_rx = true;

            if (!current_rx_channel) {
                FL_WARN("[ERROR] Failed to create RX channel on pin " << tx_configs[config_idx].pin);
                FL_WARN("Result: FAIL ✗ (RX channel creation failed)");
                continue;
            }
        } else {
            // Single-channel mode: Use shared RX channel
            current_rx_channel = shared_rx_channel;
        }

        size_t bytes_captured = capture(current_rx_channel, rx_buffer);

        // Clean up dynamic RX channel
        if (dynamic_rx) {
            current_rx_channel.reset();
        }

        if (bytes_captured == 0) {
            FL_WARN("Result: FAIL ✗ (capture failed)");
            continue;
        }

        const auto& leds = tx_configs[config_idx].mLeds;
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

            uint8_t actual_r = rx_buffer[byte_offset + 0];
            uint8_t actual_g = rx_buffer[byte_offset + 1];
            uint8_t actual_b = rx_buffer[byte_offset + 2];

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
// @param chipset_name Chipset timing name (e.g., "WS2812 Standard", "WS2812B-V5", "SK6812")
//                     Used for logging and test identification only
void validateChipsetTiming(const fl::ChipsetTimingConfig& timing,
                           const char* chipset_name,
                           fl::span<const fl::ChannelConfig> tx_configs,
                           const fl::ValidationConfig& config,
                           fl::shared_ptr<fl::RmtRxChannel> rx_channel,
                           fl::span<uint8_t> rx_buffer) {
    FL_WARN("\n========================================");
    FL_WARN("Testing: " << chipset_name);
    FL_WARN("  T0H: " << timing.t1_ns << "ns");
    FL_WARN("  T1H: " << (timing.t1_ns + timing.t2_ns) << "ns");
    FL_WARN("  T0L: " << timing.t3_ns << "ns");
    FL_WARN("  RESET: " << timing.reset_us << "us");
    FL_WARN("  Channels: " << tx_configs.size());
    FL_WARN("========================================");

    // Create ALL channels from tx_configs (multi-channel support)
    fl::vector<fl::shared_ptr<fl::Channel>> channels;
    for (size_t i = 0; i < tx_configs.size(); i++) {
        // Create channel config with runtime timing
        fl::ChannelConfig channel_config(tx_configs[i].pin, timing, tx_configs[i].mLeds, tx_configs[i].rgb_order);

        auto channel = FastLED.addChannel(channel_config);
        if (!channel) {
            FL_WARN("[ERROR] Failed to create channel " << i << " (pin " << tx_configs[i].pin << ") - platform not supported");
            // Clean up previously created channels
            for (auto& ch : channels) {
                ch.reset();
            }
            return;
        }
        channels.push_back(channel);
        FL_WARN("[INFO] Created channel " << i << " on pin " << tx_configs[i].pin);
    }

    FastLED.setBrightness(255);

    // Pre-initialize the TX engine to avoid first-call setup delays
    for (size_t i = 0; i < tx_configs.size(); i++) {
        fill_solid(tx_configs[i].mLeds.data(), tx_configs[i].mLeds.size(), CRGB::Black);
    }
    FastLED.show();
    FL_WARN("[INFO] TX engine pre-initialized for " << chipset_name);

    // Run test patterns
    int total = 0;
    int passed = 0;

    // Test 1: Solid Red
    for (size_t i = 0; i < tx_configs.size(); i++) {
        fill_solid(tx_configs[i].mLeds.data(), tx_configs[i].mLeds.size(), CRGB::Red);
    }
    runTest("Solid Red", tx_configs, config, rx_channel, rx_buffer, total, passed);

    // Test 2: Solid Green
    for (size_t i = 0; i < tx_configs.size(); i++) {
        fill_solid(tx_configs[i].mLeds.data(), tx_configs[i].mLeds.size(), CRGB::Green);
    }
    runTest("Solid Green", tx_configs, config, rx_channel, rx_buffer, total, passed);

    // Test 3: Solid Blue
    for (size_t i = 0; i < tx_configs.size(); i++) {
        fill_solid(tx_configs[i].mLeds.data(), tx_configs[i].mLeds.size(), CRGB::Blue);
    }
    runTest("Solid Blue", tx_configs, config, rx_channel, rx_buffer, total, passed);

    // Report results
    FL_WARN("\nResults for " << chipset_name << ": " << passed << "/" << total << " tests passed");

    if (passed == total) {
        FL_WARN("[PASS] " << chipset_name << " validation successful");
    } else {
        FL_WARN("[FAIL] " << chipset_name << " validation failed");
    }

    // Destroy ALL channels before testing next timing configuration
    for (auto& ch : channels) {
        ch.reset();
    }
    FL_WARN("[INFO] All " << channels.size() << " channel(s) destroyed for " << chipset_name);

    delay(1000);
}

// Main validation test orchestrator
// Discovers drivers, iterates through them, and runs all chipset timing tests
void runAllValidationTests(int pin_data,
                           int pin_rx,
                           fl::span<CRGB> leds,
                           fl::shared_ptr<fl::RmtRxChannel> rx_channel,
                           fl::span<uint8_t> rx_buffer,
                           int color_order) {
    FL_WARN("=== Using Runtime Channel API for Dynamic Driver Testing ===\n");
    FL_WARN("Initialization complete");
    FL_WARN("Discovering available drivers...\n");

    // Get all available drivers for this platform
    auto drivers = FastLED.getDriverInfo();
    FL_WARN("Found " << drivers.size() << " driver(s) available:");
    for (fl::size i = 0; i < drivers.size(); i++) {
        FL_WARN("  " << (i+1) << ". " << drivers[i].name.c_str()
                << " (priority: " << drivers[i].priority
                << ", enabled: " << (drivers[i].enabled ? "yes" : "no") << ")");
    }

    FL_WARN("\nStarting validation tests for each driver...\n");

    // Iterate through all available drivers
    for (fl::size i = 0; i < drivers.size(); i++) {
        const auto& driver = drivers[i];

        FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
        FL_WARN("║ TESTING DRIVER " << (i+1) << "/" << drivers.size() << ": " << driver.name.c_str() << " (priority: " << driver.priority << ")");
        FL_WARN("╚════════════════════════════════════════════════════════════════╝\n");

        // Set this driver as exclusive for testing
        if (!FastLED.setExclusiveDriver(driver.name.c_str())) {
            FL_WARN("[ERROR] Failed to set " << driver.name.c_str() << " as exclusive driver");
            continue;
        }
        FL_WARN(driver.name.c_str() << " driver enabled exclusively\n");

        // Create driver-specific validation configuration
        fl::ValidationConfig validation_config = [&]() {
            fl::string driver_name_str(driver.name.c_str());
            if (driver_name_str == "RMT") {
                return fl::ValidationConfig("RMT", false, pin_rx);  // RMT can use same pin for TX/RX
            } else if (driver_name_str == "SPI") {
                return fl::ValidationConfig("SPI", true, pin_rx);  // Needs jumper
            } else if (driver_name_str == "PARLIO") {
                return fl::ValidationConfig("PARLIO", true, pin_rx);  // Needs jumper
            } else {
                // Default: assume needs jumper (conservative)
                return fl::ValidationConfig(driver.name.c_str(), true, pin_rx);
            }
        }();

        FL_WARN("[CONFIG] Driver: " << validation_config.driver_name
                << ", Jumper required: " << (validation_config.requires_physical_jumper ? "yes" : "no") << "\n");

        // Create TX configuration template for validation tests
        // Timing will be overridden per test
        fl::ChannelConfig tx_config(pin_data, fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>(),
                                    leds, color_order);

        // Run validation tests with different chipset timings
        // Each test creates a channel, runs tests, then destroys the channel
        validateChipsetTiming(fl::makeTimingConfig<fl::TIMING_WS2812_800KHZ>(), "WS2812 Standard",
                            fl::span<const fl::ChannelConfig>(&tx_config, 1), validation_config,
                            rx_channel, rx_buffer);
        validateChipsetTiming(fl::makeTimingConfig<fl::TIMING_WS2812B_V5>(), "WS2812B-V5",
                            fl::span<const fl::ChannelConfig>(&tx_config, 1), validation_config,
                            rx_channel, rx_buffer);
        validateChipsetTiming(fl::makeTimingConfig<fl::TIMING_SK6812>(), "SK6812",
                            fl::span<const fl::ChannelConfig>(&tx_config, 1), validation_config,
                            rx_channel, rx_buffer);

        FL_WARN("\n[INFO] All timing tests complete for " << driver.name.c_str() << " driver");
    }

    FL_WARN("\n╔════════════════════════════════════════════════════════════════╗");
    FL_WARN("║ ALL VALIDATION TESTS COMPLETE                                  ║");
    FL_WARN("╚════════════════════════════════════════════════════════════════╝");
}

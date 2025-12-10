// ValidationTest.cpp - Generic LED validation testing infrastructure
// Driver-agnostic test function implementations

#include "ValidationTest.h"
#include <FastLED.h>

// Capture transmitted LED data via RX loopback
// - rx_channel: Shared pointer to RX device (persistent across calls)
// - rx_buffer: Buffer to store received bytes
// Returns number of bytes captured, or 0 on error
size_t capture(fl::shared_ptr<fl::RxDevice> rx_channel, fl::span<uint8_t> rx_buffer) {
    if (!rx_channel) {
        FL_ERROR("RX channel is null");
        return 0;
    }

    // Clear RX buffer
    fl::memset(rx_buffer.data(), 0, rx_buffer.size());

    // Prepare RX config (but don't arm yet to avoid locking TX resources)
    fl::RxConfig rx_config;  // Use default WS2812B-compatible settings
    rx_config.hz = 40000000; // 40MHz for high-precision LED timing capture
    // Calculate buffer size needed: For WS2812B, each LED requires 24 bits = 24 symbols
    // rx_buffer holds decoded bytes (3 bytes per LED), so buffer_size = rx_buffer.size() * 8 symbols
    rx_config.buffer_size = rx_buffer.size() * 8;  // Convert bytes to symbols (1 byte = 8 bits = 8 symbols)

    // Immediately arm RX to capture the transmission (TX is already started)
    if (!rx_channel->begin(rx_config)) {
        FL_ERROR("Failed to arm RX receiver");
        return 0;
    }

    FastLED.show();



    // Wait for RX completion
    auto wait_result = rx_channel->wait(100);
    if (wait_result != fl::RxWaitResult::SUCCESS) {
        FL_ERROR("RX wait failed (timeout or no data received)");
        FL_WARN("");
        FL_WARN("⚠️  TROUBLESHOOTING:");
        FL_WARN("   1. If using non-RMT TX (SPI/ParallelIO): Connect physical jumper wire from TX GPIO to RX GPIO " << rx_channel->getPin());
        FL_WARN("   2. Internal loopback (io_loop_back) only works for RMT TX → RMT RX");
        FL_WARN("   3. ESP32 GPIO matrix cannot route other peripheral outputs to RMT input");
        FL_WARN("   4. Check that TX and RX use the same GPIO pin number (RX pin: " << rx_channel->getPin() << ")");
        FL_WARN("");
        return 0;
    }

    // ========================================================================
    // RAW EDGE TIMING INSPECTION (Debug: Validate encoder output)
    // ========================================================================
    // This inspection helps determine if the RMT encoder is producing correct
    // symbols or if it's outputting constant timing (which would decode to all zeros)

    FL_WARN("\n[RAW EDGE TIMING] Inspecting first 32 edge transitions:");

    fl::EdgeTime edges[32];  // Capture first 32 edges (enough for ~2 WS2812B bits)
    size_t edge_count = rx_channel->getRawEdgeTimes(fl::span<fl::EdgeTime>(edges, 32));

    if (edge_count == 0) {
        FL_WARN("[RAW EDGE TIMING] WARNING: No edge data captured!");
    } else {
        FL_WARN("[RAW EDGE TIMING] Captured " << edge_count << " edges:");

        // Display first few edges with their timing
        size_t display_limit = edge_count < 16 ? edge_count : 16;
        for (size_t i = 0; i < display_limit; i++) {
            const char* level = edges[i].high ? "HIGH" : "LOW ";
            FL_WARN("  Edge[" << i << "]: " << level << " " << edges[i].ns << "ns");
        }

        if (edge_count > display_limit) {
            FL_WARN("  ... (" << (edge_count - display_limit) << " more edges not shown)");
        }

        // Analyze timing patterns to detect if encoder is working
        // For WS2812B: Bit 0 = ~400ns HIGH + ~850ns LOW, Bit 1 = ~800ns HIGH + ~450ns LOW
        // If we see all edges with identical timing, the encoder is broken
        // If we see varying timings around expected values, encoder is working

        bool has_short_high = false;  // ~400ns (bit 0)
        bool has_long_high = false;   // ~800ns (bit 1)
        bool has_short_low = false;   // ~450ns (bit 1)
        bool has_long_low = false;    // ~850ns (bit 0)

        for (size_t i = 0; i < edge_count; i++) {
            uint32_t ns = edges[i].ns;
            if (edges[i].high) {
                // High pulse
                if (ns >= 250 && ns <= 550) has_short_high = true;  // Bit 0 high (~400ns ±150ns)
                if (ns >= 650 && ns <= 950) has_long_high = true;   // Bit 1 high (~800ns ±150ns)
            } else {
                // Low pulse
                if (ns >= 300 && ns <= 600) has_short_low = true;   // Bit 1 low (~450ns ±150ns)
                if (ns >= 700 && ns <= 1000) has_long_low = true;   // Bit 0 low (~850ns ±150ns)
            }
        }

        FL_WARN("\n[RAW EDGE TIMING] Pattern Analysis:");
        FL_WARN("  Short HIGH (~400ns, Bit 0): " << (has_short_high ? "FOUND ✓" : "MISSING ✗"));
        FL_WARN("  Long HIGH  (~800ns, Bit 1): " << (has_long_high ? "FOUND ✓" : "MISSING ✗"));
        FL_WARN("  Short LOW  (~450ns, Bit 1): " << (has_short_low ? "FOUND ✓" : "MISSING ✗"));
        FL_WARN("  Long LOW   (~850ns, Bit 0): " << (has_long_low ? "FOUND ✓" : "MISSING ✗"));

        if (has_short_high && has_long_high && has_short_low && has_long_low) {
            FL_WARN("\n[RAW EDGE TIMING] ✓ Encoder appears to be working correctly (varied timing patterns)");
        } else if (!has_short_high && !has_long_high) {
            FL_ERROR("[RAW EDGE TIMING] ✗ ENCODER BROKEN: No valid HIGH pulses detected!");
            FL_ERROR("[RAW EDGE TIMING]   Possible causes:");
            FL_ERROR("[RAW EDGE TIMING]   1. Encoder not reading pixel buffer data");
            FL_ERROR("[RAW EDGE TIMING]   2. Bytes encoder state machine stuck");
            FL_ERROR("[RAW EDGE TIMING]   3. Data pointer not passed correctly to encoder");
        } else if (!has_short_low && !has_long_low) {
            FL_ERROR("[RAW EDGE TIMING] ✗ ENCODER BROKEN: No valid LOW pulses detected!");
        } else {
            FL_WARN("[RAW EDGE TIMING] ⚠ Partial pattern match - encoder may have issues");
        }
    }
    FL_WARN("");

    // Decode received data directly into rx_buffer
    // Create 4-phase RX timing from WS2812B 3-phase TX timing
    fl::ChipsetTiming ws2812b_tx{fl::TIMING_WS2812_800KHZ::T1, fl::TIMING_WS2812_800KHZ::T2,
                                  fl::TIMING_WS2812_800KHZ::T3, fl::TIMING_WS2812_800KHZ::RESET, "WS2812B"};
    auto rx_timing = fl::make4PhaseTiming(ws2812b_tx, 150);

    auto decode_result = rx_channel->decode(rx_timing, rx_buffer);

    if (!decode_result.ok()) {
        FL_ERROR("Decode failed (error code: " << static_cast<int>(decode_result.error()) << ")");
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
            FL_ERROR("RX channel is null - must be created in .ino and passed via ValidationConfig");
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
                FL_ERROR("Mismatch on LED[" << static_cast<int>(i)
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
            FL_ERROR("Failed to create channel " << i << " (pin " << config.tx_configs[i].pin << ") - platform not supported");
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
        FL_ERROR(config.timing_name << " validation failed");
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

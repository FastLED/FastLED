// ValidationTest.cpp - Generic LED validation testing infrastructure
// Driver-agnostic test function implementations

#include "ValidationTest.h"
#include <FastLED.h>

// Capture transmitted LED data via RX loopback
// - rx_channel: Shared pointer to RX device (persistent across calls)
// - rx_buffer: Buffer to store received bytes
// Returns number of bytes captured, or 0 on error
size_t capture(fl::shared_ptr<fl::RxDevice> rx_channel, fl::span<uint8_t> rx_buffer, const fl::ChipsetTimingConfig& timing) {
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

    // Arm RX BEFORE starting TX transmission
    // Note: Per ESP-IDF docs, RX will wait for first edge before capturing
    // Pre-stabilization frame (sent earlier) ensures GPIO is in stable LOW state
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
        // Calculate expected timings from config (3-phase to 4-phase conversion)
        // Bit 0: T0H = T1, T0L = T2 + T3
        // Bit 1: T1H = T1 + T2, T1L = T3
        uint32_t expected_bit0_high = timing.t1_ns;
        uint32_t expected_bit0_low = timing.t2_ns + timing.t3_ns;
        uint32_t expected_bit1_high = timing.t1_ns + timing.t2_ns;
        uint32_t expected_bit1_low = timing.t3_ns;

        // Use ±150ns tolerance for pattern matching
        const uint32_t tolerance = 150;

        bool has_short_high = false;  // Bit 0 high (T1)
        bool has_long_high = false;   // Bit 1 high (T1+T2)
        bool has_short_low = false;   // Bit 1 low (T3)
        bool has_long_low = false;    // Bit 0 low (T2+T3)

        for (size_t i = 0; i < edge_count; i++) {
            uint32_t ns = edges[i].ns;
            if (edges[i].high) {
                // High pulse
                if (ns >= expected_bit0_high - tolerance && ns <= expected_bit0_high + tolerance)
                    has_short_high = true;
                if (ns >= expected_bit1_high - tolerance && ns <= expected_bit1_high + tolerance)
                    has_long_high = true;
            } else {
                // Low pulse
                if (ns >= expected_bit1_low - tolerance && ns <= expected_bit1_low + tolerance)
                    has_short_low = true;
                if (ns >= expected_bit0_low - tolerance && ns <= expected_bit0_low + tolerance)
                    has_long_low = true;
            }
        }

        FL_WARN("\n[RAW EDGE TIMING] Pattern Analysis:");
        FL_WARN("  Short HIGH (~" << expected_bit0_high << "ns, Bit 0): " << (has_short_high ? "FOUND ✓" : "MISSING ✗"));
        FL_WARN("  Long HIGH  (~" << expected_bit1_high << "ns, Bit 1): " << (has_long_high ? "FOUND ✓" : "MISSING ✗"));
        FL_WARN("  Short LOW  (~" << expected_bit1_low << "ns, Bit 1): " << (has_short_low ? "FOUND ✓" : "MISSING ✗"));
        FL_WARN("  Long LOW   (~" << expected_bit0_low << "ns, Bit 0): " << (has_long_low ? "FOUND ✓" : "MISSING ✗"));

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
    // Create 4-phase RX timing from the passed 3-phase TX timing
    fl::ChipsetTiming tx_timing{timing.t1_ns, timing.t2_ns, timing.t3_ns, timing.reset_us, timing.name};
    auto rx_timing = fl::make4PhaseTiming(tx_timing, 150);

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
    // Multi-lane limitation: Only validate Lane 0 (first channel)
    // Hardware constraint: Only one TX channel can be read from via RX loopback
    size_t channels_to_validate = config.tx_configs.size() > 1 ? 1 : config.tx_configs.size();

    if (config.tx_configs.size() > 1) {
        FL_WARN("\n[MULTI-LANE] Testing " << config.tx_configs.size() << " lanes, validating Lane 0 only (hardware limitation)");
    }

    // Validate enabled configs (Lane 0 only for multi-lane)
    for (size_t config_idx = 0; config_idx < channels_to_validate; config_idx++) {
        total++;

        // Build test context for detailed error reporting
        const auto& leds = config.tx_configs[config_idx].mLeds;
        size_t num_leds = leds.size();

        fl::TestContext ctx{
            config.driver_name,
            config.timing_name,
            fl::toString(config.rx_type),
            test_name,
            static_cast<int>(config.tx_configs.size()),
            static_cast<int>(config_idx),
            config.base_strip_size,
            static_cast<int>(num_leds),
            config.tx_configs[config_idx].pin
        };

        FL_WARN("\n=== " << test_name << " [Lane " << config_idx << "/" << config.tx_configs.size()
                << ", Pin " << config.tx_configs[config_idx].pin
                << ", LEDs " << config.tx_configs[config_idx].mLeds.size() << "] ===");

        // Use RX channel provided via config (created in .ino file, never created dynamically here)
        if (!config.rx_channel) {
            FL_ERROR("[" << ctx.driver_name << "/" << ctx.timing_name << "/" << ctx.pattern_name
                    << " | Lane " << ctx.lane_index << "/" << ctx.lane_count
                    << " (Pin " << ctx.pin_number << ", " << ctx.num_leds << " LEDs) | RX:" << ctx.rx_type_name << "] "
                    << "RX channel is null - must be created in .ino and passed via ValidationConfig");
            FL_ERROR("Result: FAIL ✗ (RX channel not provided)");
            continue;
        }

        size_t bytes_captured = capture(config.rx_channel, config.rx_buffer, config.timing);

        if (bytes_captured == 0) {
            FL_ERROR("[" << ctx.driver_name << "/" << ctx.timing_name << "/" << ctx.pattern_name
                    << " | Lane " << ctx.lane_index << "/" << ctx.lane_count
                    << " (Pin " << ctx.pin_number << ", " << ctx.num_leds << " LEDs) | RX:" << ctx.rx_type_name << "] "
                    << "Result: FAIL ✗ (capture failed)");
            continue;
        }

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
                FL_ERROR("[" << ctx.driver_name << "/" << ctx.timing_name << "/" << ctx.pattern_name
                        << " | Lane " << ctx.lane_index << "/" << ctx.lane_count
                        << " (Pin " << ctx.pin_number << ", " << ctx.num_leds << " LEDs) | RX:" << ctx.rx_type_name << "] "
                        << "Incomplete data for LED[" << static_cast<int>(i)
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
                FL_ERROR("[" << ctx.driver_name << "/" << ctx.timing_name << "/" << ctx.pattern_name
                        << " | Lane " << ctx.lane_index << "/" << ctx.lane_count
                        << " (Pin " << ctx.pin_number << ", " << ctx.num_leds << " LEDs) | RX:" << ctx.rx_type_name << "] "
                        << "LED[" << static_cast<int>(i) << "] mismatch: "
                        << "expected RGB(" << static_cast<int>(expected_r) << ","
                        << static_cast<int>(expected_g) << "," << static_cast<int>(expected_b)
                        << ") got RGB(" << static_cast<int>(actual_r) << ","
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
            FL_ERROR("[" << ctx.driver_name << "/" << ctx.timing_name << "/" << ctx.pattern_name
                    << " | Lane " << ctx.lane_index << "/" << ctx.lane_count
                    << " (Pin " << ctx.pin_number << ", " << ctx.num_leds << " LEDs) | RX:" << ctx.rx_type_name << "] "
                    << "Result: FAIL ✗");
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

    // Run test patterns (mixed bit patterns to test MSB vs LSB handling)
    int total = 0;
    int passed = 0;

    // Test all 4 bit patterns (0-3)
    for (int pattern_id = 0; pattern_id < 4; pattern_id++) {
        // Apply pattern to all lanes
        for (size_t i = 0; i < config.tx_configs.size(); i++) {
            setMixedBitPattern(
                config.tx_configs[i].mLeds.data(),
                config.tx_configs[i].mLeds.size(),
                pattern_id
            );
        }
        runTest(getBitPatternName(pattern_id), config, total, passed);
    }

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

// Set mixed RGB bit patterns to test MSB vs LSB handling
void setMixedBitPattern(CRGB* leds, size_t count, int pattern_id) {
    switch (pattern_id) {
        case 0:
            // Pattern A: R=0xF0 (high nibble), G=0x0F (low nibble), B=0xAA (alternating)
            // Tests: High bits in R, low bits in G, mixed bits in B
            for (size_t i = 0; i < count; i++) {
                leds[i] = CRGB(0xF0, 0x0F, 0xAA);
            }
            break;

        case 1:
            // Pattern B: R=0x55 (alternating 01010101), G=0xFF (all high), B=0x00 (all low)
            // Tests: Alternating bits, all-high boundary, all-low boundary
            for (size_t i = 0; i < count; i++) {
                leds[i] = CRGB(0x55, 0xFF, 0x00);
            }
            break;

        case 2:
            // Pattern C: R=0x0F (low nibble), G=0xAA (alternating), B=0xF0 (high nibble)
            // Tests: Rotated pattern from A, ensures driver handles different channel values
            for (size_t i = 0; i < count; i++) {
                leds[i] = CRGB(0x0F, 0xAA, 0xF0);
            }
            break;

        case 3:
            // Pattern D: Solid colors alternating (Red, Green, Blue repeating)
            // Baseline test - ensures basic RGB transmission works
            for (size_t i = 0; i < count; i++) {
                int color_index = i % 3;
                if (color_index == 0) {
                    leds[i] = CRGB::Red;     // RGB(255, 0, 0)
                } else if (color_index == 1) {
                    leds[i] = CRGB::Green;   // RGB(0, 255, 0)
                } else {
                    leds[i] = CRGB::Blue;    // RGB(0, 0, 255)
                }
            }
            break;

        default:
            // Fallback: all black
            fill_solid(leds, count, CRGB::Black);
            break;
    }
}

// Get name of bit pattern for logging
const char* getBitPatternName(int pattern_id) {
    switch (pattern_id) {
        case 0: return "Pattern A (R=0xF0, G=0x0F, B=0xAA)";
        case 1: return "Pattern B (R=0x55, G=0xFF, B=0x00)";
        case 2: return "Pattern C (R=0x0F, G=0xAA, B=0xF0)";
        case 3: return "Pattern D (RGB Solid Alternating)";
        default: return "Unknown Pattern";
    }
}

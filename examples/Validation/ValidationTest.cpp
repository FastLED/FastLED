// ValidationTest.cpp - Generic LED validation testing infrastructure
// Driver-agnostic test function implementations

#include "ValidationConfig.h"  // Must be included first to set config macros
#include "ValidationTest.h"
#include <FastLED.h>
#include "fl/stl/sstream.h"

// Phase 0: Include PARLIO debug instrumentation
#if defined(ESP32) && FASTLED_ESP32_HAS_PARLIO
#include "platforms/esp/32/drivers/parlio/channel_engine_parlio.h"
#endif

/// @brief Dump raw edge timing data to console for debugging
/// @param rx_channel RX device to read edge data from
/// @param timing Chipset timing configuration for pattern analysis
/// @param range Edge range to print (offset, count)
void dumpRawEdgeTiming(fl::shared_ptr<fl::RxDevice> rx_channel,
                       const fl::ChipsetTimingConfig& timing,
                       fl::EdgeRange range) {
    if (!rx_channel) {
        FL_WARN("[RAW EDGE TIMING] ERROR: RX channel is null");
        return;
    }

    // Allocate edge buffer sized to requested count (max 256 to avoid stack overflow)
    fl::FixedVector<fl::EdgeTime, 256> edges;
    size_t buffer_size = range.count < 256 ? range.count : 256;
    edges.resize(buffer_size);  // Default initializes to EdgeTime()

    // Get edges starting at offset
    size_t edge_count = rx_channel->getRawEdgeTimes(edges, range.offset);

    if (edge_count == 0) {
        FL_WARN("[RAW EDGE TIMING] WARNING: No edge data captured at offset " << range.offset);
        return;
    }

    // Calculate actual range printed (start from offset)
    size_t start_idx = range.offset;
    size_t end_idx = range.offset + edge_count;

    FL_WARN("[RAW EDGES " << start_idx << ".." << (end_idx - 1) << "]");

    // Display edges (edges buffer contains data starting from offset)
    for (size_t i = 0; i < edge_count; i++) {
        const char* level = edges[i].high ? "H" : "L";
        size_t absolute_index = start_idx + i;
        FL_WARN("  [" << absolute_index << "] " << level << " " << edges[i].ns);
    }

    // Pattern analysis (only if showing edges from start)
    if (range.offset == 0 && edge_count >= 16) {
        // Calculate expected timings from config (3-phase to 4-phase conversion)
        uint32_t expected_bit0_high = timing.t1_ns;
        uint32_t expected_bit0_low = timing.t2_ns + timing.t3_ns;
        uint32_t expected_bit1_high = timing.t1_ns + timing.t2_ns;
        uint32_t expected_bit1_low = timing.t3_ns;

        const uint32_t tolerance = 150;

        bool has_short_high = false, has_long_high = false;
        bool has_short_low = false, has_long_low = false;

        for (size_t i = 0; i < edge_count; i++) {
            uint32_t ns = edges[i].ns;
            if (edges[i].high) {
                if (ns >= expected_bit0_high - tolerance && ns <= expected_bit0_high + tolerance)
                    has_short_high = true;
                if (ns >= expected_bit1_high - tolerance && ns <= expected_bit1_high + tolerance)
                    has_long_high = true;
            } else {
                if (ns >= expected_bit1_low - tolerance && ns <= expected_bit1_low + tolerance)
                    has_short_low = true;
                if (ns >= expected_bit0_low - tolerance && ns <= expected_bit0_low + tolerance)
                    has_long_low = true;
            }
        }

        fl::sstream ss;
        ss << "\n[RAW EDGE TIMING] Pattern Analysis:\n";
        ss << "  Short HIGH (~" << expected_bit0_high << "ns, Bit 0): " << (has_short_high ? "FOUND ✓" : "MISSING ✗") << "\n";
        ss << "  Long HIGH  (~" << expected_bit1_high << "ns, Bit 1): " << (has_long_high ? "FOUND ✓" : "MISSING ✗") << "\n";
        ss << "  Short LOW  (~" << expected_bit1_low << "ns, Bit 1): " << (has_short_low ? "FOUND ✓" : "MISSING ✗") << "\n";
        ss << "  Long LOW   (~" << expected_bit0_low << "ns, Bit 0): " << (has_long_low ? "FOUND ✓" : "MISSING ✗");
        FL_WARN(ss.str());

        if (has_short_high && has_long_high && has_short_low && has_long_low) {
            FL_WARN("\n[RAW EDGE TIMING] ✓ Encoder appears to be working correctly (varied timing patterns)");
        } else if (!has_short_high && !has_long_high) {
            ss.clear();
            ss << "[RAW EDGE TIMING] ✗ ENCODER BROKEN: No valid HIGH pulses detected!\n";
            ss << "[RAW EDGE TIMING]   Possible causes:\n";
            ss << "[RAW EDGE TIMING]   1. Encoder not reading pixel buffer data\n";
            ss << "[RAW EDGE TIMING]   2. Bytes encoder state machine stuck\n";
            ss << "[RAW EDGE TIMING]   3. Data pointer not passed correctly to encoder";
            FL_ERROR(ss.str());
        } else if (!has_short_low && !has_long_low) {
            FL_ERROR("[RAW EDGE TIMING] ✗ ENCODER BROKEN: No valid LOW pulses detected!");
        } else {
            FL_WARN("[RAW EDGE TIMING] ⚠ Partial pattern match - encoder may have issues");
        }
    }
    FL_WARN("");
}

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

    // Wait for RX completion (150ms timeout for 3000 LEDs @ WS2812B timing)
    // WS2812B: ~30μs per LED → 3000 LEDs = 90ms minimum, use 150ms for safety
    auto wait_result = rx_channel->wait(150);

    // PARLIO debug metrics only shown on errors (controlled via PARLIO_DEBUG_VERBOSE)
    // To enable verbose PARLIO debugging, define PARLIO_DEBUG_VERBOSE in ValidationConfig.h

    if (wait_result != fl::RxWaitResult::SUCCESS) {
        FL_ERROR("RX wait failed (timeout or no data received)");
        fl::sstream ss;
        ss << "\n⚠️  TROUBLESHOOTING:\n";
        ss << "   1. If using non-RMT TX (SPI/ParallelIO): Connect physical jumper wire from TX GPIO to RX GPIO " << rx_channel->getPin() << "\n";
        ss << "   2. Internal loopback (io_loop_back) only works for RMT TX → RMT RX\n";
        ss << "   3. ESP32 GPIO matrix cannot route other peripheral outputs to RMT input\n";
        ss << "   4. Check that TX and RX use the same GPIO pin number (RX pin: " << rx_channel->getPin() << ")";
        FL_WARN(ss.str());
        return 0;
    }

    // Decode received data directly into rx_buffer
    // Create 4-phase RX timing from the passed 3-phase TX timing
    fl::ChipsetTiming tx_timing{timing.t1_ns, timing.t2_ns, timing.t3_ns, timing.reset_us, timing.name};
    auto rx_timing = fl::make4PhaseTiming(tx_timing, 150);

    // Enable gap tolerance for PARLIO DMA gaps (~20µs typical, 30µs safety margin)
    // Gaps between LED frames are tolerated, preventing false errors during buffer transitions
    rx_timing.gap_tolerance_ns = 30000; // 30µs

    auto decode_result = rx_channel->decode(rx_timing, rx_buffer);

    if (!decode_result.ok()) {
        FL_ERROR("Decode failed (error code: " << static_cast<int>(decode_result.error()) << ")");
        // Print raw edge timing on decode failure to diagnose the issue
        dumpRawEdgeTiming(rx_channel, timing, fl::EdgeRange(0, 256));
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

        // Phase 1 Iteration 9: Account for front padding in RX buffer
        // PARLIO TX sends: [front_pad (8 bytes)] + [LED_data] + [back_pad (8 bytes)] + [reset]
        // RX decoder captures EVERYTHING, so rx_buffer layout is:
        //   rx_buffer[0-7] = front padding (decoded to zeros)
        //   rx_buffer[8...] = actual LED data
        // We need to skip the first 8 bytes (1 Wave8Byte per lane) when validating
        const size_t front_padding_bytes = 8; // 1 Wave8Byte = 8 bytes per lane
        const size_t rx_buffer_offset = front_padding_bytes;

        if (bytes_captured > bytes_expected + front_padding_bytes) {
            FL_WARN("Info: Captured " << bytes_captured << " bytes ("
                    << front_padding_bytes << " front pad + "
                    << bytes_expected << " LED data + "
                    << (bytes_captured - bytes_expected - front_padding_bytes) << " back pad/RESET)");
        }

        // Validate: byte-level comparison (COLOR_ORDER is RGB, so no reordering)
        int mismatches = 0;
        size_t bytes_to_check = (bytes_captured < bytes_expected + rx_buffer_offset) ?
                                 (bytes_captured > rx_buffer_offset ? bytes_captured - rx_buffer_offset : 0) :
                                 bytes_expected;

        for (size_t i = 0; i < num_leds; i++) {
            size_t byte_offset = rx_buffer_offset + i * 3; // Skip front padding
            if (byte_offset + 2 >= bytes_captured) { // Check against total captured bytes
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
                // Per-LED error logging silenced for speed - errors tracked in mismatch count
                // JSON-RPC response will include mismatch summary
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

// Multi-run validation test runner
// Runs the same test multiple times and tracks errors across runs
void runMultiTest(const char* test_name,
                  fl::ValidationConfig& config,
                  const fl::MultiRunConfig& multi_config,
                  int& total, int& passed) {

    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ MULTI-RUN TEST: " << test_name << "\n";
    ss << "║ Runs: " << multi_config.num_runs << " | Print Mode: "
       << (multi_config.print_all_runs ? "All" : "Errors ONLY") << "\n";
    ss << "╚════════════════════════════════════════════════════════════════╝";
    FL_WARN(ss.str());

    fl::vector<fl::RunResult> run_results;

    // Multi-lane limitation: Only validate Lane 0
    size_t channels_to_validate = config.tx_configs.size() > 1 ? 1 : config.tx_configs.size();

    if (config.tx_configs.size() > 1) {
        FL_WARN("[MULTI-LANE] Testing " << config.tx_configs.size() << " lanes, validating Lane 0 only");
    }

    // Execute multiple runs
    for (int run = 1; run <= multi_config.num_runs; run++) {
        // Print progress to keep output flowing (prevents auto-exit timeout)
        if (run % 3 == 1 || multi_config.num_runs <= 5) {
            FL_WARN("[Run " << run << "/" << multi_config.num_runs << "] Testing...");
        }

        fl::RunResult result;
        result.run_number = run;

        // Validate Lane 0 only
        for (size_t config_idx = 0; config_idx < channels_to_validate; config_idx++) {
            const auto& leds = config.tx_configs[config_idx].mLeds;
            size_t num_leds = leds.size();
            result.total_leds = num_leds;

            // Capture RX data
            size_t bytes_captured = capture(config.rx_channel, config.rx_buffer, config.timing);

            if (bytes_captured == 0) {
                FL_ERROR("[Run " << run << "] Capture failed");
                result.passed = false;
                break;
            }

            // Validate pixel data
            int mismatches = 0;
            size_t bytes_expected = num_leds * 3;
            size_t bytes_to_check = bytes_captured < bytes_expected ? bytes_captured : bytes_expected;

            for (size_t i = 0; i < num_leds; i++) {
                size_t byte_offset = i * 3;
                if (byte_offset + 2 >= bytes_to_check) {
                    break;
                }

                uint8_t expected_r = leds[i].r;
                uint8_t expected_g = leds[i].g;
                uint8_t expected_b = leds[i].b;

                uint8_t actual_r = config.rx_buffer[byte_offset + 0];
                uint8_t actual_g = config.rx_buffer[byte_offset + 1];
                uint8_t actual_b = config.rx_buffer[byte_offset + 2];

                if (expected_r != actual_r || expected_g != actual_g || expected_b != actual_b) {
                    // Print corruption context for first mismatch only
                    if (mismatches == 0) {
                        FL_WARN("\n[CORRUPTION @ LED " << static_cast<int>(i) << ", Run " << run << "]");

                        // Calculate edge index and print timing around corruption point
                        size_t corruption_edge_index = i * 48;
                        size_t offset = corruption_edge_index > 4 ? corruption_edge_index - 4 : 0;

                        // Dump raw edge timing around corruption point (9 edges: -4 to +4)
                        dumpRawEdgeTiming(config.rx_channel, config.timing, fl::EdgeRange(offset, 9));
                    }

                    mismatches++;

                    // Store first N errors
                    if (result.errors.size() < static_cast<size_t>(multi_config.max_errors_per_run)) {
                        result.errors.push_back(fl::LEDError(
                            i, expected_r, expected_g, expected_b,
                            actual_r, actual_g, actual_b
                        ));
                    }
                }
            }

            result.mismatches = mismatches;
            result.passed = (mismatches == 0);
        }

        run_results.push_back(result);

        // Print run result if configured
        if (multi_config.print_all_runs || !result.passed) {
            FL_WARN("[Run " << run << "/" << multi_config.num_runs << "] "
                    << (result.passed ? "PASS" : "FAIL")
                    << " | Errors: " << result.mismatches << "/" << result.total_leds
                    << " (" << (100.0 * (result.total_leds - result.mismatches) / result.total_leds) << "%)");

            // Print error details if enabled
            if (!result.passed && multi_config.print_per_led_errors && !result.errors.empty()) {
                FL_WARN("  First " << result.errors.size() << " error(s):");
                for (size_t i = 0; i < result.errors.size(); i++) {
                    const auto& err = result.errors[i];
                    FL_WARN("    LED[" << err.led_index << "]: expected RGB("
                            << static_cast<int>(err.expected_r) << ","
                            << static_cast<int>(err.expected_g) << ","
                            << static_cast<int>(err.expected_b) << ") got RGB("
                            << static_cast<int>(err.actual_r) << ","
                            << static_cast<int>(err.actual_g) << ","
                            << static_cast<int>(err.actual_b) << ")");
                }
            }
        }
    }

    // Summary statistics
    int total_passed = 0;
    int total_failed = 0;
    for (const auto& r : run_results) {
        if (r.passed) total_passed++;
        else total_failed++;
    }

    ss.clear();
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ MULTI-RUN SUMMARY\n";
    ss << "╚════════════════════════════════════════════════════════════════╝\n";
    ss << "Total Runs:   " << multi_config.num_runs << "\n";
    ss << "Passed:       " << total_passed << " (" << (100.0 * total_passed / multi_config.num_runs) << "%)\n";
    ss << "Failed:       " << total_failed << " (" << (100.0 * total_failed / multi_config.num_runs) << "%)";
    FL_WARN(ss.str());

    if (total_failed > 0) {
        ss.clear();
        ss << "\nFailed Run Numbers:\n";
        for (const auto& r : run_results) {
            if (!r.passed) {
                ss << "  Run #" << r.run_number << " - " << r.mismatches << " errors\n";
                if (!r.errors.empty()) {
                    ss << "    First error at LED[" << r.errors[0].led_index << "]: "
                       << "expected RGB(" << static_cast<int>(r.errors[0].expected_r) << ","
                       << static_cast<int>(r.errors[0].expected_g) << ","
                       << static_cast<int>(r.errors[0].expected_b) << ") got RGB("
                       << static_cast<int>(r.errors[0].actual_r) << ","
                       << static_cast<int>(r.errors[0].actual_g) << ","
                       << static_cast<int>(r.errors[0].actual_b) << ")\n";
                }
            }
        }
        FL_WARN(ss.str());
    }

    // Update totals
    total++;
    if (total_failed == 0) {
        passed++;
        FL_WARN("\n[OVERALL] PASS ✓ - All " << multi_config.num_runs << " runs succeeded");
    } else {
        FL_ERROR("\n[OVERALL] FAIL ✗ - " << total_failed << "/" << multi_config.num_runs << " runs failed");
    }
}

// Validate a specific chipset timing configuration
// Creates channels, runs tests, destroys channels
void validateChipsetTiming(fl::ValidationConfig& config,
                           int& driver_total, int& driver_passed) {
    fl::sstream ss;
    ss << "\n========================================\n";
    ss << "Testing: " << config.timing_name << "\n";
    ss << "  T0H: " << config.timing.t1_ns << "ns\n";
    ss << "  T1H: " << (config.timing.t1_ns + config.timing.t2_ns) << "ns\n";
    ss << "  T0L: " << config.timing.t3_ns << "ns\n";
    ss << "  RESET: " << config.timing.reset_us << "us\n";
    ss << "  Channels: " << config.tx_configs.size() << "\n";
    ss << "========================================";
    FL_WARN(ss.str());

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
        // Channel creation logging silenced for speed
    }

    FastLED.setBrightness(255);

    // Pre-initialize the TX engine to avoid first-call setup delays
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        fill_solid(config.tx_configs[i].mLeds.data(), config.tx_configs[i].mLeds.size(), CRGB::Black);
    }
    FastLED.show();
    // TX engine pre-init logging silenced for speed

    // CRITICAL: Wait for PARLIO streaming transmission to complete before starting tests
    // Without this delay, the RX will capture the pre-initialization BLACK frame instead of the test pattern
    // PARLIO is a streaming engine with ring buffers - need time to drain the initial frame
    delay(5);  // Reduced from 100ms - just enough for buffer drain

    // Run test patterns (mixed bit patterns to test MSB vs LSB handling)
    int total = 0;
    int passed = 0;

    // Multi-run configuration - optimized for speed
    fl::MultiRunConfig multi_config;
    multi_config.num_runs = 1;            // Single run per pattern (Python orchestrates retries)
    multi_config.print_all_runs = false;  // Only print failed runs
    multi_config.print_per_led_errors = false;  // Errors reported via JSON-RPC
    multi_config.max_errors_per_run = 5;  // Store first 5 errors for JSON response

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
        runMultiTest(getBitPatternName(pattern_id), config, multi_config, total, passed);
    }

    // Results reported via JSON-RPC (verbose logging silenced for speed)

    // Accumulate results for driver-level tracking
    driver_total += total;
    driver_passed += passed;

    // Destroy ALL channels before testing next timing configuration
    for (auto& ch : channels) {
        ch.reset();
    }
    // Channel destruction is synchronous - no delay needed
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

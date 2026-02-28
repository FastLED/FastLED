// ValidationTest.cpp - Generic LED validation testing infrastructure
// Driver-agnostic test function implementations

#include "ValidationTest.h"
#include "LegacyClocklessProxy.h"
#include <FastLED.h>
#include "fl/stl/sstream.h"
#include "fl/chipsets/encoders/ucs7604.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/ease.h"
#include "pixel_controller.h"
#include "color.h"
#include "fl/stl/vector.h"
#include "fl/stl/iterator.h"

// Phase 0: Include PARLIO debug instrumentation
#if defined(ESP32) && FASTLED_ESP32_HAS_PARLIO
#include "platforms/esp/32/drivers/parlio/channel_driver_parlio.h"
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
            // Use FL_WARN to avoid triggering bash validate early exit
            FL_WARN("[RAW EDGE TIMING] ✗ ENCODER BROKEN: No valid LOW pulses detected!");
        } else {
            FL_WARN("[RAW EDGE TIMING] ⚠ Partial pattern match - encoder may have issues");
        }
    }
    FL_WARN("");
}

/// @brief Check if a timing config uses UCS7604 encoder
static bool isUCS7604(const fl::ChipsetTimingConfig& timing) {
    return timing.encoder == fl::CLOCKLESS_ENCODER_UCS7604_8BIT ||
           timing.encoder == fl::CLOCKLESS_ENCODER_UCS7604_16BIT ||
           timing.encoder == fl::CLOCKLESS_ENCODER_UCS7604_16BIT_1600;
}

/// @brief Build expected UCS7604 encoded bytes from LED data
/// @param leds LED data span
/// @param timing Timing config (contains encoder enum)
/// @return Vector of expected encoded bytes (preamble + padding + pixel data)
static fl::vector<uint8_t> buildExpectedUCS7604(fl::span<CRGB> leds, const fl::ChipsetTimingConfig& timing) {
    fl::vector<uint8_t> expected;

    // Map encoder to UCS7604Mode
    fl::UCS7604Mode mode;
    switch (timing.encoder) {
        case fl::CLOCKLESS_ENCODER_UCS7604_8BIT:
            mode = fl::UCS7604_MODE_8BIT_800KHZ;
            break;
        case fl::CLOCKLESS_ENCODER_UCS7604_16BIT:
            mode = fl::UCS7604_MODE_16BIT_800KHZ;
            break;
        case fl::CLOCKLESS_ENCODER_UCS7604_16BIT_1600:
            mode = fl::UCS7604_MODE_16BIT_1600KHZ;
            break;
        default:
            return expected;
    }

    // Default current control (0x0F for all channels) matching channel.cpp.hpp defaults
    fl::UCS7604CurrentControl current;  // defaults to 0xF for all channels

    // Create PixelIterator from LED data (RGB order, no color adjustment, no dithering)
    PixelController<RGB> pc(leds.data(), leds.size(), ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    auto pixel_iter = pc.as_iterator(RgbwInvalid());

    // For 16-bit modes, use default gamma 2.8
    fl::shared_ptr<const fl::Gamma8> gamma;
    bool use_gamma = (mode != fl::UCS7604_MODE_8BIT_800KHZ);
    if (use_gamma) {
        gamma = fl::Gamma8::getOrCreate(2.8f);
    }

    // Encode using the same function the driver uses
    fl::encodeUCS7604(pixel_iter, leds.size(), fl::back_inserter(expected),
                      mode, current, false /* is_rgbw */, gamma.get());

    return expected;
}

// UART wave8 decoder: Decodes raw RMT edge captures of UART-encoded LED data.
// ZERO HEAP ALLOCATION - uses only stack buffers for ESP32-C6 memory safety.
//
// UART at 3.2 Mbps transmits 10-bit frames (start + 8 data + stop) at 312.5ns per bit.
// Each UART byte encodes 2 LED bits via a 2-bit LUT:
//   0x11 → LED bits 00, 0x19 → LED bits 01, 0x91 → LED bits 10, 0x99 → LED bits 11
//
// The decoder uses a streaming edge cursor to determine signal level at any time
// without storing the complete edge timeline in memory.
//
// Returns number of decoded LED bytes, or 0 on error.
static size_t decodeUartWave8(fl::shared_ptr<fl::RxDevice> rx_channel, fl::span<uint8_t> rx_buffer) {
    // UART bit period at 4.0 Mbps = 250 ns (3.2 Mbps × 10/8 compensation)
    const uint32_t BIT_NS = 250;
    const uint32_t HALF_BIT_NS = BIT_NS / 2;
    const uint32_t FRAME_NS = BIT_NS * 10;  // 10 bits per UART frame = 2500ns = 2 LED bits

    // Edge cursor state - allows forward-only traversal of the edge stream.
    // We maintain a small window of edges and advance through the RMT RX buffer
    // without storing the entire timeline.
    static constexpr size_t WINDOW_SIZE = 64;
    fl::EdgeTime window[WINDOW_SIZE];
    size_t win_count = 0;         // edges currently in window
    size_t win_idx = 0;           // current position within window
    size_t rx_offset = 0;         // offset into RMT RX buffer for next read
    uint32_t edge_start_ns = 0;   // cumulative start time of current edge
    uint32_t edge_end_ns = 0;     // cumulative end time of current edge
    uint8_t edge_level = 0;       // level of current edge (0 or 1)
    bool edges_exhausted = false;

    // Count total edges first (single pass to get count for logging)
    size_t total_edges = 0;
    {
        fl::EdgeTime probe[64];
        size_t probe_off = 0;
        while (true) {
            size_t n = rx_channel->getRawEdgeTimes(fl::span<fl::EdgeTime>(probe, 64), probe_off);
            if (n == 0) break;
            total_edges += n;
            probe_off += n;
            if (n < 64) break;
        }
    }

    if (total_edges == 0) {
        FL_WARN("[UART DECODE] No raw edges captured");
        return 0;
    }

    FL_WARN("[UART DECODE] " << total_edges << " edges to decode");

    // Load first window of edges
    win_count = rx_channel->getRawEdgeTimes(fl::span<fl::EdgeTime>(window, WINDOW_SIZE), 0);
    rx_offset = win_count;
    win_idx = 0;
    if (win_count > 0) {
        edge_start_ns = 0;
        edge_end_ns = window[0].ns;
        edge_level = window[0].high ? 1 : 0;
    }

    // Advance the edge cursor to cover time t_ns.
    // Returns the signal level at time t_ns. Only forward movement is supported.
    auto advance_to = [&](uint32_t t_ns) -> uint8_t {
        while (t_ns >= edge_end_ns && !edges_exhausted) {
            win_idx++;
            if (win_idx >= win_count) {
                // Load next window
                win_count = rx_channel->getRawEdgeTimes(
                    fl::span<fl::EdgeTime>(window, WINDOW_SIZE), rx_offset);
                if (win_count == 0) {
                    edges_exhausted = true;
                    return edge_level;
                }
                rx_offset += win_count;
                win_idx = 0;
            }
            edge_start_ns = edge_end_ns;
            edge_end_ns = edge_start_ns + window[win_idx].ns;
            edge_level = window[win_idx].high ? 1 : 0;
        }
        return edge_level;
    };

    // Reverse LUT: UART byte → 2 LED bits (or -1 for invalid)
    auto decode_lut = [](uint8_t b) -> int {
        switch (b) {
            case 0x11: return 0;
            case 0x19: return 1;
            case 0x91: return 2;
            case 0x99: return 3;
            default:   return -1;
        }
    };

    // Scan forward through the edge stream to find first idle HIGH
    uint32_t scan_pos = 0;
    // Find an upper bound for total time from the edge count (rough estimate)
    // Each edge is at least 1 bit (312ns), so max time ~ total_edges * 1000ns
    const uint32_t MAX_SCAN_NS = total_edges * 1000;
    while (scan_pos < MAX_SCAN_NS) {
        uint8_t level = advance_to(scan_pos);
        if (level == 1) break;  // Found HIGH (idle)
        scan_pos += HALF_BIT_NS;
    }

    size_t led_bytes = 0;
    size_t decode_errors = 0;
    int max_frames = static_cast<int>(rx_buffer.size()) * 4 + 10;
    int frames_in_group = 0;
    uint8_t led_accum = 0;

    for (int frame = 0; frame < max_frames; frame++) {
        // Find next start bit (HIGH→LOW transition)
        bool found = false;
        uint32_t limit = scan_pos + BIT_NS * 3;
        while (scan_pos < limit) {
            if (edges_exhausted) break;
            uint8_t level = advance_to(scan_pos);
            if (level == 0) { found = true; break; }
            scan_pos += HALF_BIT_NS / 2;
        }
        if (!found) break;

        // Verify we have room for a full frame
        uint32_t frame_end = scan_pos + FRAME_NS;
        // Sample 8 data bits at center of each bit cell (LSB first)
        uint8_t byte_val = 0;
        for (int bit = 0; bit < 8; bit++) {
            uint32_t sample = scan_pos + BIT_NS + HALF_BIT_NS + bit * BIT_NS;
            if (edges_exhausted) break;
            if (advance_to(sample)) byte_val |= (1 << bit);
        }

        // Decode 2-bit LUT and accumulate into LED byte
        int pair = decode_lut(byte_val);
        if (pair < 0) {
            decode_errors++;
            if (decode_errors <= 3) {
                FL_WARN("[UART DECODE] LUT miss: frame " << frame
                        << " byte=0x" << fl::hex << static_cast<int>(byte_val) << fl::dec);
            }
            pair = 0;
        }

        // Pack: first UART byte = bits 7-6, second = bits 5-4, etc.
        led_accum = static_cast<uint8_t>((led_accum << 2) | pair);
        frames_in_group++;

        if (frames_in_group == 4) {
            if (led_bytes < rx_buffer.size()) {
                rx_buffer[led_bytes++] = led_accum;
            }
            led_accum = 0;
            frames_in_group = 0;
        }

        scan_pos = frame_end;
    }

    if (decode_errors > 0) {
        FL_WARN("[UART DECODE] " << led_bytes << " LED bytes, " << decode_errors << " LUT errors");
    } else {
        FL_WARN("[UART DECODE] " << led_bytes << " LED bytes decoded OK");
    }

    return led_bytes;
}

// Capture transmitted LED data via RX loopback
// - rx_channel: Shared pointer to RX device (persistent across calls)
// - rx_buffer: Buffer to store received bytes
// - timing: Chipset timing configuration for RX decoder
// - driver_name: Name of the TX driver being tested (e.g., "RMT", "PARLIO") - enables io_loop_back only for RMT
// Returns number of bytes captured, or 0 on error
size_t capture(fl::shared_ptr<fl::RxDevice> rx_channel, fl::span<uint8_t> rx_buffer, const fl::ChipsetTimingConfig& timing, const char* driver_name) {
    if (!rx_channel) {
        FL_ERROR("RX channel is null");
        return 0;
    }

    // Clear RX buffer
    fl::memset(rx_buffer.data(), 0, rx_buffer.size());

    // Prepare RX config (but don't arm yet to avoid locking TX resources)
    fl::RxConfig rx_config;  // Use default WS2812B-compatible settings
    rx_config.hz = 40000000; // 40MHz for high-precision LED timing capture

    // Buffer size depends on driver encoding:
    // - WS2812/RMT/PARLIO: 1 LED byte = 8 bits = 8 RMT symbols → buffer_size = bytes * 8
    // - UART wave8: 1 LED byte = 4 UART frames × ~10 edges = ~40 symbols → buffer_size = bytes * 40
    //   BUT capped to avoid memory exhaustion on ESP32-C6 (~320KB SRAM)
    bool is_uart_driver = (fl::strcmp(driver_name, "UART") == 0);
    if (is_uart_driver) {
        // UART: each LED byte → 4 UART bytes → ~40 RMT edges, but cap at 4096
        // to stay within memory budget. The RMT RX accumulation buffer gets clamped
        // to 25% of free heap anyway, so requesting more just wastes the mBufferSize check.
        size_t uart_symbols = rx_buffer.size() * 40;
        rx_config.buffer_size = uart_symbols < 4096 ? uart_symbols : 4096;
    } else {
        rx_config.buffer_size = rx_buffer.size() * 8;  // Convert bytes to symbols (1 byte = 8 bits = 8 symbols)
    }

    // Internal loopback configuration: Enable ONLY for RMT TX -> RMT RX scenarios
    // When driver_name == "RMT", enable io_loop_back to route RMT TX output to RMT RX internally
    // This is REQUIRED for ESP32-S3 because TX GPIO output stops when RX is active on different GPIO
    // For other drivers (PARLIO, SPI, UART, I2S), disable io_loop_back (use external GPIO wire)
    bool is_rmt_driver = (fl::strcmp(driver_name, "RMT") == 0);
    rx_config.io_loop_back = is_rmt_driver;
    if (is_rmt_driver) {
        FL_WARN("[CAPTURE] RMT TX -> RMT RX: Internal loopback enabled (io_loop_back=true)");
    } else {
        FL_WARN("[CAPTURE] " << driver_name << " TX -> RMT RX: External GPIO wire (io_loop_back=false)");
    }

    // Driver-aware capture strategy:
    // - RMT: Two-TX approach (ESP32-S3 workaround - TX GPIO blocked when RX active)
    // - PARLIO/SPI/other: Single-TX approach (arm RX first, then TX)

    // TX wait timeout: 1 second max per frame - prevents infinite hang if driver stalls
    // Even a 10000-LED strip at 800kbps takes only ~300ms, so 1s is very safe
    const uint32_t TX_WAIT_TIMEOUT_MS = 1000;

    if (is_rmt_driver) {
        // RMT: Two-TX approach for ESP32-S3 compatibility
        // First TX without RX armed (diagnostics), then arm RX, then second TX
        FL_WARN("[CAPTURE] RMT: Two-TX approach (ESP32-S3 workaround)");
        FastLED.show();
        if (!FastLED.wait(TX_WAIT_TIMEOUT_MS)) {
            FL_ERROR("[CAPTURE] TX wait timeout (pre-arm) - driver may be stalled");
            return 0;
        }

        // Arm RX for capture
        if (!rx_channel->begin(rx_config)) {
            FL_ERROR("Failed to arm RX receiver");
            return 0;
        }
        // Second TX with RX armed
        FastLED.show();
        if (!FastLED.wait(TX_WAIT_TIMEOUT_MS)) {
            FL_ERROR("[CAPTURE] TX wait timeout (capture) - driver may be stalled");
            return 0;
        }
    } else {
        // Non-RMT (PARLIO, SPI, etc.): Single-TX approach
        if (!rx_channel->begin(rx_config)) {
            FL_WARN("[CAPTURE] RX begin() failed for pin " << rx_channel->getPin());
            return 0;
        }
        FL_WARN("[CAPTURE] RX armed, calling FastLED.show()...");

        FastLED.show();
        FL_WARN("[CAPTURE] FastLED.show() returned, calling wait...");
        if (!FastLED.wait(TX_WAIT_TIMEOUT_MS)) {
            FL_WARN("[CAPTURE] FastLED.wait() timed out");
            return 0;
        }
        FL_WARN("[CAPTURE] FastLED.wait() done");
    }




    // Wait for RX completion
    // WS2812B: ~30μs per LED → 3000 LEDs = 90ms, use 150ms
    // UART: ~37.5μs per LED (4 UART bytes × 3.125μs × 3 channels) → 100 LEDs = ~11ms, use 500ms
    // UART needs longer timeout because RMT RX idle detection (signal_range_max_ns=100μs)
    // triggers end-of-reception after the last UART byte, which may take time to propagate
    const uint32_t rx_wait_ms = is_uart_driver ? 500 : 150;
    FL_WARN("[CAPTURE] Waiting for RX completion (" << rx_wait_ms << "ms timeout)...");
    auto wait_result = rx_channel->wait(rx_wait_ms);
    FL_WARN("[CAPTURE] RX wait returned: " << static_cast<int>(wait_result));

    if (wait_result != fl::RxWaitResult::SUCCESS) {
        if (is_uart_driver) {
            // UART: timeout is expected because buffer_size may exceed what RMT RX
            // can accumulate (memory-capped). Try decoding whatever edges were captured.
            FL_WARN("[CAPTURE] RX wait timed out for UART - will attempt decode with captured edges");
        } else {
            FL_ERROR("RX wait failed (timeout or no data received)");
            fl::sstream ss;
            ss << "\n⚠️  TROUBLESHOOTING:\n";
            ss << "   1. Connect physical jumper wire from TX GPIO to RX GPIO " << rx_channel->getPin() << "\n";
            ss << "   2. Check that both TX and RX pins are correctly configured\n";
            ss << "   3. Verify the GPIO connection is working (GPIO baseline test should pass)\n";
            ss << "   4. For RMT TX → RMT RX: Ensure io_loop_back=true in RxConfig";
            FL_WARN(ss.str());
            return 0;
        }
    }

    // UART uses wave8 encoding with UART framing (start/stop bits), which creates
    // a fundamentally different waveform than standard WS2812 signaling.
    // Use UART-specific decoder that reconstructs UART bytes from raw edges,
    // then reverses the 2-bit LUT to recover LED pixel data.
    if (is_uart_driver) {
        FL_WARN("[CAPTURE] Using UART wave8 decoder...");
        size_t decoded = decodeUartWave8(rx_channel, rx_buffer);
        FL_WARN("[CAPTURE] UART decode result: " << decoded << " LED bytes");
        return decoded;
    }

    // Decode received data directly into rx_buffer
    // Create 4-phase RX timing from the TX timing
    //
    // SPI clockless driver uses wave8 encoding (8-bit expansion):
    //   SPI clock = 8 / (T1+T2+T3) Hz, each SPI bit = (T1+T2+T3)/8 ns
    //   Bit 0: round(T1/(T1+T2+T3)*8) HIGH pulses, rest LOW
    //   Bit 1: round((T1+T2)/(T1+T2+T3)*8) HIGH pulses, rest LOW
    // The actual pulse widths are quantized to SPI bit boundaries, so we must
    // compute the exact wave8 timing for RX decode thresholds.
    bool is_spi_driver = (fl::strcmp(driver_name, "SPI") == 0);
    fl::ChipsetTiming tx_timing;
    if (is_spi_driver) {
        // Compute actual wave8 timing from chipset timing
        const uint32_t period = timing.t1_ns + timing.t2_ns + timing.t3_ns;
        const uint32_t spi_bit_ns = period / 8;  // Each SPI bit duration
        // Wave8 LUT computes: pulses = round(fraction * 8)
        const uint32_t pulses_bit0 = static_cast<uint32_t>(
            static_cast<float>(timing.t1_ns) / period * 8.0f + 0.5f);
        const uint32_t pulses_bit1 = static_cast<uint32_t>(
            static_cast<float>(timing.t1_ns + timing.t2_ns) / period * 8.0f + 0.5f);
        // Convert back to 3-phase timing (T1=T0H, T2=T1H-T0H, T3=period-T1H)
        const uint32_t actual_t0h = pulses_bit0 * spi_bit_ns;
        const uint32_t actual_t1h = pulses_bit1 * spi_bit_ns;
        tx_timing = fl::ChipsetTiming{
            actual_t0h,                    // T1 = T0H
            actual_t1h - actual_t0h,       // T2 = T1H - T0H
            period - actual_t1h,           // T3 = period - T1H (using ideal period, not actual SPI)
            timing.reset_us,
            "SPI_wave8"
        };
        FL_WARN("[RX TIMING] SPI wave8: pulses_bit0=" << pulses_bit0
                << " pulses_bit1=" << pulses_bit1
                << " spi_bit_ns=" << spi_bit_ns
                << " -> T1=" << tx_timing.T1 << " T2=" << tx_timing.T2
                << " T3=" << tx_timing.T3);
    } else {
        tx_timing = fl::ChipsetTiming{timing.t1_ns, timing.t2_ns, timing.t3_ns, timing.reset_us, timing.name};
    }
    // SPI wave8 encoding has timing jitter due to clock quantization and GPIO matrix latency
    // Use wider tolerance for SPI (200ns) to accommodate SPI clock rounding
    // (e.g., requested 6.535MHz → actual 6.666MHz = ~2% faster)
    const uint32_t tolerance = is_spi_driver ? 200 : 170;
    auto rx_timing = fl::make4PhaseTiming(tx_timing, tolerance);

    // Enable gap tolerance for PARLIO/SPI DMA gaps
    // PARLIO: ~20µs typical gaps during buffer transitions
    // SPI: Can have longer inter-frame gaps due to software encoding timing
    // Increased to 100µs to accommodate SPI driver timing variations
    rx_timing.gap_tolerance_ns = 100000; // 100µs (was 30µs)

    FL_WARN("[CAPTURE] Decoding...");
    auto decode_result = rx_channel->decode(rx_timing, rx_buffer);

    if (!decode_result.ok()) {
        // Use FL_WARN instead of FL_ERROR to avoid triggering bash validate exit
        // This can happen during warmup/setup and is not fatal
        FL_WARN("Decode failed (error code: " << static_cast<int>(decode_result.error()) << ")");
        // Print raw edge timing on decode failure to diagnose the issue
        dumpRawEdgeTiming(rx_channel, timing, fl::EdgeRange(0, 256));
        return 0;
    }

    FL_WARN("[CAPTURE] Decoded " << decode_result.value() << " bytes");
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

        size_t bytes_captured = capture(config.rx_channel, config.rx_buffer, config.timing, config.driver_name);

        if (bytes_captured == 0) {
            FL_ERROR("[" << ctx.driver_name << "/" << ctx.timing_name << "/" << ctx.pattern_name
                    << " | Lane " << ctx.lane_index << "/" << ctx.lane_count
                    << " (Pin " << ctx.pin_number << ", " << ctx.num_leds << " LEDs) | RX:" << ctx.rx_type_name << "] "
                    << "Result: FAIL ✗ (capture failed)");
            continue;
        }

        int mismatches = 0;

        if (isUCS7604(config.timing)) {
            // UCS7604: Compare full encoded frame (preamble + padding + pixel data) byte-for-byte
            fl::vector<uint8_t> expected_encoded = buildExpectedUCS7604(
                config.tx_configs[config_idx].mLeds, config.timing);
            size_t expected_len = expected_encoded.size();

            FL_WARN("UCS7604 encoded comparison: expected " << expected_len << " bytes, captured " << bytes_captured);

            size_t compare_len = (bytes_captured < expected_len) ? bytes_captured : expected_len;
            for (size_t i = 0; i < compare_len; i++) {
                if (expected_encoded[i] != config.rx_buffer[i]) {
                    mismatches++;
                }
            }
            // Count any missing bytes as mismatches
            if (bytes_captured < expected_len) {
                mismatches += static_cast<int>(expected_len - bytes_captured);
            }

            FL_WARN("Bytes Captured: " << bytes_captured << " (expected: " << expected_len << ")");
            int total_bytes = static_cast<int>(expected_len);
            FL_WARN("Accuracy: " << (100.0 * (total_bytes - mismatches) / total_bytes) << "% ("
                    << (total_bytes - mismatches) << "/" << total_bytes << " bytes match)");
        } else {
            // WS2812: Compare raw RGB per-LED
            size_t bytes_expected = num_leds * 3;

            // NEVER EVER CHANGE THIS!!!!!
            const size_t front_padding_bytes = 0; // No front padding: PARLIO FRONT_PAD_BYTES=0
            const size_t rx_buffer_offset = front_padding_bytes;

            if (bytes_captured > bytes_expected + front_padding_bytes) {
                FL_WARN("Info: Captured " << bytes_captured << " bytes ("
                        << front_padding_bytes << " front pad + "
                        << bytes_expected << " LED data + "
                        << (bytes_captured - bytes_expected - front_padding_bytes) << " back pad/RESET)");
            }

            // Validate: byte-level comparison (COLOR_ORDER is RGB, so no reordering)
            size_t bytes_to_check = (bytes_captured < bytes_expected + rx_buffer_offset) ?
                                     (bytes_captured > rx_buffer_offset ? bytes_captured - rx_buffer_offset : 0) :
                                     bytes_expected;
            (void)bytes_to_check;

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
                    mismatches++;
                }
            }

            FL_WARN("Bytes Captured: " << bytes_captured << " (expected: " << bytes_expected << ")");
            FL_WARN("Accuracy: " << (100.0 * (num_leds - mismatches) / num_leds) << "% ("
                    << (num_leds - mismatches) << "/" << num_leds << " LEDs match)");
        }

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
                  int& total, int& passed,
                  fl::vector<fl::RunResult>* out_results) {

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
            result.totalBytes = num_leds * 3;

            // Capture RX data
            size_t bytes_captured = capture(config.rx_channel, config.rx_buffer, config.timing, config.driver_name);

            if (bytes_captured == 0) {
                FL_WARN("[Run " << run << "] Capture failed");
                result.passed = false;
                break;
            }

            // Validate pixel data
            int mismatches = 0;

            // DEBUG: Print first 24 bytes of captured data
            FL_WARN("[RUN " << run << "] Driver=" << config.driver_name << ", bytes_captured=" << bytes_captured);
            FL_WARN("[RUN " << run << "] First 24 bytes:");
            for (size_t i = 0; i < 24 && i < bytes_captured; i++) {
                FL_WARN("  [" << i << "] = 0x" << fl::hex << static_cast<int>(config.rx_buffer[i]) << fl::dec);
            }

            if (isUCS7604(config.timing)) {
                // UCS7604: Compare full encoded frame byte-for-byte
                fl::vector<uint8_t> expected_encoded = buildExpectedUCS7604(
                    config.tx_configs[config_idx].mLeds, config.timing);
                size_t expected_len = expected_encoded.size();
                result.totalBytes = static_cast<int>(expected_len);

                size_t compare_len = (bytes_captured < expected_len) ? bytes_captured : expected_len;
                for (size_t i = 0; i < compare_len; i++) {
                    if (expected_encoded[i] != config.rx_buffer[i]) {
                        result.mismatchedBytes++;
                        if ((expected_encoded[i] ^ config.rx_buffer[i]) == 0x01) {
                            result.lsbOnlyErrors++;
                        }
                        mismatches++;

                        // Print corruption context for first mismatch only
                        if (mismatches == 1) {
                            FL_WARN("\n[CORRUPTION @ byte " << static_cast<int>(i) << ", Run " << run
                                    << "] expected=0x" << fl::hex << static_cast<int>(expected_encoded[i])
                                    << " actual=0x" << static_cast<int>(config.rx_buffer[i]) << fl::dec);
                        }

                        // Store first N errors (using byte-level reporting)
                        if (result.errors.size() < static_cast<size_t>(multi_config.max_errors_per_run)) {
                            result.errors.push_back(fl::LEDError(
                                static_cast<int>(i), expected_encoded[i], 0, 0,
                                config.rx_buffer[i], 0, 0
                            ));
                        }
                    }
                }
                // Count missing bytes as mismatches
                if (bytes_captured < expected_len) {
                    mismatches += static_cast<int>(expected_len - bytes_captured);
                }
            } else {
                // WS2812: Per-LED RGB comparison
                size_t bytes_expected = num_leds * 3;

                // Determine front padding offset (PARLIO only)
                const size_t rx_buffer_offset = 0; // No front padding: PARLIO FRONT_PAD_BYTES=0

                size_t bytes_to_check = (bytes_captured < bytes_expected + rx_buffer_offset) ?
                                         (bytes_captured > rx_buffer_offset ? bytes_captured - rx_buffer_offset : 0) :
                                         bytes_expected;
                (void)bytes_to_check;

                for (size_t i = 0; i < num_leds; i++) {
                    size_t byte_offset = rx_buffer_offset + i * 3; // Apply offset for PARLIO front padding
                    if (byte_offset + 2 >= bytes_captured) { // Check against total captured bytes
                        break;
                    }

                    uint8_t expected_r = leds[i].r;
                    uint8_t expected_g = leds[i].g;
                    uint8_t expected_b = leds[i].b;

                    uint8_t actual_r = config.rx_buffer[byte_offset + 0];
                    uint8_t actual_g = config.rx_buffer[byte_offset + 1];
                    uint8_t actual_b = config.rx_buffer[byte_offset + 2];

                    // Per-byte comparison for byte-level stats
                    uint8_t exp_bytes[3] = {expected_r, expected_g, expected_b};
                    uint8_t act_bytes[3] = {actual_r, actual_g, actual_b};
                    bool led_mismatch = false;
                    for (int ch = 0; ch < 3; ch++) {
                        if (exp_bytes[ch] != act_bytes[ch]) {
                            result.mismatchedBytes++;
                            if ((exp_bytes[ch] ^ act_bytes[ch]) == 0x01) {
                                result.lsbOnlyErrors++;
                            }
                            led_mismatch = true;
                        }
                    }

                    if (led_mismatch) {
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

    // Copy results to caller if requested
    if (out_results) {
        for (const auto& r : run_results) {
            out_results->push_back(r);
        }
    }

    // Update totals
    total++;
    if (total_failed == 0) {
        passed++;
        FL_WARN("\n[OVERALL] PASS ✓ - All " << multi_config.num_runs << " runs succeeded");
    } else {
        FL_WARN("\n[OVERALL] FAIL ✗ - " << total_failed << "/" << multi_config.num_runs << " runs failed");
    }
}

// Validate a specific chipset timing configuration
// Creates channels, runs tests, destroys channels
void validateChipsetTiming(fl::ValidationConfig& config,
                           int& driver_total, int& driver_passed,
                           uint32_t& out_show_duration_ms,
                           fl::vector<fl::RunResult>* out_results) {
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

        auto channel = FastLED.add(channel_config);
        if (!channel) {
            FL_ERROR("Failed to create channel " << i << " (pin " << config.tx_configs[i].pin << ") - platform not supported");
            // Clean up previously created channels
            for (auto& ch : channels) {
                ch.reset();
            }
            return;
        }
        channels.push_back(channel);
    }

    FastLED.setBrightness(255);

    // Pre-initialize the TX engine to avoid first-call setup delays
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        fill_solid(config.tx_configs[i].mLeds.data(), config.tx_configs[i].mLeds.size(), CRGB::Black);
    }
    FastLED.show();
    if (!FastLED.wait(1000)) {  // 1s timeout - driver stall guard
        FL_ERROR("[PREINIT] TX wait timeout - driver may be stalled on this platform");
        FastLED.clear(ClearFlags::CHANNELS);
        return;
    }
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
    multi_config.max_errors_per_run = 10;  // Store first 10 errors for JSON response

    // Measure show-only duration (excludes setup/teardown overhead)
    uint32_t show_start_ms = millis();

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
        runMultiTest(getBitPatternName(pattern_id), config, multi_config, total, passed, out_results);
    }

    out_show_duration_ms += millis() - show_start_ms;

    // Results reported via JSON-RPC (verbose logging silenced for speed)

    // Accumulate results for driver-level tracking
    driver_total += total;
    driver_passed += passed;

    // Destroy ALL channels before testing next timing configuration
    // CRITICAL: Clear FastLED's global channel registry to prevent accumulation
    // If we only destroy local shared_ptrs, FastLED still holds references
    FastLED.clear(ClearFlags::CHANNELS);
    // Channel destruction is synchronous - no delay needed
}

// Validate using the legacy template addLeds API (single-lane only)
// Nearly identical to validateChipsetTiming() — only channel creation differs:
//   Normal:  FastLED.add(channel_config) → Channel
//   Legacy:  LegacyClocklessProxy(pin, leds, numLeds) → WS2812B<PIN> → ClocklessIdf5 → Channel
void validateChipsetTimingLegacy(fl::ValidationConfig& config,
                                 int& driver_total, int& driver_passed,
                                 uint32_t& out_show_duration_ms,
                                 fl::vector<fl::RunResult>* out_results) {
    // Legacy API only supports single-lane
    if (config.tx_configs.size() != 1) {
        FL_ERROR("Legacy API validation requires exactly 1 lane, got " << config.tx_configs.size());
        return;
    }

    fl::sstream ss;
    ss << "\n========================================\n";
    ss << "Testing (LEGACY API): " << config.timing_name << "\n";
    ss << "  T0H: " << config.timing.t1_ns << "ns\n";
    ss << "  T1H: " << (config.timing.t1_ns + config.timing.t2_ns) << "ns\n";
    ss << "  T0L: " << config.timing.t3_ns << "ns\n";
    ss << "  RESET: " << config.timing.reset_us << "us\n";
    ss << "  Pin: " << config.tx_configs[0].pin << "\n";
    ss << "  LEDs: " << config.tx_configs[0].mLeds.size() << "\n";
    ss << "========================================";
    FL_WARN(ss.str());

    int pin = config.tx_configs[0].pin;
    CRGB* leds = config.tx_configs[0].mLeds.data();
    int numLeds = static_cast<int>(config.tx_configs[0].mLeds.size());

    // Create legacy controller via proxy (maps runtime pin to WS2812B<PIN> template)
    LegacyClocklessProxy proxy(pin, leds, numLeds);
    if (!proxy.valid()) {
        FL_ERROR("Legacy proxy invalid (pin " << pin << " out of range 0-8)");
        return;
    }

    FastLED.setBrightness(255);

    // Pre-initialize the TX engine to avoid first-call setup delays
    // (same as validateChipsetTiming)
    fill_solid(leds, numLeds, CRGB::Black);
    FastLED.show();
    if (!FastLED.wait(1000)) {
        FL_ERROR("[LEGACY] TX wait timeout - driver may be stalled");
        return;  // proxy destructor cleans up
    }

    delay(5);  // Buffer drain (same as validateChipsetTiming)

    // Run test patterns (identical to validateChipsetTiming)
    int total = 0;
    int passed = 0;

    fl::MultiRunConfig multi_config;
    multi_config.num_runs = 1;
    multi_config.print_all_runs = false;
    multi_config.print_per_led_errors = false;
    multi_config.max_errors_per_run = 10;

    // Measure show-only duration (excludes setup/teardown overhead)
    uint32_t show_start_ms = millis();

    for (int pattern_id = 0; pattern_id < 4; pattern_id++) {
        setMixedBitPattern(leds, numLeds, pattern_id);
        runMultiTest(getBitPatternName(pattern_id), config, multi_config, total, passed, out_results);
    }

    out_show_duration_ms += millis() - show_start_ms;

    driver_total += total;
    driver_passed += passed;

    // proxy destructor deletes the controller → ~CLEDController removes from draw list
    // (no FastLED.reset(CHANNELS) needed — proxy handles cleanup)
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

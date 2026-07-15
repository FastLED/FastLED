// AutoResearchTest.cpp - Generic LED autoresearch testing infrastructure
// Driver-agnostic test function implementations

// Gate out under low-memory mode -- this file pulls in <FastLED.h> and
// the full chipset / encoder matrix (UCS7604, wave3, pixel iterators).
// LowMemory targets don't use those (their RPC surface is in
// AutoResearchLowMemory.h, which only uses fl::Remote + the LPC SCT
// driver). Matches the conditional structure in AutoResearch.ino itself.
#include "fl/system/sketch_macros.h"
#if !defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && !FL_PLATFORM_HAS_LARGE_MEMORY
#define FASTLED_AUTORESEARCH_LOW_MEMORY 1
#endif
#if !(defined(FASTLED_AUTORESEARCH_LOW_MEMORY) && FASTLED_AUTORESEARCH_LOW_MEMORY)

#include "AutoResearchTest.h"
#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
#include "fl/stl/atomic.h"
#include "fl/stl/isr.h"
#endif
#include "LegacyClocklessProxy.h"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/objectfled_diagnostics.h"
#include <FastLED.h>
#include "fl/stl/cstdio.h"
#include "fl/stl/sstream.h"

// PR #3421 framework-hang fix:
// FL_WARN expansion ALWAYS constructs `fl::sstream() << X` regardless
// of log level -- `fl::ScopedLogDisable` only suppresses OUTPUT inside
// `log_emit`. Under autoresearch's RPC ASYNC handler (which wraps the
// test in ScopedLogDisable), the cumulative `fl::sstream`/`fl::string`
// heap alloc/free cycles across this file's many FL_WARN sites leave
// the Teensy 4 heap in a state where a later function-exit destructor
// of a heap-backed fl::string hangs indefinitely (~80% of runs on
// LPUART, FlexIO, and ObjectFLED autoresearch tests).
//
// AR_FL_WARN gates the FL_WARN expansion at log level so the sstream
// is never constructed when output is suppressed. Used for every
// FL_WARN in this file. Marker-narrowed investigation confirmed the
// fix moves LPUART from ~1/5 success to 5/5 -- see session research.
#define AR_FL_WARN(...) do { \
    if (fl::getLogLevel() >= static_cast<fl::u8>(fl::LogLevel::FL_LOG_LEVEL_WARN)) { \
        FL_WARN(__VA_ARGS__); \
    } \
} while (0)
#include "fl/chipsets/encoders/ucs7604.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/channels/wave3.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/math/ease.h"
#include "pixel_controller.h"
#include "color.h"
#include "fl/stl/vector.h"
#include "fl/stl/iterator.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
void countRpPioRxRisingEdges(void* user_data) {
    auto* edges = static_cast<fl::atomic_int*>(user_data);
    if (edges != nullptr) {
        edges->fetch_add(1);
    }
}
#endif

// Phase 0: Include PARLIO debug instrumentation
#if defined(ESP32) && FASTLED_ESP32_HAS_PARLIO
#include "platforms/esp/32/drivers/parlio/channel_driver_parlio.h"
#endif

/// @brief Dump raw edge timing data to console for debugging
/// @param rx_channel RX device to read edge data from
/// @param timing Chipset timing configuration for pattern analysis
/// @param range Edge range to print (offset, count)
void dumpRawEdgeTiming(fl::shared_ptr<fl::RxChannel> rx_channel,
                       const fl::ChipsetTimingConfig& timing,
                       fl::EdgeRange range) {
    if (!rx_channel) {
        AR_FL_WARN("[RAW EDGE TIMING] ERROR: RX channel is null");
        return;
    }

    // Heap-allocated edge buffer (was a 2 KB FixedVector on the stack —
    // the capture path already runs the loopTask close to its limit,
    // FastLED#3569).
    fl::vector<fl::EdgeTime> edges;
    size_t buffer_size = range.count < 256 ? range.count : 256;
    edges.resize(buffer_size);  // Default initializes to EdgeTime()

    // Get edges starting at offset
    size_t edge_count = rx_channel->getRawEdgeTimes(edges, range.offset);

    if (edge_count == 0) {
        AR_FL_WARN("[RAW EDGE TIMING] WARNING: No edge data captured at offset " << range.offset);
        return;
    }

    // Calculate actual range printed (start from offset)
    size_t start_idx = range.offset;
    size_t end_idx = range.offset + edge_count;

    // Single-line collated edge dump. Was one FL_WARN per edge (~32-256
    // lines per call) which dominated the Teensy 4 UART TX FIFO on every
    // decode-failure path. Compact format keeps the diagnostic value
    // (each edge as `H580` or `L640`) while shipping in a single WS
    // frame. See fbuild#755 follow-up FL_WARN audit.
    {
        fl::sstream edge_dump;
        edge_dump << "[RAW EDGES " << start_idx << ".." << (end_idx - 1) << "]";
        for (size_t i = 0; i < edge_count; i++) {
            edge_dump << " " << (edges[i].high ? "H" : "L") << edges[i].ns;
        }
        AR_FL_WARN(edge_dump.str());
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
        AR_FL_WARN(ss.str());

        if (has_short_high && has_long_high && has_short_low && has_long_low) {
            AR_FL_WARN("\n[RAW EDGE TIMING] ✓ Encoder appears to be working correctly (varied timing patterns)");
        } else if (!has_short_high && !has_long_high) {
            ss.clear();
            ss << "[RAW EDGE TIMING] ✗ ENCODER BROKEN: No valid HIGH pulses detected!\n";
            ss << "[RAW EDGE TIMING]   Possible causes:\n";
            ss << "[RAW EDGE TIMING]   1. Encoder not reading pixel buffer data\n";
            ss << "[RAW EDGE TIMING]   2. Bytes encoder state machine stuck\n";
            ss << "[RAW EDGE TIMING]   3. Data pointer not passed correctly to encoder";
            FL_ERROR(ss.str());
        } else if (!has_short_low && !has_long_low) {
            // Use FL_WARN to avoid triggering bash autoresearch early exit
            AR_FL_WARN("[RAW EDGE TIMING] ✗ ENCODER BROKEN: No valid LOW pulses detected!");
        } else {
            AR_FL_WARN("[RAW EDGE TIMING] ⚠ Partial pattern match - encoder may have issues");
        }
    }
    AR_FL_WARN("");
}

/// @brief Check if an encoder selector identifies a UCS7604 variant
static bool isUCS7604(fl::ClocklessEncoder encoder) {
    return encoder == fl::ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_8BIT ||
           encoder == fl::ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT ||
           encoder == fl::ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT_1600;
}

/// @brief Build expected UCS7604 encoded bytes from LED data
/// @param leds LED data span
/// @param encoder Encoder selector identifying the UCS7604 variant
/// @return Vector of expected encoded bytes (preamble + padding + pixel data)
static fl::vector<uint8_t> buildExpectedUCS7604(fl::span<CRGB> leds, fl::ClocklessEncoder encoder) {
    fl::vector<uint8_t> expected;

    // Map encoder to UCS7604Mode
    fl::UCS7604Mode mode;
    switch (encoder) {
        case fl::ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_8BIT:
            mode = fl::UCS7604Mode::UCS7604_MODE_8BIT_800KHZ;
            break;
        case fl::ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT:
            mode = fl::UCS7604Mode::UCS7604_MODE_16BIT_800KHZ;
            break;
        case fl::ClocklessEncoder::CLOCKLESS_ENCODER_UCS7604_16BIT_1600:
            mode = fl::UCS7604Mode::UCS7604_MODE_16BIT_1600KHZ;
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
    bool use_gamma = (mode != fl::UCS7604Mode::UCS7604_MODE_8BIT_800KHZ);
    if (use_gamma) {
        gamma = fl::Gamma8::getOrCreate(2.8f);
    }

    // Encode using the same function the driver uses
    fl::encodeUCS7604(pixel_iter, leds.size(), fl::back_inserter(expected),
                      mode, current, false /* is_rgbw */, gamma.get());

    return expected;
}

// Decode SPI bit stream from raw RMT edges into LED RGB bytes
// The SPI data pin carries APA102 encoded data clocked by PCLK.
// RMT RX captures edges on the data pin. Each edge duration / bit_period_ns
// gives the number of consecutive same-value bits.
// Returns number of LED RGB bytes written to rx_buffer, or 0 on error.
static size_t decodeSpiEdges(fl::shared_ptr<fl::RxChannel> rx_channel,
                             fl::span<uint8_t> rx_buffer,
                             uint32_t clock_hz) {
    if (!rx_channel || clock_hz == 0) {
        AR_FL_WARN("[SPI DECODE] Invalid parameters");
        return 0;
    }

    const uint32_t bit_period_ns = static_cast<uint32_t>(1000000000ULL / clock_hz);
    const uint32_t half_bit_ns = bit_period_ns / 2;
    AR_FL_WARN("[SPI DECODE] clock=" << clock_hz << " Hz, bit_period=" << bit_period_ns << " ns");

    // Read raw edges (up to 4096 to handle large strips)
    constexpr size_t MAX_EDGES = 4096;
    fl::vector<fl::EdgeTime> edges(MAX_EDGES);
    fl::span<fl::EdgeTime> edge_span(edges.data(), edges.size());
    size_t edge_count = rx_channel->getRawEdgeTimes(edge_span, 0);

    if (edge_count == 0) {
        AR_FL_WARN("[SPI DECODE] No edges captured");
        return 0;
    }
    AR_FL_WARN("[SPI DECODE] Captured " << edge_count << " edges");

    // Reconstruct bit stream from edges
    // Each edge has a level (high/low) and duration in ns.
    // Number of bits = round(duration_ns / bit_period_ns)
    fl::vector<uint8_t> bits;
    bits.reserve(edge_count * 4); // Rough estimate

    for (size_t i = 0; i < edge_count; i++) {
        uint32_t duration = edges[i].ns;
        uint32_t num_bits = (duration + half_bit_ns) / bit_period_ns;
        if (num_bits == 0) num_bits = 1; // At least 1 bit per edge
        uint8_t bit_val = edges[i].high ? 1 : 0;
        for (uint32_t b = 0; b < num_bits; b++) {
            bits.push_back(bit_val);
        }
    }

    AR_FL_WARN("[SPI DECODE] Reconstructed " << bits.size() << " bits");

    if (bits.size() < 32) {
        AR_FL_WARN("[SPI DECODE] Too few bits for APA102 frame");
        return 0;
    }

    // Convert bits to bytes (MSB first)
    size_t total_bytes = bits.size() / 8;
    fl::vector<uint8_t> raw_bytes(total_bytes);
    for (size_t i = 0; i < total_bytes; i++) {
        uint8_t byte_val = 0;
        for (int bit = 7; bit >= 0; bit--) {
            size_t bit_idx = i * 8 + (7 - bit);
            if (bit_idx < bits.size() && bits[bit_idx]) {
                byte_val |= (1 << bit);
            }
        }
        raw_bytes[i] = byte_val;
    }

    AR_FL_WARN("[SPI DECODE] Decoded " << total_bytes << " raw bytes");

    // Log first few bytes for debugging
    {
        fl::sstream dbg;
        dbg << "[SPI DECODE] First bytes:";
        size_t show = total_bytes < 20 ? total_bytes : 20;
        for (size_t i = 0; i < show; i++) {
            dbg << " 0x";
            uint8_t hi = raw_bytes[i] >> 4;
            uint8_t lo = raw_bytes[i] & 0xF;
            dbg << (char)(hi < 10 ? '0' + hi : 'A' + hi - 10);
            dbg << (char)(lo < 10 ? '0' + lo : 'A' + lo - 10);
        }
        AR_FL_WARN(dbg.str());
    }

    // Find APA102 start frame (4 bytes of 0x00)
    // The RMT may not capture the start frame since it's all zeros and
    // the idle state is also LOW. In that case, the first captured data
    // is the first LED frame header (0xE0 | brightness).
    size_t data_start = 0;

    // Check if we captured the start frame
    if (total_bytes >= 4 && raw_bytes[0] == 0x00 && raw_bytes[1] == 0x00 &&
        raw_bytes[2] == 0x00 && raw_bytes[3] == 0x00) {
        data_start = 4; // Skip start frame
        AR_FL_WARN("[SPI DECODE] Found start frame at offset 0");
    } else {
        // No start frame captured (RMT started at first edge = first HIGH bit)
        // First byte should be 0xE0|brightness or 0xFF
        AR_FL_WARN("[SPI DECODE] No start frame (first edge = first LED data)");
    }

    // Extract LED RGB data from APA102 frames
    // Each LED: [0xE0|brightness] [color0] [color1] [color2] (4 bytes)
    // With EOrder=RGB in the validation config, the wire order after the
    // brightness header is [R, G, B] — we copy these directly to rx_buffer.
    // NOTE: The APA102 end frame (all 0xFF) also has (0xFF & 0xE0) == 0xE0,
    // so we cannot distinguish end frame from a max-brightness LED purely
    // from the header byte. We stop after we've exhausted captured data.
    size_t led_bytes_written = 0;
    size_t pos = data_start;

    while (pos + 4 <= total_bytes) {
        uint8_t header = raw_bytes[pos];

        // Check for valid LED frame header (top 3 bits must be 111)
        if ((header & 0xE0) != 0xE0) {
            AR_FL_WARN("[SPI DECODE] End of LED data at byte " << pos
                    << " (header=0x" << ((header >> 4) < 10 ? '0' + (header >> 4) : 'A' + (header >> 4) - 10)
                    << ((header & 0xF) < 10 ? '0' + (header & 0xF) : 'A' + (header & 0xF) - 10) << ")");
            break;
        }

        uint8_t c0 = raw_bytes[pos + 1];
        uint8_t c1 = raw_bytes[pos + 2];
        uint8_t c2 = raw_bytes[pos + 3];

        if (led_bytes_written + 3 <= rx_buffer.size()) {
            rx_buffer[led_bytes_written + 0] = c0;
            rx_buffer[led_bytes_written + 1] = c1;
            rx_buffer[led_bytes_written + 2] = c2;
            led_bytes_written += 3;
        } else {
            AR_FL_WARN("[SPI DECODE] rx_buffer full at LED " << (led_bytes_written / 3));
            break;
        }
        pos += 4;
    }

    size_t num_leds = led_bytes_written / 3;
    AR_FL_WARN("[SPI DECODE] Extracted " << num_leds << " LEDs (" << led_bytes_written << " RGB bytes)");
    return led_bytes_written;
}

// Collect expensive raw-edge evidence only for a failed run.  The normal RPC
// soak path must remain allocation-quiet, but a failed decode or byte compare
// still needs the same postmortem snapshot as a failed RX wait.
void collectRawEdgeDiagnostics(fl::shared_ptr<fl::RxChannel> rx_channel,
                               fl::RunResult* diagnostics) {
    if (!rx_channel || !diagnostics || !diagnostics->rawEdgeSample.empty()) {
        return;
    }

    fl::vector<fl::EdgeTime> edges;
    edges.resize(256);
    diagnostics->rawEdgesAfterWait =
        static_cast<int>(rx_channel->getRawEdgeTimes(edges, 0));

    fl::vector<fl::EdgeTime> sample_edges;
    sample_edges.resize(32);
    const size_t sample_count = rx_channel->getRawEdgeTimes(sample_edges, 0);
    fl::sstream sample;
    for (size_t i = 0; i < sample_count; ++i) {
        if (i > 0) {
            sample << ' ';
        }
        sample << (sample_edges[i].high ? 'H' : 'L') << sample_edges[i].ns;
    }
    diagnostics->rawEdgeSample = sample.str();
}

// Capture transmitted LED data via RX loopback
// - rx_channel: Shared pointer to RX device (persistent across calls)
// - rx_buffer: Buffer to store received bytes
// - timing: Chipset timing configuration for RX decoder
// - driver_name: Name of the TX driver being tested (e.g., "RMT", "PARLIO") - enables io_loop_back only for RMT
// Returns number of bytes captured, or 0 on error
size_t capture(fl::shared_ptr<fl::RxChannel> rx_channel,
               fl::span<uint8_t> rx_buffer,
               size_t expected_data_bytes,
               const fl::ChipsetTimingConfig& timing,
               const char* driver_name,
               fl::RunResult* diagnostics) {
    // The RPC runner disables log output for normal tests. Keep successful
    // captures allocation-quiet too: constructing diagnostic strings and
    // copying edge snapshots on every frame defeats that guard during a
    // repeated-frame soak. Failure paths still collect full evidence below.
    const bool logs_enabled = fl::getLogLevel() >=
        static_cast<fl::u8>(fl::LogLevel::FL_LOG_LEVEL_WARN);
    if (diagnostics) {
        diagnostics->captureWaitResult = -1;
        diagnostics->rawEdgesAfterWait = 0;
        diagnostics->rpPioGpioTransitions = -1;
        diagnostics->rxBeginError.clear();
    }

    if (!rx_channel) {
        FL_ERROR("RX channel is null");
        return 0;
    }

    // Clear RX buffer
    fl::memset(rx_buffer.data(), 0, rx_buffer.size());

    // Prepare RX config (but don't arm yet to avoid locking TX resources)
    fl::RxChannelConfig rx_config(rx_channel->getPin());
    rx_config.hz = 40000000; // 40MHz for high-precision LED timing capture

    // Buffer size: 1 LED byte = 8 bits = 8 RMT symbols
    // UART with TX inversion produces standard WS2812 waveform, same symbol count
    bool is_uart_driver = (fl::strcmp(driver_name, "UART") == 0)
                       || (fl::strcmp(driver_name, "LPUART") == 0);
    bool is_object_fled_driver = (fl::strcmp(driver_name, "OBJECT_FLED") == 0);
    rx_config.edge_capacity = rx_buffer.size() * 8;
#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
    // RP's platform-default RX resolves to PIO, while RxChannel retains the
    // requested PLATFORM_DEFAULT enum. Recognize both that public default and
    // an explicit PIO backend so the DMA capture is sized per frame.
    const bool is_rp_pio_capture =
        rx_channel->backend() == fl::RxBackend::PIO ||
        fl::strcmp(driver_name, "PIO0") == 0 || fl::strcmp(driver_name, "PIO1") == 0;
    if (is_rp_pio_capture) {
        // The shared result buffer is intentionally sized for the largest
        // AutoResearch run, not this frame. PIO DMA stores one word for each
        // high and low phase, so using that whole buffer here would request
        // several MiB on a Pico and leave DMA with an invalid destination.
        constexpr size_t kPioPhasesPerDataByte = 16;
        if (expected_data_bytes >
            (static_cast<size_t>(-1) - 1u) / kPioPhasesPerDataByte) {
            FL_ERROR("[CAPTURE] PIO edge-capacity overflow");
            return 0;
        }
        rx_config.edge_capacity = expected_data_bytes * kPioPhasesPerDataByte + 1u;
    }
#endif

    // Internal loopback configuration: Enable ONLY for RMT TX -> RMT RX scenarios
    // When driver_name == "RMT", enable io_loop_back to route RMT TX output to RMT RX internally
    // This is REQUIRED for ESP32-S3 because TX GPIO output stops when RX is active on different GPIO
    // For other drivers (PARLIO, SPI, UART, I2S), disable io_loop_back (use external GPIO wire)
    bool is_rmt_driver = (fl::strcmp(driver_name, "RMT") == 0);
    rx_config.io_loop_back = is_rmt_driver;
    // RX DMA streaming: extends capture past the non-DMA cap by sizing
    // mem_block_symbols = 14336 so ESP-IDF allocates ~14 DMA descriptor
    // nodes (each 4092 bytes), yielding a 57 KB user-buffer cap — enough to
    // capture ~583 WS2812B LEDs in a single rmt_receive() call. Safe for
    // non-RMT TX paths (SPI, PARLIO, I2S, UART, LCD_*) that don't contend
    // for the shared ESP32-S3 DMA slot. RMT loopback stays non-DMA.
    // See issue #2254.
    rx_config.use_dma = !is_rmt_driver;
    // Preserve the channel's explicitly-selected capture backend —
    // RxChannel::begin() recreates the device when the backend field
    // differs, and this config is built fresh (default =
    // PLATFORM_DEFAULT), which silently reverted rxBackend overrides
    // like I2S_RX back to RMT (FastLED#3576 Phase 3).
    rx_config.backend = rx_channel->backend();
    if (is_rmt_driver) {
        AR_FL_WARN("[CAPTURE] RMT TX -> RMT RX: Internal loopback enabled (io_loop_back=true)");
    } else {
        // The RX peripheral is platform-dependent (RMT on ESP32, FlexPWM on
        // Teensy 4, LPC_SCT on LPC845, …). Don't claim "RMT RX" on platforms
        // that have no RMT — that label burned an hour of Teensy-4 debug
        // (FastLED#3059). Just say "external RX" so the label stays correct
        // regardless of backend.
        if (logs_enabled) {
            AR_FL_WARN("[CAPTURE] " << driver_name
                        << " TX -> external RX: External GPIO wire "
                        << "(io_loop_back=false, use_dma=true)");
        }
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
        if (logs_enabled) {
            AR_FL_WARN("[CAPTURE] RMT: Two-TX approach (ESP32-S3 workaround)");
        }
        FastLED.show();
        if (!FastLED.wait(TX_WAIT_TIMEOUT_MS)) {
            FL_ERROR("[CAPTURE] TX wait timeout (pre-arm) - driver may be stalled");
            return 0;
        }

        // Arm RX for capture
        if (!rx_channel->begin(rx_config)) {
            if (diagnostics) diagnostics->rxBeginError = rx_channel->lastBeginError();
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
            if (diagnostics) diagnostics->rxBeginError = rx_channel->lastBeginError();
            AR_FL_WARN("[CAPTURE] RX begin() failed for pin " << rx_channel->getPin());
            return 0;
        }
        if (logs_enabled) {
            AR_FL_WARN("[CAPTURE] RX armed, calling FastLED.show()...");
        }

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
        fl::atomic_int rising_edges(0);
        fl::isr::handle edge_counter;
        bool edge_counter_attached = false;
        const bool measure_rp_pio =
            (fl::strcmp(driver_name, "PIO0") == 0 || fl::strcmp(driver_name, "PIO1") == 0) &&
            diagnostics != nullptr;
        if (measure_rp_pio) {
            // FastLED.show() drives the channel engine until it reaches its
            // terminal state, so polling GPIO after show() is too late to
            // observe a short clockless frame. Count rising edges through the
            // shared FastLED ISR dispatcher while show() is running instead.
            // This is deliberately a binary physical oracle: an IRQ count
            // above zero proves the jumper carries TX activity, while the PIO
            // RX backend remains responsible for exact-byte verification.
            fl::isr::config edge_config;
            edge_config.handler = countRpPioRxRisingEdges;
            edge_config.user_data = &rising_edges;
            edge_config.flags = fl::isr::ISR_FLAG_EDGE_RISING;
            edge_counter_attached = fl::isr::attach_external_handler(
                static_cast<fl::u8>(rx_channel->getPin()), edge_config, &edge_counter) == 0;
        }
#endif
        FastLED.show();
        if (logs_enabled) {
            AR_FL_WARN("[CAPTURE] FastLED.show() returned, calling wait...");
        }
#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)
        if (edge_counter_attached && edge_counter.is_valid()) {
            fl::isr::detach_handler(edge_counter);
        }
        if ((fl::strcmp(driver_name, "PIO0") == 0 || fl::strcmp(driver_name, "PIO1") == 0) &&
            diagnostics != nullptr) {
            diagnostics->rpPioGpioTransitions = edge_counter_attached ? rising_edges.load() : -1;
        }
#endif
        if (!FastLED.wait(TX_WAIT_TIMEOUT_MS)) {
            if (is_object_fled_driver) {
                fl::objectFledDiagnosticsRecord("afterFastLedWaitTimeout");
            }
            AR_FL_WARN("[CAPTURE] FastLED.wait() timed out");
            return 0;
        }
        if (is_object_fled_driver) {
            fl::objectFledDiagnosticsRecord("afterFastLedWait");
        }
        if (logs_enabled) {
            AR_FL_WARN("[CAPTURE] FastLED.wait() done");
        }
    }




    // Wait for RX completion
    // WS2812B: ~30μs per LED → 3000 LEDs = 90ms, use 150ms
    // UART with inverted TX produces same waveform timing as WS2812
    const uint32_t rx_wait_ms = is_uart_driver ? 500 : 150;
    if (logs_enabled) {
        AR_FL_WARN("[CAPTURE] Waiting for RX completion (" << rx_wait_ms
                    << "ms timeout)...");
    }
    auto wait_result = rx_channel->wait(rx_wait_ms);
    if (diagnostics) {
        diagnostics->captureWaitResult = static_cast<int>(wait_result);
        diagnostics->decodeOutputCapacity = static_cast<int>(rx_buffer.size());
        if (wait_result != fl::RxWaitResult::SUCCESS) {
            collectRawEdgeDiagnostics(rx_channel, diagnostics);
        }
    }
    if (logs_enabled) {
        AR_FL_WARN("[CAPTURE] RX wait returned: "
                    << static_cast<int>(wait_result));
    }

    if (wait_result != fl::RxWaitResult::SUCCESS) {
        AR_FL_WARN("RX wait failed (timeout or no data received)");
        fl::sstream ss;
        ss << "\n⚠️  TROUBLESHOOTING:\n";
        ss << "   1. Connect physical jumper wire from TX GPIO to RX GPIO " << rx_channel->getPin() << "\n";
        ss << "   2. Check that both TX and RX pins are correctly configured\n";
        ss << "   3. Verify the GPIO connection is working (GPIO baseline test should pass)\n";
        ss << "   4. For RMT TX → RMT RX: Ensure io_loop_back=true in RxConfig";
        AR_FL_WARN(ss.str());
        if (!is_uart_driver) {
            return 0;
        }
        // UART: still attempt decode with captured edges (timeout may be expected)
        AR_FL_WARN("[CAPTURE] UART: attempting decode with captured edges despite timeout");
    }

    // UART with TX inversion at 4 Mbps produces a standard WS2812-compatible
    // waveform on the wire. Each 10-bit UART frame (2500ns) encodes exactly 2
    // LED bits with correct WS2812 timing:
    //   T0H=250ns, T0L=1000ns (LED bit "0")
    //   T1H=750ns, T1L=500ns  (LED bit "1")
    // Use the standard WS2812 decoder with UART-specific timing thresholds.
    if (is_uart_driver) {
        AR_FL_WARN("[CAPTURE] UART (inverted TX): using standard WS2812 decoder with UART timing...");
        // UART timing at 4 Mbps: T0H=250ns, T1H-T0H=500ns, T0L=1000ns
        fl::ChipsetTiming uart_timing{
            250,   // T1 = T0H (1 UART bit = 250ns)
            500,   // T2 = T1H - T0H (3 bits - 1 bit = 2 bits = 500ns)
            500,   // T3 = T1L (2 UART bits = 500ns, stop + next start)
            50,    // reset_us (WS2812 minimum)
            "UART_4Mbps"
        };
        // Use wider tolerance (250ns) for UART because the UART clock and RMT
        // sample clock are asynchronous, and GPIO matrix adds ~10-20ns jitter
        auto rx_timing = fl::make4PhaseTiming(uart_timing, 250);
        rx_timing.gap_tolerance_ns = 100000; // 100µs for UART inter-frame gaps

        AR_FL_WARN("[CAPTURE] UART RX timing: T0H=" << uart_timing.T1
                << " T1H=" << (uart_timing.T1 + uart_timing.T2)
                << " T0L=" << (uart_timing.T2 + uart_timing.T3)
                << " T1L=" << uart_timing.T3);

        auto decode_result = rx_channel->decode(rx_timing, rx_buffer);
        if (!decode_result.ok()) {
            AR_FL_WARN("[CAPTURE] UART decode failed (error: " << static_cast<int>(decode_result.error()) << ")");
            // Was EdgeRange(0, 256) -- 257 FL_WARN lines per decode failure
// drowned the Teensy 4 UART TX FIFO and stalled autoresearch
// between patterns (fbuild#755). 32 edges is enough to see whether
// the first transition is a clean bit-0 or a glitch.
dumpRawEdgeTiming(rx_channel, timing, fl::EdgeRange(0, 32));
            return 0;
        }
        AR_FL_WARN("[CAPTURE] UART decoded " << decode_result.value() << " LED bytes");
        return decode_result.value();
    }

    // SPI chipset drivers: decode raw SPI bit stream.
    // These drivers use platform hardware SPI/I80/I2S to output APA102 data.
    // RMT RX captures edges on the data pin; clock pin is ignored.
    bool is_lcd_spi_driver = (fl::strcmp(driver_name, "LCD_SPI") == 0);
    bool is_i2s_spi_driver = (fl::strcmp(driver_name, "I2S_SPI") == 0);
    bool is_spi_unified_driver = (fl::strcmp(driver_name, "SPI_UNIFIED") == 0);
    if (is_lcd_spi_driver || is_i2s_spi_driver || is_spi_unified_driver) {
        // SPI clock used for validation: 2.4MHz (matches ValidationRemote.cpp)
        const uint32_t spi_clock_hz = 2400000;
        AR_FL_WARN("[CAPTURE] SPI chipset decode: clock=" << spi_clock_hz << " Hz");
        size_t decoded = decodeSpiEdges(rx_channel, rx_buffer, spi_clock_hz);
        if (decoded == 0) {
            AR_FL_WARN("[CAPTURE] SPI decode failed");
            // Was EdgeRange(0, 256) -- 257 FL_WARN lines per decode failure
// drowned the Teensy 4 UART TX FIFO and stalled autoresearch
// between patterns (fbuild#755). 32 edges is enough to see whether
// the first transition is a clean bit-0 or a glitch.
dumpRawEdgeTiming(rx_channel, timing, fl::EdgeRange(0, 32));
        }
        return decoded;
    }

    // Decode received data directly into rx_buffer
    // Create 4-phase RX timing from the TX timing
    //
    // Wave8 drivers (SPI, PARLIO, I2S) use 8-bit expansion encoding:
    //   Bit 0: round(T1/(T1+T2+T3)*8) HIGH pulses, rest LOW
    //   Bit 1: round((T1+T2)/(T1+T2+T3)*8) HIGH pulses, rest LOW
    // The actual pulse widths are quantized to tick boundaries, so we must
    // compute the exact wave8 timing for RX decode thresholds.
    // Without this correction, the RX decoder may reject valid waveforms when
    // the nominal timing period differs from the quantized 8-tick period.
    // Example: WS2812_800KHZ has T0L=1000ns nominal but PARLIO wave8 produces 750ns.
    //
    // Clock sources differ by driver:
    //   SPI/I2S: clock = 8/(T1+T2+T3) Hz, tick = (T1+T2+T3)/8 ns (variable)
    //   PARLIO:  clock = 8 MHz fixed, tick = 125 ns (fixed)
    bool is_spi_driver = (fl::strcmp(driver_name, "SPI") == 0);
    bool is_parlio_driver = (fl::strcmp(driver_name, "PARLIO") == 0);
    bool is_i2s_driver = (fl::strcmp(driver_name, "I2S") == 0);
    // LCD_CLOCKLESS auto-selects wave3 vs wave8 based on canUseWave3() of the
    // chipset timing. Compute the same eligibility check on the host side so
    // the RX decoder reconstructs the correct quantized waveform.
    bool is_lcd_clockless_driver = (fl::strcmp(driver_name, "LCD_CLOCKLESS") == 0);
    bool lcd_clockless_uses_wave3 = false;
    if (is_lcd_clockless_driver) {
        fl::ChipsetTiming probe{timing.t1_ns, timing.t2_ns, timing.t3_ns, timing.reset_us, timing.name};
        lcd_clockless_uses_wave3 = fl::canUseWave3(probe);
    }
    bool uses_wave8 = is_spi_driver || is_parlio_driver || is_i2s_driver
                      || (is_lcd_clockless_driver && !lcd_clockless_uses_wave3);
    bool uses_wave3 = is_lcd_clockless_driver && lcd_clockless_uses_wave3;
    fl::ChipsetTiming tx_timing;
    if (uses_wave8) {
        // Compute actual wave8 timing from chipset timing
        const uint32_t period = timing.t1_ns + timing.t2_ns + timing.t3_ns;
        // PARLIO uses fixed 8MHz clock (125ns/tick), not derived from period
        // SPI/I2S/LCD_CLOCKLESS derive clock from period: tick = period/8
        const uint32_t tick_ns = is_parlio_driver ? 125 : (period / 8);
        // Wave8 LUT computes: pulses = round(fraction * 8)
        const uint32_t pulses_bit0 = static_cast<uint32_t>(
            static_cast<float>(timing.t1_ns) / period * 8.0f + 0.5f);
        const uint32_t pulses_bit1 = static_cast<uint32_t>(
            static_cast<float>(timing.t1_ns + timing.t2_ns) / period * 8.0f + 0.5f);
        // Convert back to 3-phase timing (T1=T0H, T2=T1H-T0H, T3=actual_period-T1H)
        const uint32_t actual_t0h = pulses_bit0 * tick_ns;
        const uint32_t actual_t1h = pulses_bit1 * tick_ns;
        const uint32_t actual_period = 8 * tick_ns;
        const char* wave8_name = is_spi_driver ? "SPI_wave8" :
                                 is_parlio_driver ? "PARLIO_wave8" :
                                 is_lcd_clockless_driver ? "LCD_CLOCKLESS_wave8" : "I2S_wave8";
        tx_timing = fl::ChipsetTiming{
            actual_t0h,                    // T1 = T0H
            actual_t1h - actual_t0h,       // T2 = T1H - T0H
            actual_period - actual_t1h,    // T3 = actual_period - T1H
            timing.reset_us,
            wave8_name
        };
        AR_FL_WARN("[RX TIMING] " << wave8_name << ": pulses_bit0=" << pulses_bit0
                << " pulses_bit1=" << pulses_bit1
                << " tick_ns=" << tick_ns
                << " -> T1=" << tx_timing.T1 << " T2=" << tx_timing.T2
                << " T3=" << tx_timing.T3);
    } else if (uses_wave3) {
        // Wave3 encoding: 3 ticks per LED bit, clock = 3/(T1+T2+T3) Hz
        const uint32_t period = timing.t1_ns + timing.t2_ns + timing.t3_ns;
        const uint32_t tick_ns = period / 3;
        const uint32_t ticks_bit0 = static_cast<uint32_t>(
            static_cast<float>(timing.t1_ns) / period * 3.0f + 0.5f);
        const uint32_t ticks_bit1 = static_cast<uint32_t>(
            static_cast<float>(timing.t1_ns + timing.t2_ns) / period * 3.0f + 0.5f);
        const uint32_t actual_t0h = ticks_bit0 * tick_ns;
        const uint32_t actual_t1h = ticks_bit1 * tick_ns;
        const uint32_t actual_period = 3 * tick_ns;
        tx_timing = fl::ChipsetTiming{
            actual_t0h,
            actual_t1h - actual_t0h,
            actual_period - actual_t1h,
            timing.reset_us,
            "LCD_CLOCKLESS_wave3"
        };
        AR_FL_WARN("[RX TIMING] LCD_CLOCKLESS_wave3: ticks_bit0=" << ticks_bit0
                << " ticks_bit1=" << ticks_bit1
                << " tick_ns=" << tick_ns
                << " -> T1=" << tx_timing.T1 << " T2=" << tx_timing.T2
                << " T3=" << tx_timing.T3);
    } else {
        tx_timing = fl::ChipsetTiming{timing.t1_ns, timing.t2_ns, timing.t3_ns, timing.reset_us, timing.name};
    }
    // Wave8/wave3 encoding has timing jitter due to clock quantization and GPIO matrix latency
    // Use wider tolerance (200ns) to accommodate clock rounding
    const uint32_t tolerance = (uses_wave8 || uses_wave3) ? 200 : 170;
    auto rx_timing = fl::make4PhaseTiming(tx_timing, tolerance);

    // Enable gap tolerance for PARLIO/SPI DMA gaps
    // PARLIO: ~20µs typical gaps during buffer transitions
    // SPI: Can have longer inter-frame gaps due to software encoding timing
    // Increased to 100µs to accommodate SPI driver timing variations
    rx_timing.gap_tolerance_ns = 100000; // 100µs (was 30µs)

    AR_FL_WARN("[CAPTURE] Decoding...");
    auto decode_result = rx_channel->decode(rx_timing, rx_buffer);
    if (diagnostics) {
        diagnostics->decodeOk = decode_result.ok() ? 1 : 0;
        diagnostics->decodeError =
            decode_result.ok() ? -1 : static_cast<int>(decode_result.error());
        diagnostics->decodeBytes =
            decode_result.ok() ? static_cast<int>(decode_result.value()) : 0;
    }

    if (!decode_result.ok()) {
        // Use FL_WARN instead of FL_ERROR to avoid triggering bash autoresearch exit
        // This can happen during warmup/setup and is not fatal
        AR_FL_WARN("Decode failed (error code: " << static_cast<int>(decode_result.error()) << ")");
        // Print raw edge timing on decode failure to diagnose the issue
        // Was EdgeRange(0, 256) -- 257 FL_WARN lines per decode failure
// drowned the Teensy 4 UART TX FIFO and stalled autoresearch
// between patterns (fbuild#755). 32 edges is enough to see whether
// the first transition is a clean bit-0 or a glitch.
dumpRawEdgeTiming(rx_channel, timing, fl::EdgeRange(0, 32));
        return 0;
    }

    AR_FL_WARN("[CAPTURE] Decoded " << decode_result.value() << " bytes");
    return decode_result.value();
}

// Generic driver-agnostic autoresearch test runner
// Tests all channels using the specified configuration
void runTest(const char* test_name,
             fl::AutoResearchConfig& config,
             int& total, int& passed) {
    // Multi-lane limitation: Only test Lane 0 (first channel)
    // Hardware constraint: Only one TX channel can be read from via RX loopback
    size_t channels_to_test = config.tx_configs.size() > 1 ? 1 : config.tx_configs.size();

    if (config.tx_configs.size() > 1) {
        AR_FL_WARN("\n[MULTI-LANE] Testing " << config.tx_configs.size() << " lanes, testing Lane 0 only (hardware limitation)");
    }

    // Test enabled configs (Lane 0 only for multi-lane)
    for (size_t config_idx = 0; config_idx < channels_to_test; config_idx++) {
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
            config.tx_configs[config_idx].getDataPin()
        };

        AR_FL_WARN("\n=== " << test_name << " [Lane " << config_idx << "/" << config.tx_configs.size()
                << ", Pin " << config.tx_configs[config_idx].getDataPin()
                << ", LEDs " << config.tx_configs[config_idx].mLeds.size() << "] ===");

        // Use RX channel provided via config (created in .ino file, never created dynamically here)
        if (!config.rx_channel) {
            FL_ERROR("[" << ctx.driver_name << "/" << ctx.timing_name << "/" << ctx.pattern_name
                    << " | Lane " << ctx.lane_index << "/" << ctx.lane_count
                    << " (Pin " << ctx.pin_number << ", " << ctx.num_leds << " LEDs) | RX:" << ctx.rx_type_name << "] "
                    << "RX channel is null - must be created in .ino and passed via AutoResearchConfig");
            FL_ERROR("Result: FAIL ✗ (RX channel not provided)");
            continue;
        }

        size_t bytes_captured = capture(config.rx_channel, config.rx_buffer, num_leds * 3u,
                                        config.timing, config.driver_name);

        if (bytes_captured == 0) {
            FL_ERROR("[" << ctx.driver_name << "/" << ctx.timing_name << "/" << ctx.pattern_name
                    << " | Lane " << ctx.lane_index << "/" << ctx.lane_count
                    << " (Pin " << ctx.pin_number << ", " << ctx.num_leds << " LEDs) | RX:" << ctx.rx_type_name << "] "
                    << "Result: FAIL ✗ (capture failed)");
            continue;
        }

        int mismatches = 0;

        if (isUCS7604(config.encoder)) {
            // UCS7604: Compare full encoded frame (preamble + padding + pixel data) byte-for-byte
            fl::vector<uint8_t> expected_encoded = buildExpectedUCS7604(
                config.tx_configs[config_idx].mLeds, config.encoder);
            size_t expected_len = expected_encoded.size();

            AR_FL_WARN("UCS7604 encoded comparison: expected " << expected_len << " bytes, captured " << bytes_captured);

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

            AR_FL_WARN("Bytes Captured: " << bytes_captured << " (expected: " << expected_len << ")");
            int total_bytes = static_cast<int>(expected_len);
            AR_FL_WARN("Accuracy: " << (100.0 * (total_bytes - mismatches) / total_bytes) << "% ("
                    << (total_bytes - mismatches) << "/" << total_bytes << " bytes match)");
        } else {
            // WS2812: Compare raw RGB per-LED
            size_t bytes_expected = num_leds * 3;

            // NEVER EVER CHANGE THIS!!!!!
            const size_t front_padding_bytes = 0; // No front padding: PARLIO FRONT_PAD_BYTES=0
            const size_t rx_buffer_offset = front_padding_bytes;

            if (bytes_captured > bytes_expected + front_padding_bytes) {
                AR_FL_WARN("Info: Captured " << bytes_captured << " bytes ("
                        << front_padding_bytes << " front pad + "
                        << bytes_expected << " LED data + "
                        << (bytes_captured - bytes_expected - front_padding_bytes) << " back pad/RESET)");
            }

            // Compare: byte-level comparison (COLOR_ORDER is RGB, so no reordering)
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

            AR_FL_WARN("Bytes Captured: " << bytes_captured << " (expected: " << bytes_expected << ")");
            AR_FL_WARN("Accuracy: " << (100.0 * (num_leds - mismatches) / num_leds) << "% ("
                    << (num_leds - mismatches) << "/" << num_leds << " LEDs match)");
        }

        if (mismatches == 0) {
            AR_FL_WARN("Result: PASS ✓");
            passed++;
        } else {
            FL_ERROR("[" << ctx.driver_name << "/" << ctx.timing_name << "/" << ctx.pattern_name
                    << " | Lane " << ctx.lane_index << "/" << ctx.lane_count
                    << " (Pin " << ctx.pin_number << ", " << ctx.num_leds << " LEDs) | RX:" << ctx.rx_type_name << "] "
                    << "Result: FAIL ✗");
        }
    }
}

// Multi-run autoresearch test runner
// Runs the same test multiple times and tracks errors across runs
void runMultiTest(const char* test_name,
                  fl::AutoResearchConfig& config,
                  const fl::MultiRunConfig& multi_config,
                  int& total, int& passed,
                  fl::vector<fl::RunResult>* out_results) {

    // PR #3421 framework-hang fix:
    // FL_WARN expansion ALWAYS constructs `fl::sstream() << X` regardless
    // of log level -- `ScopedLogDisable` only suppresses the OUTPUT inside
    // `log_emit`. Under autoresearch's RPC ASYNC handler (which wraps the
    // test in `ScopedLogDisable`), the cumulative `fl::sstream`/`fl::string`
    // heap alloc/free cycles in runMultiTest's banner + per-run + summary
    // FL_WARNs were leaving the Teensy 4 heap in a state where the
    // function-exit destructor of the heap-backed `fl::string` hangs
    // indefinitely (reproduced ~80% of runs on LPUART, FlexIO, and
    // ObjectFLED autoresearch tests). Skipping the sstream construction
    // when log level is below WARN avoids the heap thrash entirely.
    // See: marker-narrowed investigation in PR #3421 / session research.
    const bool kLogsEnabled = fl::getLogLevel() >=
        static_cast<fl::u8>(fl::LogLevel::FL_LOG_LEVEL_WARN);

    fl::sstream ss;
    if (kLogsEnabled) {
        ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
        ss << "║ MULTI-RUN TEST: " << test_name << "\n";
        ss << "║ Runs: " << multi_config.num_runs << " | Print Mode: "
           << (multi_config.print_all_runs ? "All" : "Errors ONLY") << "\n";
        ss << "╚════════════════════════════════════════════════════════════════╝";
        AR_FL_WARN(ss.str());
    }

    fl::vector<fl::RunResult> run_results;
    int total_passed = 0;
    int total_failed = 0;
    bool retained_success = false;

    // Multi-lane limitation: Only test Lane 0
    size_t channels_to_test = config.tx_configs.size() > 1 ? 1 : config.tx_configs.size();

    if (config.tx_configs.size() > 1) {
        AR_FL_WARN("[MULTI-LANE] Testing " << config.tx_configs.size() << " lanes, testing Lane 0 only");
    }

    // Execute multiple runs
    for (int run = 1; run <= multi_config.num_runs; run++) {
        // runSingleTest executes synchronously inside the RPC task, so the
        // sketch loop cannot feed its five-second watchdog while a legitimate
        // repeated-frame soak is in progress. Each capture is independently
        // bounded by the TX/RX waits below; feed between them rather than
        // relaxing the watchdog or adding serial progress chatter.
        FastLED.watchdog().feed();
        // Do not construct progress strings under ScopedLogDisable: a long
        // hardware soak otherwise performs throwaway heap allocations.
        if (kLogsEnabled && (run % 3 == 1 || multi_config.num_runs <= 5)) {
            AR_FL_WARN("[Run " << run << "/" << multi_config.num_runs << "] Testing...");
        }

        fl::RunResult result;
        result.run_number = run;

        // Test Lane 0 only
        for (size_t config_idx = 0; config_idx < channels_to_test; config_idx++) {
            const auto& leds = config.tx_configs[config_idx].mLeds;
            size_t num_leds = leds.size();
            result.total_leds = num_leds;
            result.totalBytes = num_leds * 3;

            // Capture RX data
            size_t bytes_captured = capture(config.rx_channel, config.rx_buffer, num_leds * 3u,
                                            config.timing, config.driver_name, &result);
            result.capturedBytes = static_cast<int>(bytes_captured);

            if (bytes_captured == 0) {
                AR_FL_WARN("[Run " << run << "] Capture failed");
                result.mismatches = static_cast<int>(num_leds);
                result.mismatchedBytes = static_cast<int>(result.totalBytes);
                result.captureFailed = true;
                result.passed = false;
                // This also covers decoder failures after a successful RX
                // wait: capture() reports zero bytes, so preserve the raw
                // edge snapshot before this run exits.
                collectRawEdgeDiagnostics(config.rx_channel, &result);
                break;
            }

            // Check pixel data
            int mismatches = 0;

            // Diagnostic: single-line driver + first-24-bytes dump. Was 26
            // FL_WARNs (one per byte) -- on a 4-pattern test that produced 104
            // serial lines of pure hex dump, which dominated the Teensy 4 UART
            // TX FIFO under bursty autoresearch loads (fbuild#755 followup,
            // FastLED#3219 stack of fixes). Collapsing into a single
            // space-separated hex string keeps the diagnostic value while
            // dropping per-pattern serial overhead from ~2.1 KB to ~80 bytes.
            if (kLogsEnabled) {
                fl::sstream hex_dump;
                size_t dump_count = (bytes_captured < 24) ? bytes_captured : 24;
                for (size_t i = 0; i < dump_count; i++) {
                    if (i > 0) hex_dump << " ";
                    hex_dump << fl::hex << static_cast<int>(config.rx_buffer[i]) << fl::dec;
                }
                AR_FL_WARN("[RUN " << run << "] Driver=" << config.driver_name
                        << " bytes_captured=" << bytes_captured
                        << " first" << static_cast<int>(dump_count) << "=" << hex_dump.str());
            }

            if (isUCS7604(config.encoder)) {
                // UCS7604: Compare full encoded frame byte-for-byte
                fl::vector<uint8_t> expected_encoded = buildExpectedUCS7604(
                    config.tx_configs[config_idx].mLeds, config.encoder);
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
                            AR_FL_WARN("\n[CORRUPTION @ byte " << static_cast<int>(i) << ", Run " << run
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

                size_t verified_leds = 0;
                for (size_t i = 0; i < num_leds; i++) {
                    size_t byte_offset = rx_buffer_offset + i * 3; // Apply offset for PARLIO front padding
                    if (byte_offset + 2 >= bytes_captured) { // Check against total captured bytes
                        break;
                    }
                    verified_leds = i + 1;

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
                            AR_FL_WARN("\n[CORRUPTION @ LED " << static_cast<int>(i) << ", Run " << run << "]");

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

                // Truncated capture: RX returned fewer bytes than expected. Count
                // the unchecked LEDs as mismatches rather than silently passing.
                // A short capture means we did NOT verify those LEDs, so the run
                // MUST fail. The old silent break hid real bugs on strips longer
                // than the RMT DMA buffer (~170 LEDs for WS2812B). See issue #2254.
                if (verified_leds < num_leds) {
                    size_t unchecked = num_leds - verified_leds;
                    mismatches += static_cast<int>(unchecked);
                    result.mismatchedBytes += static_cast<int>(unchecked * 3);
                    AR_FL_WARN("[TRUNCATED CAPTURE] Only verified " << verified_leds
                            << "/" << num_leds << " LEDs (" << bytes_captured
                            << " bytes captured, needed " << (num_leds * 3)
                            << "). Marking " << unchecked
                            << " unchecked LEDs as mismatches.");
                }
            }

            result.mismatches = mismatches;
            result.passed = (mismatches == 0);
            if (!result.passed) {
                collectRawEdgeDiagnostics(config.rx_channel, &result);
            }
        }

        // Print run result if configured (sstream construction skipped
        // when logs disabled -- see kLogsEnabled note at function top).
        if (kLogsEnabled && (multi_config.print_all_runs || !result.passed)) {
            AR_FL_WARN("[Run " << run << "/" << multi_config.num_runs << "] "
                    << (result.passed ? "PASS" : "FAIL")
                    << " | Errors: " << result.mismatches << "/" << result.total_leds
                    << " (" << (100.0 * (result.total_leds - result.mismatches) / result.total_leds) << "%)");

            // Print error details if enabled
            if (!result.passed && multi_config.print_per_led_errors && !result.errors.empty()) {
                AR_FL_WARN("  First " << result.errors.size() << " error(s):");
                for (size_t i = 0; i < result.errors.size(); i++) {
                    const auto& err = result.errors[i];
                    AR_FL_WARN("    LED[" << err.led_index << "]: expected RGB("
                            << static_cast<int>(err.expected_r) << ","
                            << static_cast<int>(err.expected_g) << ","
                            << static_cast<int>(err.expected_b) << ") got RGB("
                            << static_cast<int>(err.actual_r) << ","
                            << static_cast<int>(err.actual_g) << ","
                            << static_cast<int>(err.actual_b) << ")");
                }
            }
        }

        if (result.passed) {
            ++total_passed;
        } else {
            ++total_failed;
        }

        // The RPC response includes only failed pattern details. Keeping every
        // successful result during a long frame soak needlessly grows the
        // transient heap (and used to make the 16-frame RP PIO run wedge).
        // Retain one representative successful capture for byte-evidence;
        // retain every failure so its full postmortem remains available.
        if (!result.passed || !retained_success || multi_config.num_runs == 1) {
            if (result.passed) {
                retained_success = true;
            }
            run_results.push_back(fl::move(result));
        }
        FastLED.watchdog().feed();
    }

    // Summary statistics
    // Summary FL_WARN -- gated on log level (see kLogsEnabled note above)
    // to prevent heap thrash under autoresearch ScopedLogDisable.
    if (kLogsEnabled) {
        ss.clear();
        ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
        ss << "║ MULTI-RUN SUMMARY\n";
        ss << "╚════════════════════════════════════════════════════════════════╝\n";
        ss << "Total Runs:   " << multi_config.num_runs << "\n";
        ss << "Passed:       " << total_passed << " (" << (100.0 * total_passed / multi_config.num_runs) << "%)\n";
        ss << "Failed:       " << total_failed << " (" << (100.0 * total_failed / multi_config.num_runs) << "%)";
        AR_FL_WARN(ss.str());

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
            AR_FL_WARN(ss.str());
        }
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
        if (kLogsEnabled) {
            AR_FL_WARN("\n[OVERALL] PASS ✓ - All " << multi_config.num_runs << " runs succeeded");
        }
    } else {
        if (kLogsEnabled) {
            AR_FL_WARN("\n[OVERALL] FAIL ✗ - " << total_failed << "/" << multi_config.num_runs << " runs failed");
        }
    }
}

// AutoResearch a specific chipset timing configuration
// Creates channels, runs tests, destroys channels
void autoResearchChipsetTiming(fl::AutoResearchConfig& config,
                           int& driver_total, int& driver_passed,
                           uint32_t& out_show_duration_ms,
                           fl::vector<fl::RunResult>* out_results,
                           int num_runs_per_pattern) {
    fl::sstream ss;
    ss << "\n========================================\n";
    ss << "Testing: " << config.timing_name << "\n";
    bool has_spi_config = config.tx_configs.size() > 0 && config.tx_configs[0].isSpi();
    if (has_spi_config) {
        const auto* spi_cfg = config.tx_configs[0].getSpiChipset();
        ss << "  Protocol: SPI (APA102)\n";
        ss << "  Clock: " << (spi_cfg ? spi_cfg->timing.clock_hz : 0) << " Hz\n";
    } else {
        ss << "  T0H: " << config.timing.t1_ns << "ns\n";
        ss << "  T1H: " << (config.timing.t1_ns + config.timing.t2_ns) << "ns\n";
        ss << "  T0L: " << config.timing.t3_ns << "ns\n";
        ss << "  RESET: " << config.timing.reset_us << "us\n";
    }
    ss << "  Channels: " << config.tx_configs.size() << "\n";
    ss << "========================================";
    AR_FL_WARN(ss.str());

    // Create ALL channels from tx_configs (multi-channel support)
    fl::vector<fl::shared_ptr<fl::Channel>> channels;
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        fl::shared_ptr<fl::Channel> channel;
        if (config.tx_configs[i].isSpi()) {
            // SPI chipset: pass through the SPI config directly
            channel = FastLED.add(config.tx_configs[i]);
        } else {
            // Clockless chipset: re-create with runtime timing
            fl::ChannelConfig channel_config(config.tx_configs[i].getDataPin(), config.timing, config.tx_configs[i].mLeds, config.tx_configs[i].rgb_order);
            channel = FastLED.add(channel_config);
        }
        if (!channel) {
            FL_ERROR("Failed to create channel " << i << " (pin " << config.tx_configs[i].getDataPin() << ") - platform not supported");
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
    // num_runs=1: Python orchestrates retries (default).
    // num_runs>=2: Back-to-back captures of the same buffer to expose second-frame
    // degradation (e.g., SPI driver zeroing its DMA buffer after the first show()).
    multi_config.num_runs = (num_runs_per_pattern > 0) ? num_runs_per_pattern : 1;
    multi_config.print_all_runs = (multi_config.num_runs > 1);
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
    if (fl::strcmp(config.driver_name, "OBJECT_FLED") == 0) {
        fl::objectFledDiagnosticsRecord("afterFastLedClear");
    }
    // Channel destruction is synchronous - no delay needed
}

// AutoResearch using the legacy template addLeds API (supports multi-lane)
// Nearly identical to autoResearchChipsetTiming() — only channel creation differs:
//   Normal:  FastLED.add(channel_config) → Channel
//   Legacy:  LegacyClocklessProxy(pin, leds, numLeds) → WS2812B<PIN> → ClocklessIdf5 → Channel
void autoResearchChipsetTimingLegacy(fl::AutoResearchConfig& config,
                                 int& driver_total, int& driver_passed,
                                 uint32_t& out_show_duration_ms,
                                 fl::vector<fl::RunResult>* out_results,
                                 int num_runs_per_pattern) {
    fl::sstream ss;
    ss << "\n========================================\n";
    ss << "Testing (LEGACY API): " << config.timing_name << "\n";
    ss << "  T0H: " << config.timing.t1_ns << "ns\n";
    ss << "  T1H: " << (config.timing.t1_ns + config.timing.t2_ns) << "ns\n";
    ss << "  T0L: " << config.timing.t3_ns << "ns\n";
    ss << "  RESET: " << config.timing.reset_us << "us\n";
    ss << "  Lanes: " << config.tx_configs.size() << "\n";
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        ss << "  Lane " << i << ": pin=" << config.tx_configs[i].getDataPin()
           << " LEDs=" << config.tx_configs[i].mLeds.size() << "\n";
    }
    ss << "========================================";
    AR_FL_WARN(ss.str());

    // Create one legacy proxy per lane. By default every lane uses WS2812B,
    // but AutoResearch can supply per-lane chipsets to exercise multiple
    // ObjectFLED timing groups in a single FastLED.show().
    fl::vector<fl::unique_ptr<LegacyClocklessProxy>> proxies;
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        int pin = config.tx_configs[i].getDataPin();
        CRGB* leds = config.tx_configs[i].mLeds.data();
        int numLeds = static_cast<int>(config.tx_configs[i].mLeds.size());
        LegacyClocklessChipset chipset = LegacyClocklessChipset::WS2812B;
        if (!config.legacy_chipsets.empty() &&
            i < config.legacy_chipsets.size()) {
            chipset = config.legacy_chipsets[i];
        }

        auto proxy = fl::make_unique<LegacyClocklessProxy>(
            pin, leds, numLeds, chipset, config.legacy_rgbw);
        if (!proxy->valid()) {
            FL_ERROR("Legacy proxy invalid for lane " << i << " (pin " << pin
                     << ", chipset " << legacyClocklessChipsetName(chipset)
                     << " not in supported set)");
            return;  // vector destructor cleans up already-created proxies
        }
        proxies.push_back(fl::move(proxy));
    }

    FastLED.setBrightness(255);

    // Pre-initialize the TX engine to avoid first-call setup delays
    for (size_t i = 0; i < config.tx_configs.size(); i++) {
        fill_solid(config.tx_configs[i].mLeds.data(), config.tx_configs[i].mLeds.size(), CRGB::Black);
    }
    FastLED.show();
    if (!FastLED.wait(1000)) {
        FL_ERROR("[LEGACY] TX wait timeout - driver may be stalled");
        return;  // vector destructor cleans up proxies
    }

    delay(5);  // Buffer drain (same as autoResearchChipsetTiming)

    // Run test patterns (identical to autoResearchChipsetTiming)
    int total = 0;
    int passed = 0;

    fl::MultiRunConfig multi_config;
    multi_config.num_runs = (num_runs_per_pattern > 0) ? num_runs_per_pattern : 1;
    multi_config.print_all_runs = (multi_config.num_runs > 1);
    multi_config.print_per_led_errors = false;
    multi_config.max_errors_per_run = 10;

    // Measure show-only duration (excludes setup/teardown overhead)
    uint32_t show_start_ms = millis();

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

    driver_total += total;
    driver_passed += passed;

    // proxies vector destructor deletes all controllers → ~CLEDController removes from draw list
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

#endif  // !FASTLED_AUTORESEARCH_LOW_MEMORY

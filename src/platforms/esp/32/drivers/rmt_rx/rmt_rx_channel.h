#pragma once

#include "fl/compiler_control.h"

#ifdef ESP32

#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "fl/stl/array.h"
#include "fl/stl/iterator.h"
#include "fl/result.h"
#include "fl/rx_device.h"

// RMT symbol is a 32-bit value (union with duration0/level0/duration1/level1 bitfields)
// We expose uint32_t in the interface to avoid ESP-IDF header dependencies
// Implementation will cast between uint32_t and rmt_symbol_word_t (static_assert size match)
using RmtSymbol = uint32_t;

namespace fl {

/**
 * @brief RMT RX Channel Interface
 *
 * Provides RAII-based management of RMT receive channels with automatic internal buffering.
 * Follows FastLED conventions:
 * - Uses fl:: namespace
 * - IRAM_ATTR for ISR callbacks
 * - FL_DBG for debug logging
 * - Result<T, E> for error handling
 *
 * Recommended API with wait() and decode():
 * @code
 * auto rx = RmtRxChannel::create(6, 40000000, 1024);  // GPIO 6, 40MHz, 1024 symbols
 *
 * // Initialize with custom signal range (optional - defaults to 100ns/100μs)
 * if (!rx->begin(100, 100000)) {  // min=100ns, max=100μs
 *     FL_WARN("RX channel init failed");
 *     return;
 * }
 *
 * // Wait for data with 100ms timeout
 * if (rx->wait(100) == RxWaitResult::SUCCESS) {
 *     // Convert 3-phase TX timing to 4-phase RX timing
 *     fl::ChipsetTiming ws2812b_tx{fl::TIMING_WS2812_800KHZ::T1, fl::TIMING_WS2812_800KHZ::T2,
 *                                   fl::TIMING_WS2812_800KHZ::T3, fl::TIMING_WS2812_800KHZ::RESET, "WS2812B"};
 *     auto rx_timing = fl::make4PhaseTiming(ws2812b_tx, 150);
 *
 *     // Decode to bytes
 *     fl::vector<uint8_t> bytes;
 *     auto result = rx->decode(rx_timing, fl::back_inserter(bytes));
 *     if (result.ok()) {
 *         FL_DBG("Decoded " << result.value() << " bytes");
 *     }
 * }
 * @endcode
 */
class RmtRxChannel : public RxDevice {
public:
    /**
     * @brief Create RX channel instance (does not initialize hardware)
     * @param pin GPIO pin number for receiving signals
     * @return Shared pointer to RmtRxChannel interface
     *
     * Hardware parameters (resolution_hz, buffer_size) are passed via RxConfig in begin().
     *
     * Example:
     * @code
     * auto rx = RmtRxChannel::create(6);  // GPIO 6
     * RxConfig config;
     * config.buffer_size = 1024;
     * config.hz = 40000000;  // Optional: 40MHz clock
     * rx->begin(config);
     * @endcode
     */
    static fl::shared_ptr<RmtRxChannel> create(int pin);

    /**
     * @brief Virtual destructor
     */
    virtual ~RmtRxChannel() = default;

    /**
     * @brief Initialize (or re-arm) RMT RX channel with configuration
     * @param config RX configuration (signal ranges, edge detection, skip count)
     * @return true on success, false on failure
     *
     * First call: Creates RMT RX channel, registers ISR callback, and arms receiver
     * Subsequent calls: Re-arms the receiver for a new capture (clears state, re-enables channel)
     *
     * The receiver is automatically armed and ready to capture after begin() returns.
     * This allows loopback validation: call begin(), then transmit, then poll finished().
     *
     * Configuration Parameters:
     * - signal_range_min_ns: Pulses shorter than this are ignored (noise filtering)
     * - signal_range_max_ns: Pulses longer than this terminate reception (idle detection)
     * - skip_signals: Number of symbols to skip before capturing (useful for memory-constrained environments)
     * - start_low: Pin idle state for edge detection (true=LOW for WS2812B, false=HIGH for inverted)
     *
     * Edge Detection:
     * Automatically skips symbols captured before TX starts transmitting by detecting the first
     * edge transition (rising for start_low=true, falling for start_low=false).
     *
     * Example values:
     * - WS2812B: min=100ns, max=100000ns (100μs), start_low=true
     * - SK6812: min=100ns, max=100000ns (100μs), start_low=true
     * - APA102: min=50ns, max=50000ns (50μs), start_low=true
     *
     * Example with skip_signals:
     * @code
     * auto rx = RmtRxChannel::create(6, 40000000, 100);  // 100 symbol buffer
     * RxConfig config{100, 100000, 900};  // Skip first 900 symbols
     * rx->begin(config);
     * // Transmit 1000 symbols
     * // Result: Only last 100 symbols captured in buffer
     * @endcode
     */
    virtual bool begin(const RxConfig& config) override = 0;

    /**
     * @brief Check if receive operation is complete
     * @return true if receive finished, false if still in progress
     */
    virtual bool finished() const override = 0;

    /**
     * @brief Wait for RMT symbols with timeout
     * @param timeout_ms Timeout in milliseconds
     * @return RxWaitResult - SUCCESS, TIMEOUT, or BUFFER_OVERFLOW
     *
     * This is an alternative API for receiving RMT symbols when you need
     * precise timing control. After successful wait, use decode() to convert
     * the captured symbols to bytes.
     *
     * Behavior:
     * - Uses internal buffer size specified in create()
     * - Starts RX receive operation automatically
     * - Busy-waits with yield() until data received or timeout
     * - Returns SUCCESS if buffer filled OR receive completes before timeout
     * - Returns TIMEOUT if timeout occurs
     * - Returns BUFFER_OVERFLOW if buffer was too small
     *
     * Success cases:
     * 1. Buffer filled (reached buffer_size symbols)
     * 2. Receive completed naturally (reset pulse detected) before timeout
     *
     * Note: For loopback validation, use begin() to arm the receiver,
     * then transmit, then poll finished() manually for precise control.
     *
     * Example:
     * @code
     * auto rx = RmtRxChannel::create(6, 40000000, 1000);
     * rx->begin(100, 100000);  // min=100ns, max=100μs (or use defaults)
     *
     * // Wait up to 50ms
     * if (rx->wait(50) == RxWaitResult::SUCCESS) {
     *     // Decode captured symbols
     *     fl::ChipsetTiming ws2812b{fl::TIMING_WS2812_800KHZ::T1, fl::TIMING_WS2812_800KHZ::T2,
     *                               fl::TIMING_WS2812_800KHZ::T3, fl::TIMING_WS2812_800KHZ::RESET, "WS2812B"};
     *     auto rx_timing = fl::make4PhaseTiming(ws2812b, 150);
     *     fl::vector<uint8_t> bytes;
     *     rx->decode(rx_timing, fl::back_inserter(bytes));
     * }
     * @endcode
     */
    virtual RxWaitResult wait(uint32_t timeout_ms) override = 0;

    /**
     * @brief Get the RMT clock resolution in Hz
     * @return Resolution in Hz (e.g., 40000000 for 40MHz)
     */
    virtual uint32_t getResolutionHz() const = 0;

    /**
     * @brief Decode received RMT symbols to bytes into a span
     * @param timing Chipset timing thresholds for bit detection
     * @param out Output span to write decoded bytes
     * @return Result with total bytes decoded, or error
     *
     * This method writes directly to a span.
     * Returns the number of bytes written, which may be less than the span size
     * if the decoded data is smaller than the buffer.
     *
     * Example:
     * @code
     * auto rx = RmtRxChannel::create(6, 40000000, 1024);
     * rx->begin(100, 100000);  // min=100ns, max=100μs
     *
     * // Wait for symbols
     * if (rx->wait(50) == RxWaitResult::SUCCESS) {
     *     fl::ChipsetTiming ws2812b{fl::TIMING_WS2812_800KHZ::T1, fl::TIMING_WS2812_800KHZ::T2,
     *                               fl::TIMING_WS2812_800KHZ::T3, fl::TIMING_WS2812_800KHZ::RESET, "WS2812B"};
     *     auto rx_timing = fl::make4PhaseTiming(ws2812b, 150);
     *     uint8_t buffer[256];
     *     auto result = rx->decode(rx_timing, buffer);
     *     if (result.ok()) {
     *         FL_DBG("Decoded " << result.value() << " bytes");
     *     }
     * }
     * @endcode
     */
    virtual fl::Result<uint32_t, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                                       fl::span<uint8_t> out) override = 0;

    /**
     * @brief Get raw edge timings in universal format (for debugging)
     * @param out Output span to write EdgeTime entries (pre-allocated by caller)
     * @return Number of EdgeTime entries written (may be less than out.size() if insufficient data)
     *
     * Converts RMT symbols to universal EdgeTime format (high/low + nanoseconds).
     * Each RMT symbol produces 2 EdgeTime entries (duration0/level0, duration1/level1).
     *
     * Example:
     * @code
     * auto rx = RmtRxChannel::create(6);  // GPIO 6
     * RxConfig config;
     * config.buffer_size = 512;
     * config.hz = 40000000;
     * rx->begin(config);
     * rx->wait(100);
     *
     * // Print each edge
     * EdgeTime edges[100];
     * size_t count = rx->getRawEdgeTimes(edges);
     * for (size_t i = 0; i < count; i++) {
     *     FL_DBG((edges[i].high ? "HIGH " : "LOW ") << edges[i].ns << "ns");
     * }
     * @endcode
     */
    virtual size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) override = 0;

    /**
     * @brief Get device type name
     * @return "RMT" for RMT-based receiver
     */
    virtual const char* name() const override = 0;

    /**
     * @brief Manually inject edge timings for testing (Phase 1)
     * @param edges Span of EdgeTime entries to inject (nanosecond timings)
     * @return true on success, false on failure
     *
     * Converts EdgeTime (nanosecond) to RMT symbols (ticks) and stores
     * in internal buffer. After injection, use decode() to process edges.
     */
    virtual bool injectEdges(fl::span<const EdgeTime> edges) override = 0;

protected:
    RmtRxChannel() = default;

    /**
     * @brief Get received symbols as a span (internal method for decode())
     * @return Span of const RmtSymbol containing received data
     */
    virtual fl::span<const RmtSymbol> getReceivedSymbols() const = 0;
};

} // namespace fl

#endif // ESP32

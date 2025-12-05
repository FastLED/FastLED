#pragma once

#include "ftl/stdint.h"
#include "ftl/shared_ptr.h"
#include "ftl/span.h"
#include "ftl/vector.h"
#include "ftl/array.h"
#include "fl/result.h"

// RMT symbol is a 32-bit value (union with duration0/level0/duration1/level1 bitfields)
// We expose uint32_t in the interface to avoid ESP-IDF header dependencies
// Implementation will cast between uint32_t and rmt_symbol_word_t (static_assert size match)
using RmtSymbol = uint32_t;

namespace fl {
namespace esp32 {

/**
 * @brief Error codes for RMT decoder operations
 */
enum class DecodeError : uint8_t {
    OK = 0,                  ///< No error (not typically used)
    HIGH_ERROR_RATE,         ///< Symbol decode error rate too high (>10%)
    BUFFER_OVERFLOW,         ///< Output buffer overflow
    INVALID_ARGUMENT         ///< Invalid input arguments
};

/**
 * @brief RX timing thresholds for chipset detection
 *
 * Defines acceptable timing ranges for decoding RMT symbols back to bits.
 * Uses min/max ranges to tolerate signal jitter and hardware variations.
 *
 * Thresholds should be ±150ns wider than nominal TX timing to account for:
 * - Clock drift between TX and RX
 * - Signal propagation delays
 * - LED capacitance effects
 * - GPIO sampling jitter
 */
struct ChipsetTimingRx {
    // Bit 0 timing thresholds
    uint32_t t0h_min_ns; ///< Bit 0 high time minimum (e.g., 250ns)
    uint32_t t0h_max_ns; ///< Bit 0 high time maximum (e.g., 550ns)
    uint32_t t0l_min_ns; ///< Bit 0 low time minimum (e.g., 700ns)
    uint32_t t0l_max_ns; ///< Bit 0 low time maximum (e.g., 1000ns)

    // Bit 1 timing thresholds
    uint32_t t1h_min_ns; ///< Bit 1 high time minimum (e.g., 650ns)
    uint32_t t1h_max_ns; ///< Bit 1 high time maximum (e.g., 950ns)
    uint32_t t1l_min_ns; ///< Bit 1 low time minimum (e.g., 300ns)
    uint32_t t1l_max_ns; ///< Bit 1 low time maximum (e.g., 600ns)

    // Reset pulse threshold
    uint32_t reset_min_us; ///< Reset pulse minimum duration (e.g., 50us)
};

/**
 * @brief WS2812B RX timing thresholds
 *
 * Based on datasheet specs with ±150ns tolerance:
 * - T0H: 400ns ±150ns → [250ns, 550ns]
 * - T0L: 850ns ±150ns → [700ns, 1000ns]
 * - T1H: 800ns ±150ns → [650ns, 950ns]
 * - T1L: 450ns ±150ns → [300ns, 600ns]
 * - RESET: 280us minimum (WS2812-V5B datasheet)
 */
constexpr ChipsetTimingRx CHIPSET_TIMING_WS2812B_RX = {.t0h_min_ns = 250,
                                                       .t0h_max_ns = 550,
                                                       .t0l_min_ns = 700,
                                                       .t0l_max_ns = 1000,
                                                       .t1h_min_ns = 650,
                                                       .t1h_max_ns = 950,
                                                       .t1l_min_ns = 300,
                                                       .t1l_max_ns = 600,
                                                       .reset_min_us = 280};

/**
 * @brief Result codes for RMT RX wait() operations
 */
enum class RmtRxWaitResult : uint8_t {
    SUCCESS = 0,             ///< Operation completed successfully
    TIMEOUT = 1,             ///< Operation timed out
    BUFFER_OVERFLOW = 2      ///< Buffer overflow
};

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
 * if (!rx->begin()) {
 *     FL_WARN("RX channel init failed");
 *     return;
 * }
 *
 * // Wait for data with 100ms timeout
 * if (rx->wait(100) == RmtRxWaitResult::SUCCESS) {
 *     // Decode to bytes
 *     fl::HeapVector<uint8_t> bytes;
 *     auto result = rx->decode(CHIPSET_TIMING_WS2812B_RX, fl::back_inserter(bytes));
 *     if (result.ok()) {
 *         FL_DBG("Decoded " << result.value() << " bytes");
 *     }
 * }
 * @endcode
 */
class RmtRxChannel {
public:
    /**
     * @brief Create RX channel instance (does not initialize hardware)
     * @param pin GPIO pin number for receiving signals
     * @param resolution_hz RMT clock resolution (default 40MHz for 25ns ticks)
     * @param max_buffer_size Internal buffer size in symbols (default 1024, -1 for no limit)
     * @return Shared pointer to RmtRxChannel interface
     */
    static fl::shared_ptr<RmtRxChannel> create(int pin, uint32_t resolution_hz = 40000000, int32_t max_buffer_size = 1024);

    /**
     * @brief Virtual destructor
     */
    virtual ~RmtRxChannel() = default;

    /**
     * @brief Initialize (or re-arm) RMT RX channel
     * @return true on success, false on failure
     *
     * First call: Creates RMT RX channel, registers ISR callback, and arms receiver
     * Subsequent calls: Re-arms the receiver for a new capture (clears state, re-enables channel)
     *
     * The receiver is automatically armed and ready to capture after begin() returns.
     * This allows loopback validation: call begin(), then transmit, then poll finished().
     */
    virtual bool begin() = 0;

    /**
     * @brief Check if receive operation is complete
     * @return true if receive finished, false if still in progress
     */
    virtual bool finished() const = 0;

    /**
     * @brief Wait for RMT symbols with timeout
     * @param timeout_ms Timeout in milliseconds
     * @return RmtRxWaitResult - SUCCESS, TIMEOUT, or BUFFER_OVERFLOW
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
     * rx->begin();
     *
     * // Wait up to 50ms
     * if (rx->wait(50) == RmtRxWaitResult::SUCCESS) {
     *     // Decode captured symbols
     *     fl::HeapVector<uint8_t> bytes;
     *     rx->decode(CHIPSET_TIMING_WS2812B_RX, fl::back_inserter(bytes));
     * }
     * @endcode
     */
    virtual RmtRxWaitResult wait(uint32_t timeout_ms) = 0;

    /**
     * @brief Get the RMT clock resolution in Hz
     * @return Resolution in Hz (e.g., 40000000 for 40MHz)
     */
    virtual uint32_t getResolutionHz() const = 0;

    /**
     * @brief Decode received RMT symbols to bytes (convenience method)
     * @tparam OutputIteratorUint8 Output iterator type (e.g., fl::back_inserter)
     * @param timing Chipset timing thresholds for bit detection
     * @param out Output iterator for decoded bytes
     * @return Result with total bytes decoded, or error
     *
     * This is a high-level convenience wrapper that:
     * - Uses the channel's internal resolution_hz automatically
     * - Decodes the received symbols from the last receive operation
     * - Decodes all received symbols until end or reset pulse
     *
     * Example:
     * @code
     * auto rx = RmtRxChannel::create(6, 40000000, 1024);
     * rx->begin();
     *
     * // Wait for symbols
     * if (rx->wait(50) == RmtRxWaitResult::SUCCESS) {
     *     // Decode to vector
     *     fl::HeapVector<uint8_t> bytes;
     *     auto result = rx->decode(CHIPSET_TIMING_WS2812B_RX,
     *                              fl::back_inserter(bytes));
     *     if (result.ok()) {
     *         FL_DBG("Decoded " << result.value() << " bytes");
     *     }
     * }
     * @endcode
     */
    template<typename OutputIteratorUint8>
    fl::Result<uint32_t, DecodeError> decode(const ChipsetTimingRx &timing,
                                              OutputIteratorUint8 out);

protected:
    RmtRxChannel() = default;

    /**
     * @brief Get received symbols as a span (internal method for decode())
     * @return Span of const RmtSymbol containing received data
     */
    virtual fl::span<const RmtSymbol> getReceivedSymbols() const = 0;
};

} // namespace esp32
} // namespace fl

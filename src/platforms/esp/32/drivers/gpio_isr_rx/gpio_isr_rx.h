#pragma once

#include "fl/stl/stdint.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/result.h"
#include "fl/rx_device.h"

namespace fl {

/**
 * @brief Edge timestamp captured by GPIO ISR
 *
 * Stores the timestamp in nanoseconds (relative to capture start)
 * and the GPIO level at the time of the edge.
 *
 * Note: ISR writes raw CPU cycles, main thread converts to nanoseconds
 * via getEdges(). This avoids slow time conversions in ISR.
 */
struct EdgeTimestamp {
    union {
        uint32_t time_ns;    ///< Timestamp in nanoseconds (after conversion)
        uint32_t cycles;     ///< CPU cycles (during ISR capture)
    };
    uint8_t level;       ///< GPIO level (0 or 1)
};

/**
 * @brief GPIO ISR-Based RX Channel Interface
 *
 * Provides ISR-based edge detection receiver that captures GPIO edge toggle
 * timestamps with ±1µs precision using fast timer polling (2µs interval).
 *
 * Features:
 * - Timer polling: 2µs interval (500kHz rate) for ±1µs precision
 * - Edge detection: Timer-driven GPIO register polling
 * - Timestamp precision: CPU cycle counter (nanosecond resolution)
 * - Output: Array of edge change timestamps
 * - Buffer: Pre-allocated (no dynamic allocation in ISR)
 * - Disarm conditions:
 *   - Buffer full
 *   - Idle timeout elapsed (configurable)
 * - Restrictions: No FL_WARN in ISR (ISR-safe logging only)
 * - WARNING: 2µs polling interval below ESP-IDF recommended 5µs minimum
 *
 * Pin Configuration:
 * - IMPORTANT: The pin must be configured (pinMode) by the user BEFORE calling begin()
 * - This ISR RX device only sets up interrupt handling - it does NOT change pin mode
 * - Pin can be INPUT or OUTPUT - GPIO_IN_REG register reads work for both modes
 * - Rationale: Allows validation of OUTPUT pins (LED drivers) without reconfiguration
 *
 * Usage Example:
 * @code
 * // Configure pin first (user's responsibility)
 * pinMode(6, INPUT);  // or OUTPUT - both work
 *
 * auto rx = GpioIsrRx::create(6, 1024);  // GPIO 6, 1024 edge buffer
 *
 * // Initialize with custom signal range (optional - defaults to 100ns/100μs)
 * if (!rx->begin(100, 100000)) {  // min=100ns, max=100μs
 *     FL_WARN("GPIO ISR RX init failed");
 *     return;
 * }
 *
 * // Wait for data with 100ms timeout
 * if (rx->wait(100) == RxWaitResult::SUCCESS) {
 *     // Get captured edges
 *     fl::span<const EdgeTimestamp> edges = rx->getEdges();
 *     FL_DBG("Captured " << edges.size() << " edges");
 * }
 * @endcode
 */
class GpioIsrRx : public RxDevice {
public:
    /**
     * @brief Create GPIO ISR RX instance (does not initialize hardware)
     * @param pin GPIO pin number for receiving signals
     * @return Shared pointer to GpioIsrRx interface
     *
     * Hardware parameters (buffer_size) are passed via RxConfig in begin().
     *
     * Example:
     * @code
     * auto rx = GpioIsrRx::create(6);  // GPIO 6
     * RxConfig config;
     * config.buffer_size = 1024;
     * rx->begin(config);
     * @endcode
     */
    static fl::shared_ptr<GpioIsrRx> create(int pin);

    /**
     * @brief Virtual destructor
     */
    virtual ~GpioIsrRx() = default;

    /**
     * @brief Initialize (or re-arm) GPIO ISR RX channel with configuration
     * @param config RX configuration (signal ranges, edge detection, skip count)
     * @return true on success, false on failure
     *
     * IMPORTANT: Pin must be configured with pinMode() BEFORE calling begin()
     *
     * First call: Installs ISR service and arms receiver (does NOT change pin mode)
     * Subsequent calls: Re-arms the receiver for a new capture (clears state, re-enables ISR)
     *
     * The receiver is automatically armed and ready to capture after begin() returns.
     *
     * Configuration Parameters:
     * - signal_range_min_ns: Pulses shorter than this are ignored (ISR-level noise filtering)
     * - signal_range_max_ns: Pulses longer than this terminate reception (idle detection timeout)
     * - skip_signals: Number of edges to skip before capturing (useful for memory-constrained environments)
     * - start_low: Pin idle state for edge detection (true=LOW for WS2812B, false=HIGH for inverted)
     *
     * Edge Detection:
     * Automatically skips edges captured before TX starts transmitting by detecting the first
     * edge transition (rising for start_low=true, falling for start_low=false).
     *
     * Example with skip_signals:
     * @code
     * pinMode(6, INPUT);  // Configure pin first!
     * auto rx = GpioIsrRx::create(6, 100);  // 100 edge buffer
     * RxConfig config{100, 100000, 900};  // Skip first 900 edges
     * rx->begin(config);
     * // Transmit 1000 edges
     * // Result: Only last 100 edges captured in buffer
     * @endcode
     */
    virtual bool begin(const RxConfig& config) override = 0;

    /**
     * @brief Check if receive operation is complete
     * @return true if receive finished, false if still in progress
     */
    virtual bool finished() const override = 0;

    /**
     * @brief Wait for edge timestamps with timeout
     * @param timeout_ms Timeout in milliseconds
     * @return RxWaitResult - SUCCESS, TIMEOUT, or BUFFER_OVERFLOW
     *
     * Behavior:
     * - Uses internal buffer size specified in create()
     * - Starts edge capture operation automatically
     * - Busy-waits with yield() until data received or timeout
     * - Returns SUCCESS if buffer filled OR idle timeout elapsed
     * - Returns TIMEOUT if wait timeout occurs
     * - Returns BUFFER_OVERFLOW if buffer was too small
     *
     * Success cases:
     * 1. Buffer filled (reached buffer_size edges)
     * 2. Idle timeout elapsed since first edge (signal_range_max_ns from begin())
     */
    virtual RxWaitResult wait(uint32_t timeout_ms) override = 0;

    /**
     * @brief Get captured edge timestamps as a span
     * @return Span of const EdgeTimestamp containing captured edges
     *
     * Returns edges captured since the last begin() call.
     * The span remains valid until the next begin() call.
     */
    virtual fl::span<const EdgeTimestamp> getEdges() const = 0;

    /**
     * @brief Decode captured edges to bytes into a span
     * @param timing Chipset timing thresholds for bit detection
     * @param out Output span to write decoded bytes
     * @return Result with total bytes decoded, or error
     *
     * Converts edge timestamps to pulse durations, then decodes pulses to bytes
     * using the provided timing thresholds.
     *
     * Example:
     * @code
     * auto rx = GpioIsrRx::create(6, 1024);
     * rx->begin(100, 100000);  // min=100ns, max=100μs (or use defaults)
     *
     * // Wait for edges
     * if (rx->wait(50) == RxWaitResult::SUCCESS) {
     *     uint8_t buffer[256];
     *     auto rx_timing = makeRxTiming(TIMING_WS2812_800KHZ);
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
     * @brief Manually inject edge timings for testing (Phase 1)
     * @param edges Span of EdgeTime entries to inject (nanosecond timings)
     * @return true on success, false on failure
     *
     * Stores EdgeTime entries directly in internal edge buffer as EdgeTimestamp.
     * After injection, use decode() to process edges.
     */
    virtual bool injectEdges(fl::span<const EdgeTime> edges) override = 0;

protected:
    GpioIsrRx() = default;
};

} // namespace fl

/**
 * @file gpio_isr_rx.h
 * @brief GPIO ISR-Based RX Channel Interface (redirects to MCPWM implementation)
 *
 * This header provides the public GPIO ISR RX interface and redirects to
 * the dual-ISR MCPWM implementation for ESP32-C6 and compatible platforms.
 */

#pragma once

// IWYU pragma: private

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
 */
struct EdgeTimestamp {
    union {
        u32 time_ns;    ///< Timestamp in nanoseconds (after conversion)
        u32 cycles;     ///< CPU cycles (during ISR capture)
    };
    u8 level;       ///< GPIO level (0 or 1)
};

/**
 * @brief GPIO ISR-Based RX Channel Interface
 *
 * Provides ISR-based edge detection receiver that captures GPIO edge toggle
 * timestamps with hardware precision using MCPWM timer (12.5 ns resolution).
 *
 * Architecture:
 * - Fast ISR (RISC-V assembly): Captures edges with MCPWM timestamps (~130 ns latency)
 * - Slow ISR (C, GPTimer 10µs): Processes buffer, applies filtering, manages completion
 *
 * Features:
 * - Hardware timestamps: MCPWM timer at 80 MHz (12.5 ns resolution)
 * - Ultra-fast edge capture: <130 ns ISR latency
 * - Circular buffer: Lock-free communication between ISRs
 * - Filtering: Skip signals, jitter filter, timeout detection
 * - Edge capture rate: >1 MHz
 *
 * Pin Configuration:
 * - IMPORTANT: The pin must be configured (pinMode) by the user BEFORE calling begin()
 * - This ISR RX device only sets up interrupt handling - it does NOT change pin mode
 *
 * Usage Example:
 * @code
 * // Configure pin first (user's responsibility)
 * pinMode(6, INPUT);
 *
 * auto rx = GpioIsrRx::create(6);
 *
 * // Initialize with configuration
 * RxConfig config;
 * config.buffer_size = 256;  // Must be power of 2
 * config.signal_range_min_ns = 100;
 * config.signal_range_max_ns = 100000;
 * config.skip_signals = 0;
 *
 * if (!rx->begin(config)) {
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
     * @brief Create GPIO ISR RX instance with MCPWM hardware timestamps
     * @param pin GPIO pin number for receiving signals
     * @return Shared pointer to GpioIsrRx interface
     *
     * Hardware parameters (buffer_size) are passed via RxConfig in begin().
     *
     * IMPORTANT: Buffer size must be a power of 2 (e.g., 64, 128, 256, 512)
     * for optimal performance (fast modulo optimization).
     *
     * Example:
     * @code
     * auto rx = GpioIsrRx::create(6);  // GPIO 6
     * RxConfig config;
     * config.buffer_size = 256;  // Power of 2 required
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
     * IMPORTANT: buffer_size must be power of 2 (e.g., 64, 128, 256, 512)
     *
     * First call: Installs ISR service and arms receiver
     * Subsequent calls: Re-arms the receiver for a new capture
     *
     * Configuration Parameters:
     * - buffer_size: Circular buffer size (MUST be power of 2)
     * - signal_range_min_ns: Pulses shorter than this are ignored (jitter filter)
     * - signal_range_max_ns: Pulses longer than this terminate reception (idle timeout)
     * - skip_signals: Number of edges to skip before capturing
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
     * - Starts edge capture operation automatically
     * - Busy-waits with yield() until data received or timeout
     * - Returns SUCCESS if buffer filled OR idle timeout elapsed
     * - Returns TIMEOUT if wait timeout occurs
     */
    virtual RxWaitResult wait(u32 timeout_ms) override = 0;

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
     */
    virtual fl::Result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                                       fl::span<u8> out) override = 0;

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

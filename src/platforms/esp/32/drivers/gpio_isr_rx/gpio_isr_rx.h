#pragma once

#include "ftl/stdint.h"
#include "ftl/shared_ptr.h"
#include "ftl/span.h"
#include "fl/result.h"

// Forward declare types from rmt_rx_channel.h to avoid circular dependency
namespace fl {
    enum class DecodeError : uint8_t;
    struct ChipsetTimingRx;
}

namespace fl {

/**
 * @brief Edge timestamp captured by GPIO ISR
 *
 * Stores the timestamp (in nanoseconds relative to capture start)
 * and the GPIO level at the time of the edge.
 */
struct EdgeTimestamp {
    uint32_t time_ns;    ///< Timestamp in nanoseconds (relative to capture start)
    uint8_t level;       ///< GPIO level (0 or 1)
};

/**
 * @brief Result codes for GPIO ISR RX wait() operations
 */
enum class GpioIsrRxWaitResult : uint8_t {
    SUCCESS = 0,             ///< Operation completed successfully
    TIMEOUT = 1,             ///< Operation timed out
    BUFFER_OVERFLOW = 2      ///< Buffer overflow
};

/**
 * @brief GPIO ISR-Based RX Channel Interface
 *
 * Provides ISR-based edge detection receiver that captures GPIO edge toggle
 * timestamps with 100ns precision at priority 3, outputting to a pre-allocated buffer.
 *
 * Features:
 * - ISR priority: 3
 * - Edge detection: Hardware-triggered (ANY_EDGE)
 * - Timestamp precision: Microsecond resolution (sufficient for 100ns requirement)
 * - Output: Array of edge change timestamps
 * - Buffer: Pre-allocated (no dynamic allocation in ISR)
 * - Disarm conditions:
 *   - Buffer full
 *   - 50µs timeout elapsed
 * - Restrictions: No FL_WARN in ISR (ISR-safe logging only)
 *
 * Usage Example:
 * @code
 * auto rx = GpioIsrRx::create(6, 1024);  // GPIO 6, 1024 edge buffer
 *
 * // Initialize with custom signal range (optional - defaults to 100ns/100μs)
 * if (!rx->begin(100, 100000)) {  // min=100ns, max=100μs
 *     FL_WARN("GPIO ISR RX init failed");
 *     return;
 * }
 *
 * // Wait for data with 100ms timeout
 * if (rx->wait(100) == GpioIsrRxWaitResult::SUCCESS) {
 *     // Get captured edges
 *     fl::span<const EdgeTimestamp> edges = rx->getEdges();
 *     FL_DBG("Captured " << edges.size() << " edges");
 * }
 * @endcode
 */
class GpioIsrRx {
public:
    /**
     * @brief Create GPIO ISR RX instance (does not initialize hardware)
     * @param pin GPIO pin number for receiving signals
     * @param buffer_size Buffer size for edge timestamps (default 1024)
     * @return Shared pointer to GpioIsrRx interface
     */
    static fl::shared_ptr<GpioIsrRx> create(int pin, size_t buffer_size = 1024);

    /**
     * @brief Virtual destructor
     */
    virtual ~GpioIsrRx() = default;

    /**
     * @brief Initialize (or re-arm) GPIO ISR RX channel
     * @param signal_range_min_ns Minimum pulse width in nanoseconds (default: 100ns, filters glitches)
     * @param signal_range_max_ns Maximum pulse width in nanoseconds (default: 100000ns = 100μs, idle threshold)
     * @param skip_signals Number of edges to skip before capturing (default: 0, capture all)
     * @return true on success, false on failure
     *
     * First call: Configures GPIO, installs ISR service, and arms receiver
     * Subsequent calls: Re-arms the receiver for a new capture (clears state, re-enables ISR)
     *
     * The receiver is automatically armed and ready to capture after begin() returns.
     *
     * Signal Range Parameters:
     * - signal_range_min_ns: Pulses shorter than this are ignored (noise filtering)
     *   ISR-level filtering discards edges that create pulses shorter than this threshold.
     * - signal_range_max_ns: Pulses longer than this terminate reception (idle detection)
     *   When no edges are received for this duration, the receiver automatically stops.
     * - skip_signals: Number of edges to skip before capturing (useful for memory-constrained environments)
     *   When skip_signals > 0, the ISR will discard the first N edges, then begin writing
     *   to the buffer starting with edge N+1.
     *
     * These parameters control noise filtering and idle detection behavior, allowing
     * tuning for different LED protocols and signal characteristics.
     *
     * Example with skip_signals:
     * @code
     * auto rx = GpioIsrRx::create(6, 100);  // 100 edge buffer
     * rx->begin(100, 100000, 900);  // Skip first 900 edges
     * // Transmit 1000 edges
     * // Result: Only last 100 edges captured in buffer
     * @endcode
     */
    virtual bool begin(uint32_t signal_range_min_ns = 100, uint32_t signal_range_max_ns = 100000, uint32_t skip_signals = 0) = 0;

    /**
     * @brief Check if receive operation is complete
     * @return true if receive finished, false if still in progress
     */
    virtual bool finished() const = 0;

    /**
     * @brief Wait for edge timestamps with timeout
     * @param timeout_ms Timeout in milliseconds
     * @return GpioIsrRxWaitResult - SUCCESS, TIMEOUT, or BUFFER_OVERFLOW
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
    virtual GpioIsrRxWaitResult wait(uint32_t timeout_ms) = 0;

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
     * if (rx->wait(50) == GpioIsrRxWaitResult::SUCCESS) {
     *     uint8_t buffer[256];
     *     auto result = rx->decode(CHIPSET_TIMING_WS2812B_RX, buffer);
     *     if (result.ok()) {
     *         FL_DBG("Decoded " << result.value() << " bytes");
     *     }
     * }
     * @endcode
     */
    virtual fl::Result<uint32_t, DecodeError> decode(const ChipsetTimingRx &timing,
                                                       fl::span<uint8_t> out) = 0;

protected:
    GpioIsrRx() = default;
};

} // namespace fl

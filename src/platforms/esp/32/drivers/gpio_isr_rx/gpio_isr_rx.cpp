#include "gpio_isr_rx.h"

#ifdef ESP32

// Include feature flags to detect FASTLED_RMT5
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/dbg.h"
#include "fl/warn.h"
#include "ftl/vector.h"
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_timer.h"  // For esp_timer_get_time()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"  // For taskYIELD()
FL_EXTERN_C_END

namespace fl {

// ============================================================================
// Edge Timestamp Decoder Implementation (private to this file)
// ============================================================================

namespace {

/**
 * @brief Decode single pulse to bit value
 * @param high_ns High time in nanoseconds
 * @param low_ns Low time in nanoseconds
 * @param timing Timing thresholds
 * @return 0 for bit 0, 1 for bit 1, -1 for invalid pulse
 *
 * Checks high time and low time against timing thresholds.
 * Returns -1 if timing doesn't match either bit pattern.
 */
inline int decodePulseBit(uint32_t high_ns, uint32_t low_ns, const ChipsetTimingRx &timing) {
    // Decision logic: check if timing matches bit 0 pattern
    bool t0h_match = (high_ns >= timing.t0h_min_ns) && (high_ns <= timing.t0h_max_ns);
    bool t0l_match = (low_ns >= timing.t0l_min_ns) && (low_ns <= timing.t0l_max_ns);

    if (t0h_match && t0l_match) {
        return 0; // Bit 0
    }

    // Check if timing matches bit 1 pattern
    bool t1h_match = (high_ns >= timing.t1h_min_ns) && (high_ns <= timing.t1h_max_ns);
    bool t1l_match = (low_ns >= timing.t1l_min_ns) && (low_ns <= timing.t1l_max_ns);

    if (t1h_match && t1l_match) {
        return 1; // Bit 1
    }

    // Timing doesn't match either pattern
    return -1; // Invalid
}

/**
 * @brief Check if pulse duration indicates a reset pulse
 * @param duration_ns Pulse duration in nanoseconds
 * @param timing Timing thresholds
 * @return true if pulse is reset pulse (long low duration)
 */
inline bool isResetPulse(uint32_t duration_ns, const ChipsetTimingRx &timing) {
    uint32_t reset_min_ns = timing.reset_min_us * 1000;
    return duration_ns >= reset_min_ns;
}

/**
 * @brief Decode edge timestamps to bytes (span-based implementation)
 * Internal implementation - not exposed in public header
 */
fl::Result<uint32_t, DecodeError> decodeEdgeTimestamps(const ChipsetTimingRx &timing,
                                                         fl::span<const EdgeTimestamp> edges,
                                                         fl::span<uint8_t> bytes_out) {
    if (edges.empty()) {
        FL_WARN("decodeEdgeTimestamps: edges span is empty");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    if (bytes_out.empty()) {
        FL_WARN("decodeEdgeTimestamps: bytes_out span is empty");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    FL_DBG("decodeEdgeTimestamps: decoding " << edges.size() << " edges into buffer of " << bytes_out.size() << " bytes");

    // Decoding state
    size_t error_count = 0;
    uint32_t bytes_decoded = 0;
    uint8_t current_byte = 0;
    int bit_index = 0; // 0-7, MSB first
    bool buffer_overflow = false;

    // Convert edges to pulses and decode
    // WS2812B protocol: HIGH pulse followed by LOW pulse = 1 bit
    // We need pairs of edges to determine pulse durations

    size_t i = 0;
    while (i < edges.size()) {
        // Need at least 2 edges to form a pulse (rising + falling, or falling + rising)
        if (i + 1 >= edges.size()) {
            break; // Not enough edges for a complete pulse
        }

        const EdgeTimestamp &edge0 = edges[i];
        const EdgeTimestamp &edge1 = edges[i + 1];

        // Calculate pulse duration
        uint32_t pulse_duration_ns = edge1.time_ns - edge0.time_ns;

        // Check for reset pulse (long low period indicates frame end)
        if (edge0.level == 0 && isResetPulse(pulse_duration_ns, timing)) {
            FL_DBG("decodeEdgeTimestamps: reset pulse detected at edge " << i
                   << " (duration=" << pulse_duration_ns << "ns)");

            // Flush partial byte if needed
            if (bit_index != 0) {
                FL_WARN("decodeEdgeTimestamps: partial byte at reset (bit_index="
                        << bit_index << "), flushing");
                // Shift remaining bits to MSB position
                current_byte <<= (8 - bit_index);

                // Check buffer space
                if (bytes_decoded < bytes_out.size()) {
                    bytes_out[bytes_decoded] = current_byte;
                    bytes_decoded++;
                } else {
                    buffer_overflow = true;
                }
            }
            break; // End of frame
        }

        // WS2812B protocol: expect HIGH-LOW pulse pairs
        // Need 3 edges to determine one bit: transition to HIGH, transition to LOW, transition to HIGH/LOW
        if (i + 2 >= edges.size()) {
            break; // Not enough edges for complete bit
        }

        const EdgeTimestamp &edge2 = edges[i + 2];

        // Pattern: edge0 (start high), edge1 (start low), edge2 (next transition)
        // High time: edge1.time_ns - edge0.time_ns
        // Low time: edge2.time_ns - edge1.time_ns

        uint32_t high_ns = 0;
        uint32_t low_ns = 0;

        // Determine high/low times based on edge levels
        if (edge0.level == 1 && edge1.level == 0) {
            // Standard pattern: HIGH -> LOW -> (next)
            high_ns = edge1.time_ns - edge0.time_ns;
            low_ns = edge2.time_ns - edge1.time_ns;
        } else if (edge0.level == 0 && edge1.level == 1) {
            // Inverted start: LOW -> HIGH -> LOW
            // This means we're starting in the middle of a LOW pulse
            // Skip to the HIGH edge
            i++;
            continue;
        } else {
            // Unexpected edge pattern
            FL_DBG("decodeEdgeTimestamps: unexpected edge pattern at index " << i
                   << " (level0=" << static_cast<int>(edge0.level)
                   << ", level1=" << static_cast<int>(edge1.level) << ")");
            error_count++;
            i++;
            continue;
        }

        // Decode pulse to bit
        int bit = decodePulseBit(high_ns, low_ns, timing);
        if (bit < 0) {
            error_count++;
            FL_DBG("decodeEdgeTimestamps: invalid pulse at edge "
                   << i << " (high=" << high_ns << "ns, low=" << low_ns << "ns)");
            i += 2; // Skip this bit's edges
            continue;
        }

        // Accumulate bit into byte (MSB first)
        current_byte = (current_byte << 1) | static_cast<uint8_t>(bit);
        bit_index++;

        // Byte complete?
        if (bit_index == 8) {
            // Check buffer space
            if (bytes_decoded < bytes_out.size()) {
                bytes_out[bytes_decoded] = current_byte;
                bytes_decoded++;
            } else {
                // Buffer full, stop decoding
                FL_WARN("decodeEdgeTimestamps: output buffer overflow at byte " << bytes_decoded);
                buffer_overflow = true;
                break;
            }
            current_byte = 0;
            bit_index = 0;
        }

        // Move to next pulse (skip 2 edges: high end, low end)
        i += 2;
    }

    // Flush partial byte if we reached end of edges without reset
    if (bit_index != 0 && !buffer_overflow) {
        FL_WARN("decodeEdgeTimestamps: partial byte at end (bit_index="
                << bit_index << "), flushing");
        // Shift remaining bits to MSB position
        current_byte <<= (8 - bit_index);

        if (bytes_decoded < bytes_out.size()) {
            bytes_out[bytes_decoded] = current_byte;
            bytes_decoded++;
        } else {
            buffer_overflow = true;
        }
    }

    FL_DBG("decodeEdgeTimestamps: decoded " << bytes_decoded << " bytes, " << error_count << " errors");

    // Determine error type and return Result
    if (buffer_overflow) {
        FL_WARN("decodeEdgeTimestamps: buffer overflow - output buffer too small");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
    }

    // Calculate error rate based on expected bits
    size_t total_pulses = edges.size() / 2;  // Approximate pulse count
    if (error_count >= (total_pulses / 10)) {
        FL_WARN("decodeEdgeTimestamps: high error rate: "
                << error_count << "/" << total_pulses << " pulses ("
                << (100 * error_count / total_pulses) << "%)");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
    }

    return fl::Result<uint32_t, DecodeError>::success(bytes_decoded);
}

} // anonymous namespace

// ============================================================================
// GPIO ISR RX Channel Implementation
// ============================================================================

/**
 * @brief Implementation of GPIO ISR RX Channel
 *
 * All ISR-related methods are marked IRAM_ATTR to ensure they run from IRAM.
 * Uses volatile for all variables accessed from both ISR and main code.
 */
class GpioIsrRxImpl : public GpioIsrRx {
public:
    GpioIsrRxImpl(gpio_num_t pin, size_t buffer_size)
        : mPin(pin)
        , mBufferSize(buffer_size)
        , mReceiveDone(false)
        , mEdgesCaptured(0)
        , mStartTimeUs(0)
        , mLastEdgeTimeNs(0)
        , mEdgeBuffer()
        , mIsrInstalled(false)
        , mSignalRangeMinNs(100)
        , mSignalRangeMaxNs(100000)
        , mSkipCounter(0)
    {
        FL_DBG("GpioIsrRx constructed: pin=" << static_cast<int>(mPin)
               << " buffer_size=" << mBufferSize);
    }

    ~GpioIsrRxImpl() override {
        if (mIsrInstalled) {
            FL_DBG("Removing GPIO ISR handler");
            gpio_isr_handler_remove(mPin);
            mIsrInstalled = false;
        }
    }

    bool begin(uint32_t signal_range_min_ns = 100, uint32_t signal_range_max_ns = 100000, uint32_t skip_signals = 0) override {
        // Store signal range parameters for noise filtering and idle detection
        mSignalRangeMinNs = signal_range_min_ns;
        mSignalRangeMaxNs = signal_range_max_ns;
        mSkipCounter = skip_signals;

        FL_DBG("GPIO ISR RX begin: signal_range_min=" << mSignalRangeMinNs
               << "ns, signal_range_max=" << mSignalRangeMaxNs << "ns, skip_signals=" << skip_signals);

        // If already initialized, just re-arm the receiver for a new capture
        if (mIsrInstalled) {
            FL_DBG("GPIO ISR already initialized, re-arming receiver");

            // Clear receive state
            clear();

            // Re-enable GPIO interrupt
            gpio_intr_enable(mPin);

            FL_DBG("GPIO ISR receiver re-armed and ready");
            return true;
        }

        // First-time initialization
        // Clear receive state
        clear();

        // Configure GPIO
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << mPin);
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.intr_type = GPIO_INTR_ANYEDGE;  // Capture both edges

        esp_err_t err = gpio_config(&io_conf);
        if (err != ESP_OK) {
            FL_WARN("Failed to configure GPIO: " << static_cast<int>(err));
            return false;
        }

        FL_DBG("GPIO configured successfully");

        // Install GPIO ISR service if not already installed
        static bool gpio_isr_service_installed = false;
        if (!gpio_isr_service_installed) {
            // Use flags to allocate ISR with priority 3
            // ESP_INTR_FLAG_LEVEL3 sets interrupt priority to 3
            err = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                FL_WARN("Failed to install GPIO ISR service: " << static_cast<int>(err));
                return false;
            }
            gpio_isr_service_installed = true;
            FL_DBG("GPIO ISR service installed with priority 3");
        }

        // Allocate edge buffer
        mEdgeBuffer.clear();
        mEdgeBuffer.reserve(mBufferSize);
        for (size_t i = 0; i < mBufferSize; i++) {
            mEdgeBuffer.push_back({0, 0});
        }

        // Add ISR handler for specific GPIO pin
        err = gpio_isr_handler_add(mPin, gpioIsrHandler, this);
        if (err != ESP_OK) {
            FL_WARN("Failed to add GPIO ISR handler: " << static_cast<int>(err));
            return false;
        }

        mIsrInstalled = true;
        FL_DBG("GPIO ISR handler added successfully");

        // Record start time for relative timestamps
        mStartTimeUs = esp_timer_get_time();

        FL_DBG("GPIO ISR receiver armed and ready");
        return true;
    }

    bool finished() const override {
        return mReceiveDone;
    }

    GpioIsrRxWaitResult wait(uint32_t timeout_ms) override {
        if (!mIsrInstalled) {
            FL_WARN("wait(): GPIO ISR not initialized");
            return GpioIsrRxWaitResult::TIMEOUT;
        }

        FL_DBG("wait(): buffer_size=" << mBufferSize << ", timeout_ms=" << timeout_ms);

        // Convert timeout to microseconds for comparison
        int64_t timeout_us = static_cast<int64_t>(timeout_ms) * 1000;

        // Busy-wait with yield (using ESP-IDF native functions)
        int64_t wait_start_us = esp_timer_get_time();
        while (!finished()) {
            int64_t elapsed_us = esp_timer_get_time() - wait_start_us;

            // Check if buffer filled (success condition)
            if (mEdgesCaptured >= mBufferSize) {
                FL_DBG("wait(): buffer filled (" << mEdgesCaptured << ")");
                return GpioIsrRxWaitResult::SUCCESS;
            }

            // Check wait timeout
            if (elapsed_us >= timeout_us) {
                FL_WARN("wait(): timeout after " << elapsed_us << "us, captured " << mEdgesCaptured << " edges");
                return GpioIsrRxWaitResult::TIMEOUT;
            }

            taskYIELD();  // Allow other tasks to run
        }

        // Receive completed (50µs timeout)
        FL_DBG("wait(): receive done, count=" << mEdgesCaptured);
        return GpioIsrRxWaitResult::SUCCESS;
    }

    fl::span<const EdgeTimestamp> getEdges() const override {
        if (mEdgeBuffer.empty()) {
            return fl::span<const EdgeTimestamp>();
        }
        return fl::span<const EdgeTimestamp>(mEdgeBuffer.data(), mEdgesCaptured);
    }

    fl::Result<uint32_t, DecodeError> decode(const ChipsetTimingRx &timing,
                                               fl::span<uint8_t> out) override {
        // Get captured edges from last receive operation
        fl::span<const EdgeTimestamp> edges = getEdges();

        if (edges.empty()) {
            return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
        }

        // Use the edge timestamp decoder
        return decodeEdgeTimestamps(timing, edges, out);
    }

private:
    /**
     * @brief Clear receive state (internal method)
     */
    void clear() {
        mReceiveDone = false;
        mEdgesCaptured = 0;
        mLastEdgeTimeNs = 0;
        // mSkipCounter is set in begin(), not here
        mStartTimeUs = esp_timer_get_time();
        FL_DBG("GPIO ISR RX state cleared");
    }

    /**
     * @brief Disarm the ISR (stop capturing edges)
     *
     * IRAM_ATTR for ISR context
     *
     * NOTE: This function is no longer used as the ISR has been optimized
     * to call gpio_intr_disable() directly inline for better performance.
     * Kept for potential future use.
     */
    inline void IRAM_ATTR disarmISR() {
        // Disable GPIO interrupt
        gpio_intr_disable(mPin);
    }

    /**
     * @brief GPIO ISR handler (static wrapper) - HEAVILY OPTIMIZED
     *
     * IRAM_ATTR for ISR context
     *
     * Optimizations applied:
     * - Cached volatile variable reads (single read per variable)
     * - Inlined timeout check (eliminated function call)
     * - Unified noise filtering logic (eliminated duplicate code path)
     * - Streamlined skip counter handling
     * - Optimized buffer full detection with pre-increment
     * - Reduced branching in critical path
     */
    static void IRAM_ATTR gpioIsrHandler(void* arg) {
        GpioIsrRxImpl* self = static_cast<GpioIsrRxImpl*>(arg);
        if (!self) {
            return;
        }

        // Cache volatile reads - single read per variable
        size_t edges_captured = self->mEdgesCaptured;
        int64_t start_time_us = self->mStartTimeUs;
        uint32_t last_edge_time_ns = self->mLastEdgeTimeNs;
        uint32_t skip_counter = self->mSkipCounter;

        // Get current timestamp
        int64_t now_us = esp_timer_get_time();

        // First edge: initialize start time
        if (edges_captured == 0) {
            self->mStartTimeUs = now_us;
            start_time_us = now_us;
        }

        // Inline timeout check - avoid function call overhead
        // Idle timeout = signal_range_max_ns converted to microseconds
        int64_t timeout_us = static_cast<int64_t>(self->mSignalRangeMaxNs / 1000);
        if ((now_us - start_time_us) >= timeout_us) {
            gpio_intr_disable(self->mPin);
            self->mReceiveDone = true;
            return;
        }

        // Calculate relative timestamp in nanoseconds
        int64_t relative_time_ns = (now_us - start_time_us) * 1000;
        uint32_t current_time_ns = static_cast<uint32_t>(relative_time_ns);

        // Unified noise filtering (applies to both skip and capture phases)
        if (last_edge_time_ns > 0) {
            uint32_t pulse_width_ns = current_time_ns - last_edge_time_ns;

            // Reject noise/glitches shorter than minimum threshold
            if (pulse_width_ns < self->mSignalRangeMinNs) {
                return;
            }
        }

        // Update last edge time (shared by skip and capture)
        self->mLastEdgeTimeNs = current_time_ns;

        // Handle skip counter
        if (skip_counter > 0) {
            self->mSkipCounter = skip_counter - 1;
            return;
        }

        // Fast path: check buffer full condition early
        if (edges_captured >= self->mBufferSize) {
            gpio_intr_disable(self->mPin);
            self->mReceiveDone = true;
            return;
        }

        // Capture edge: read GPIO level and store
        int level = gpio_get_level(self->mPin);

        // Direct buffer write using cached index
        self->mEdgeBuffer[edges_captured].time_ns = current_time_ns;
        self->mEdgeBuffer[edges_captured].level = static_cast<uint8_t>(level);

        // Pre-increment and check if buffer now full
        size_t next_index = edges_captured + 1;
        self->mEdgesCaptured = next_index;

        if (next_index >= self->mBufferSize) {
            gpio_intr_disable(self->mPin);
            self->mReceiveDone = true;
        }
    }

    gpio_num_t mPin;                              ///< GPIO pin for RX
    size_t mBufferSize;                          ///< Buffer size for edge timestamps
    volatile bool mReceiveDone;                  ///< Set by ISR when receive complete
    volatile size_t mEdgesCaptured;              ///< Number of edges captured (set by ISR)
    volatile int64_t mStartTimeUs;              ///< Start time in microseconds (for relative timestamps)
    volatile uint32_t mLastEdgeTimeNs;         ///< Last edge timestamp for pulse width calculation
    fl::HeapVector<EdgeTimestamp> mEdgeBuffer;   ///< Pre-allocated edge buffer
    bool mIsrInstalled;                          ///< True if ISR handler is installed
    uint32_t mSignalRangeMinNs;                ///< Minimum pulse width (noise filtering)
    uint32_t mSignalRangeMaxNs;                ///< Maximum pulse width (idle detection)
    uint32_t mSkipCounter;                       ///< Runtime counter for skipping (decremented in ISR)
};

// Factory method implementation
fl::shared_ptr<GpioIsrRx> GpioIsrRx::create(int pin, size_t buffer_size) {
    return fl::make_shared<GpioIsrRxImpl>(static_cast<gpio_num_t>(pin), buffer_size);
}

} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32

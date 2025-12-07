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
        : pin_(pin)
        , buffer_size_(buffer_size)
        , receive_done_(false)
        , edges_captured_(0)
        , start_time_us_(0)
        , last_edge_time_ns_(0)
        , edge_buffer_()
        , isr_installed_(false)
        , signal_range_min_ns_(100)
        , signal_range_max_ns_(100000)
    {
        FL_DBG("GpioIsrRx constructed: pin=" << static_cast<int>(pin_)
               << " buffer_size=" << buffer_size_);
    }

    ~GpioIsrRxImpl() override {
        if (isr_installed_) {
            FL_DBG("Removing GPIO ISR handler");
            gpio_isr_handler_remove(pin_);
            isr_installed_ = false;
        }
    }

    bool begin(uint32_t signal_range_min_ns = 100, uint32_t signal_range_max_ns = 100000) override {
        // Store signal range parameters for noise filtering and idle detection
        signal_range_min_ns_ = signal_range_min_ns;
        signal_range_max_ns_ = signal_range_max_ns;

        FL_DBG("GPIO ISR RX begin: signal_range_min=" << signal_range_min_ns_
               << "ns, signal_range_max=" << signal_range_max_ns_ << "ns");

        // If already initialized, just re-arm the receiver for a new capture
        if (isr_installed_) {
            FL_DBG("GPIO ISR already initialized, re-arming receiver");

            // Clear receive state
            clear();

            // Re-enable GPIO interrupt
            gpio_intr_enable(pin_);

            FL_DBG("GPIO ISR receiver re-armed and ready");
            return true;
        }

        // First-time initialization
        // Clear receive state
        clear();

        // Configure GPIO
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = (1ULL << pin_);
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
        edge_buffer_.clear();
        edge_buffer_.reserve(buffer_size_);
        for (size_t i = 0; i < buffer_size_; i++) {
            edge_buffer_.push_back({0, 0});
        }

        // Add ISR handler for specific GPIO pin
        err = gpio_isr_handler_add(pin_, gpioIsrHandler, this);
        if (err != ESP_OK) {
            FL_WARN("Failed to add GPIO ISR handler: " << static_cast<int>(err));
            return false;
        }

        isr_installed_ = true;
        FL_DBG("GPIO ISR handler added successfully");

        // Record start time for relative timestamps
        start_time_us_ = esp_timer_get_time();

        FL_DBG("GPIO ISR receiver armed and ready");
        return true;
    }

    bool finished() const override {
        return receive_done_;
    }

    GpioIsrRxWaitResult wait(uint32_t timeout_ms) override {
        if (!isr_installed_) {
            FL_WARN("wait(): GPIO ISR not initialized");
            return GpioIsrRxWaitResult::TIMEOUT;
        }

        FL_DBG("wait(): buffer_size=" << buffer_size_ << ", timeout_ms=" << timeout_ms);

        // Convert timeout to microseconds for comparison
        int64_t timeout_us = static_cast<int64_t>(timeout_ms) * 1000;

        // Busy-wait with yield (using ESP-IDF native functions)
        int64_t wait_start_us = esp_timer_get_time();
        while (!finished()) {
            int64_t elapsed_us = esp_timer_get_time() - wait_start_us;

            // Check if buffer filled (success condition)
            if (edges_captured_ >= buffer_size_) {
                FL_DBG("wait(): buffer filled (" << edges_captured_ << ")");
                return GpioIsrRxWaitResult::SUCCESS;
            }

            // Check wait timeout
            if (elapsed_us >= timeout_us) {
                FL_WARN("wait(): timeout after " << elapsed_us << "us, captured " << edges_captured_ << " edges");
                return GpioIsrRxWaitResult::TIMEOUT;
            }

            taskYIELD();  // Allow other tasks to run
        }

        // Receive completed (50µs timeout)
        FL_DBG("wait(): receive done, count=" << edges_captured_);
        return GpioIsrRxWaitResult::SUCCESS;
    }

    fl::span<const EdgeTimestamp> getEdges() const override {
        if (edge_buffer_.empty()) {
            return fl::span<const EdgeTimestamp>();
        }
        return fl::span<const EdgeTimestamp>(edge_buffer_.data(), edges_captured_);
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
        receive_done_ = false;
        edges_captured_ = 0;
        last_edge_time_ns_ = 0;
        start_time_us_ = esp_timer_get_time();
        FL_DBG("GPIO ISR RX state cleared");
    }

    /**
     * @brief Check if idle timeout has elapsed since first edge
     * @param current_time_us Current time in microseconds
     * @return true if timeout elapsed, false otherwise
     *
     * Uses signal_range_max_ns_ to determine idle detection threshold.
     * Idle detection triggers when no edges received for signal_range_max_ns_.
     *
     * IRAM_ATTR for ISR context
     */
    inline bool IRAM_ATTR hasTimeoutElapsed(int64_t current_time_us) const {
        // Idle timeout = signal_range_max_ns converted to microseconds
        int64_t timeout_us = static_cast<int64_t>(signal_range_max_ns_ / 1000);
        return (current_time_us - start_time_us_) >= timeout_us;
    }

    /**
     * @brief Disarm the ISR (stop capturing edges)
     *
     * IRAM_ATTR for ISR context
     */
    inline void IRAM_ATTR disarmISR() {
        // Disable GPIO interrupt
        gpio_intr_disable(pin_);
    }

    /**
     * @brief GPIO ISR handler (static wrapper)
     *
     * IRAM_ATTR for ISR context
     */
    static void IRAM_ATTR gpioIsrHandler(void* arg) {
        GpioIsrRxImpl* self = static_cast<GpioIsrRxImpl*>(arg);
        if (!self) {
            return;
        }

        // Get current timestamp
        int64_t now_us = esp_timer_get_time();

        // Check if this is the first edge (start_time_us_ would be 0 or very old)
        // If edges_captured_ is 0, this is the first edge, so update start time
        if (self->edges_captured_ == 0) {
            self->start_time_us_ = now_us;
        }

        // Check timeout (50µs elapsed since first edge)
        if (self->hasTimeoutElapsed(now_us)) {
            self->disarmISR();
            self->receive_done_ = true;
            return;
        }

        // Capture edge if buffer has space
        if (self->edges_captured_ < self->buffer_size_) {
            // Calculate relative timestamp in nanoseconds
            int64_t relative_time_ns = (now_us - self->start_time_us_) * 1000;
            uint32_t current_time_ns = static_cast<uint32_t>(relative_time_ns);

            // Noise filtering: check if pulse width meets minimum threshold
            // (Skip this check for the first edge since we have no previous edge to compare)
            if (self->edges_captured_ > 0) {
                uint32_t pulse_width_ns = current_time_ns - self->last_edge_time_ns_;

                // If pulse is shorter than minimum threshold, discard it (noise)
                if (pulse_width_ns < self->signal_range_min_ns_) {
                    // Ignore this edge - it's too short (likely noise/glitch)
                    return;
                }
            }

            // Read GPIO level
            int level = gpio_get_level(self->pin_);

            // Store edge timestamp
            size_t current_index = self->edges_captured_;
            self->edge_buffer_[current_index].time_ns = current_time_ns;
            self->edge_buffer_[current_index].level = static_cast<uint8_t>(level);
            self->edges_captured_ = current_index + 1;

            // Update last edge time for next pulse width calculation
            self->last_edge_time_ns_ = current_time_ns;

            // Check if buffer is now full
            if (self->edges_captured_ >= self->buffer_size_) {
                self->disarmISR();
                self->receive_done_ = true;
            }
        } else {
            // Buffer overflow (shouldn't happen, but handle it)
            self->disarmISR();
            self->receive_done_ = true;
        }
    }

    gpio_num_t pin_;                              ///< GPIO pin for RX
    size_t buffer_size_;                          ///< Buffer size for edge timestamps
    volatile bool receive_done_;                  ///< Set by ISR when receive complete
    volatile size_t edges_captured_;              ///< Number of edges captured (set by ISR)
    volatile int64_t start_time_us_;              ///< Start time in microseconds (for relative timestamps)
    volatile uint32_t last_edge_time_ns_;         ///< Last edge timestamp for pulse width calculation
    fl::HeapVector<EdgeTimestamp> edge_buffer_;   ///< Pre-allocated edge buffer
    bool isr_installed_;                          ///< True if ISR handler is installed
    uint32_t signal_range_min_ns_;                ///< Minimum pulse width (noise filtering)
    uint32_t signal_range_max_ns_;                ///< Maximum pulse width (idle detection)
};

// Factory method implementation
fl::shared_ptr<GpioIsrRx> GpioIsrRx::create(int pin, size_t buffer_size) {
    return fl::make_shared<GpioIsrRxImpl>(static_cast<gpio_num_t>(pin), buffer_size);
}

} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32

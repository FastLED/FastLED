#include "rmt_rx_channel.h"

#ifdef ESP32

// Include feature flags to detect FASTLED_RMT5
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/dbg.h"
#include "fl/warn.h"
#include "ftl/iterator.h"

FL_EXTERN_C_BEGIN
#include "driver/rmt_rx.h"
#include "driver/rmt_common.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_timer.h"  // For esp_timer_get_time()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"  // For taskYIELD()
FL_EXTERN_C_END

#include <type_traits>

namespace fl {

// Ensure RmtSymbol (uint32_t) and rmt_symbol_word_t have the same size
static_assert(sizeof(RmtSymbol) == sizeof(rmt_symbol_word_t),
              "RmtSymbol must be the same size as rmt_symbol_word_t (32 bits)");
static_assert(std::is_trivially_copyable<rmt_symbol_word_t>::value,
              "rmt_symbol_word_t must be trivially copyable for safe casting");

// ============================================================================
// RMT Symbol Decoder Implementation (private to this file)
// ============================================================================

namespace {

/**
 * @brief Convert RMT ticks to nanoseconds
 * @param ticks Tick count from RMT symbol
 * @param ns_per_tick Cached nanoseconds per tick
 * @return Duration in nanoseconds
 */
inline uint32_t ticksToNs(uint32_t ticks, uint32_t ns_per_tick) {
    // Convert RMT ticks to nanoseconds
    // Example: 16 ticks × 25 ns/tick = 400ns
    return ticks * ns_per_tick;
}

/**
 * @brief Check if symbol is a reset pulse
 * @param symbol RMT symbol to check
 * @param timing Timing thresholds
 * @param ns_per_tick Cached nanoseconds per tick
 * @return true if symbol is reset pulse (long low duration)
 */
inline bool isResetPulse(RmtSymbol symbol, const ChipsetTimingRx &timing, uint32_t ns_per_tick) {
    // Cast RmtSymbol to rmt_symbol_word_t to access bitfields
    const auto &rmt_sym = reinterpret_cast<const rmt_symbol_word_t &>(symbol);

    // Reset pulse characteristics:
    // - Long low duration (>50us)
    // - Either duration0 or duration1 can be the low period

    // Convert durations to nanoseconds
    uint32_t duration0_ns = ticksToNs(rmt_sym.duration0, ns_per_tick);
    uint32_t duration1_ns = ticksToNs(rmt_sym.duration1, ns_per_tick);

    // Check if either duration exceeds reset threshold
    uint32_t reset_min_ns = timing.reset_min_us * 1000;

    // Reset pulse should have level=0 (low) for the long duration
    // Check duration0 with level0=0, or duration1 with level1=0
    if (rmt_sym.level0 == 0 && duration0_ns >= reset_min_ns) {
        return true;
    }
    if (rmt_sym.level1 == 0 && duration1_ns >= reset_min_ns) {
        return true;
    }

    return false;
}

/**
 * @brief Decode single symbol to bit value
 * @param symbol RMT symbol to decode
 * @param timing Timing thresholds
 * @param ns_per_tick Cached nanoseconds per tick
 * @return 0 for bit 0, 1 for bit 1, -1 for invalid symbol
 *
 * Checks high time and low time against timing thresholds.
 * Returns -1 if timing doesn't match either bit pattern.
 */
inline int decodeBit(RmtSymbol symbol, const ChipsetTimingRx &timing, uint32_t ns_per_tick) {
    // Cast RmtSymbol to rmt_symbol_word_t to access bitfields
    const auto &rmt_sym = reinterpret_cast<const rmt_symbol_word_t &>(symbol);

    // Convert tick durations to nanoseconds
    uint32_t high_ns = ticksToNs(rmt_sym.duration0, ns_per_tick);
    uint32_t low_ns = ticksToNs(rmt_sym.duration1, ns_per_tick);

    // WS2812B protocol: first duration is high, second is low
    // Check if levels match expected pattern (high=1, low=0)
    if (rmt_sym.level0 != 1 || rmt_sym.level1 != 0) {
        // Unexpected level pattern - possibly inverted signal or noise
        return -1;
    }

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
 * @brief Decode RMT symbols to bytes (span-based implementation)
 * Internal implementation - not exposed in public header
 */
fl::Result<uint32_t, DecodeError> decodeRmtSymbols(const ChipsetTimingRx &timing,
                                                     uint32_t resolution_hz,
                                                     fl::span<const RmtSymbol> symbols,
                                                     fl::span<uint8_t> bytes_out) {
    if (symbols.empty()) {
        FL_WARN("decodeRmtSymbols: symbols span is empty");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    if (bytes_out.empty()) {
        FL_WARN("decodeRmtSymbols: bytes_out span is empty");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    // Calculate nanoseconds per tick for efficient conversion
    // Example: 40MHz = 40,000,000 Hz → 1,000,000,000 / 40,000,000 = 25ns per tick
    uint32_t ns_per_tick = 1000000000UL / resolution_hz;

    FL_DBG("decodeRmtSymbols: resolution=" << resolution_hz << "Hz, ns_per_tick=" << ns_per_tick);

    // Decoding state
    size_t error_count = 0;
    uint32_t bytes_decoded = 0;
    uint8_t current_byte = 0;
    int bit_index = 0; // 0-7, MSB first
    bool buffer_overflow = false;

    FL_DBG("decodeRmtSymbols: decoding " << symbols.size() << " symbols into buffer of " << bytes_out.size() << " bytes");

    // Cast RmtSymbol array to rmt_symbol_word_t for access to bitfields
    const auto *rmt_symbols = reinterpret_cast<const rmt_symbol_word_t *>(symbols.data());

    for (size_t i = 0; i < symbols.size(); i++) {
        // Check for reset pulse (frame boundary)
        if (isResetPulse(symbols[i], timing, ns_per_tick)) {
            FL_DBG("decodeRmtSymbols: reset pulse detected at symbol " << i);

            // Flush partial byte if needed
            if (bit_index != 0) {
                FL_WARN("decodeRmtSymbols: partial byte at reset (bit_index="
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

        // Decode symbol to bit
        int bit = decodeBit(symbols[i], timing, ns_per_tick);
        if (bit < 0) {
            error_count++;
            FL_DBG("decodeRmtSymbols: invalid symbol at index "
                   << i << " (duration0=" << rmt_symbols[i].duration0
                   << ", duration1=" << rmt_symbols[i].duration1 << ")");

            // Special handling for last symbol with duration1=0 (idle threshold truncation)
            // This occurs when the RMT RX idle threshold is exceeded, truncating the low period
            // We can still decode the bit using only the high period (duration0)
            if (i == symbols.size() - 1 && rmt_symbols[i].duration1 == 0 && rmt_symbols[i].level0 == 1) {
                uint32_t high_ns = ticksToNs(rmt_symbols[i].duration0, ns_per_tick);

                // Check if high period matches bit 0 pattern
                if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns) {
                    bit = 0;
                    FL_DBG("decodeRmtSymbols: last symbol with duration1=0, decoded as bit 0 from high period (" << high_ns << "ns)");
                }
                // Check if high period matches bit 1 pattern
                else if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns) {
                    bit = 1;
                    FL_DBG("decodeRmtSymbols: last symbol with duration1=0, decoded as bit 1 from high period (" << high_ns << "ns)");
                }

                // If we successfully decoded the bit, don't skip it
                if (bit >= 0) {
                    error_count--; // Undo the error count increment
                    // Fall through to bit accumulation below
                } else {
                    continue; // Still invalid, skip it
                }
            } else {
                continue; // Skip invalid symbol
            }
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
                FL_WARN("decodeRmtSymbols: output buffer overflow at byte " << bytes_decoded);
                buffer_overflow = true;
                break;
            }
            current_byte = 0;
            bit_index = 0;
        }
    }

    // Flush partial byte if we reached end of symbols without reset
    if (bit_index != 0 && !buffer_overflow) {
        FL_WARN("decodeRmtSymbols: partial byte at end (bit_index="
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

    FL_DBG("decodeRmtSymbols: decoded " << bytes_decoded << " bytes, " << error_count << " errors");

    // Determine error type and return Result
    if (buffer_overflow) {
        FL_WARN("decodeRmtSymbols: buffer overflow - output buffer too small");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
    }

    if (error_count >= (symbols.size() / 10)) {
        FL_WARN("decodeRmtSymbols: high error rate: "
                << error_count << "/" << symbols.size() << " symbols ("
                << (100 * error_count / symbols.size()) << "%)");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
    }

    return fl::Result<uint32_t, DecodeError>::success(bytes_decoded);
}

/**
 * @brief Decode RMT symbols to bytes using output iterator (template wrapper)
 * Internal implementation - not exposed in public header
 */
template<typename OutputIteratorUint8>
fl::Result<uint32_t, DecodeError> decodeRmtSymbols(const ChipsetTimingRx &timing,
                                                     uint32_t resolution_hz,
                                                     fl::span<const RmtSymbol> symbols,
                                                     OutputIteratorUint8 out) {
    // Chunk size for decoding
    constexpr size_t MAX_CHUNK_SIZE = 256;

    fl::array<uint8_t, MAX_CHUNK_SIZE> chunk_buffer;
    fl::span<uint8_t> chunk_span(chunk_buffer.data(), MAX_CHUNK_SIZE);

    // Call span-based decode implementation
    auto result = decodeRmtSymbols(timing, resolution_hz, symbols, chunk_span);
    if (!result.ok()) {
        return fl::Result<uint32_t, DecodeError>::failure(result.error());
    }

    uint32_t bytes_decoded = result.value();

    // Copy decoded bytes to output iterator
    for (uint32_t i = 0; i < bytes_decoded; i++) {
        *out++ = chunk_buffer[i];
    }

    return fl::Result<uint32_t, DecodeError>::success(bytes_decoded);
}

} // anonymous namespace

// ============================================================================
// RMT RX Channel Implementation
// ============================================================================

/**
 * @brief Implementation of RMT RX Channel
 *
 * All methods are defined inline to avoid IRAM attribute warnings.
 * IRAM_ATTR must be specified only once (on definition), not separately on declaration.
 */
class RmtRxChannelImpl : public RmtRxChannel {
public:
    RmtRxChannelImpl(gpio_num_t pin, uint32_t resolution_hz, int32_t max_buffer_size)
        : channel_(nullptr)
        , pin_(pin)
        , resolution_hz_(resolution_hz)
        , buffer_size_(max_buffer_size > 0 ? static_cast<size_t>(max_buffer_size) : 4096)
        , receive_done_(false)
        , symbols_received_(0)
        , signal_range_min_ns_(100)
        , signal_range_max_ns_(100000)
        , internal_buffer_()
    {
        FL_DBG("RmtRxChannel constructed: pin=" << static_cast<int>(pin_)
               << " resolution=" << resolution_hz_ << "Hz"
               << " max_buffer_size=" << buffer_size_);
    }

    ~RmtRxChannelImpl() override {
        if (channel_) {
            FL_DBG("Deleting RMT RX channel");
            rmt_del_channel(channel_);
            channel_ = nullptr;
        }
    }

    bool begin(uint32_t signal_range_min_ns = 100, uint32_t signal_range_max_ns = 100000) override {
        // Store signal range parameters
        signal_range_min_ns_ = signal_range_min_ns;
        signal_range_max_ns_ = signal_range_max_ns;

        FL_DBG("RX begin: signal_range_min=" << signal_range_min_ns_
               << "ns, signal_range_max=" << signal_range_max_ns_ << "ns");

        // If already initialized, just re-arm the receiver for a new capture
        if (channel_) {
            FL_DBG("RX channel already initialized, re-arming receiver");

            // Clear receive state
            clear();

            // Allocate buffer and arm receiver
            if (!allocateAndArm()) {
                FL_WARN("Failed to re-arm receiver in begin()");
                return false;
            }

            FL_DBG("RX receiver re-armed and ready");
            return true;
        }

        // First-time initialization
        // Clear receive state
        clear();

        // Configure RX channel
        rmt_rx_channel_config_t rx_config = {};
        rx_config.gpio_num = pin_;
        rx_config.clk_src = RMT_CLK_SRC_DEFAULT;
        rx_config.resolution_hz = resolution_hz_;
        rx_config.mem_block_symbols = 64;  // Use 64 symbols per memory block

        // Additional flags
        rx_config.flags.invert_in = false;  // No signal inversion
        rx_config.flags.with_dma = false;   // Start with non-DMA (universal compatibility)
        // io_loop_back is deprecated and removed in ESP-IDF 6 - use physical jumper wire instead

        // Create RX channel
        esp_err_t err = rmt_new_rx_channel(&rx_config, &channel_);
        if (err != ESP_OK) {
            FL_WARN("Failed to create RX channel: " << static_cast<int>(err));
            return false;
        }

        FL_DBG("RX channel created successfully");

        // Register ISR callback
        rmt_rx_event_callbacks_t callbacks = {};
        callbacks.on_recv_done = rxDoneCallback;

        err = rmt_rx_register_event_callbacks(channel_, &callbacks, this);
        if (err != ESP_OK) {
            FL_WARN("Failed to register RX callbacks: " << static_cast<int>(err));
            rmt_del_channel(channel_);
            channel_ = nullptr;
            return false;
        }

        FL_DBG("RX callbacks registered successfully");

        // Enable RX channel
        err = rmt_enable(channel_);
        if (err != ESP_OK) {
            FL_WARN("Failed to enable RX channel: " << static_cast<int>(err));
            rmt_del_channel(channel_);
            channel_ = nullptr;
            return false;
        }

        FL_DBG("RX channel enabled");

        // Allocate buffer and arm receiver
        if (!allocateAndArm()) {
            FL_WARN("Failed to arm receiver in begin()");
            rmt_del_channel(channel_);
            channel_ = nullptr;
            return false;
        }

        FL_DBG("RX receiver armed and ready");
        return true;
    }

    bool finished() const override {
        return receive_done_;
    }

    RmtRxWaitResult wait(uint32_t timeout_ms) override {
        if (!channel_) {
            FL_WARN("wait(): channel not initialized");
            return RmtRxWaitResult::TIMEOUT;  // Treat as timeout
        }

        FL_DBG("wait(): buffer_size=" << buffer_size_ << ", timeout_ms=" << timeout_ms);

        // Only allocate and arm if not already receiving (begin() wasn't called)
        // Check if internal_buffer_ is empty to determine if we need to arm
        if (internal_buffer_.empty()) {
            // Allocate buffer and arm receiver
            if (!allocateAndArm()) {
                FL_WARN("wait(): failed to allocate and arm");
                return RmtRxWaitResult::TIMEOUT;  // Treat as timeout
            }
        } else {
            FL_DBG("wait(): receiver already armed, using existing buffer");
        }

        // Convert timeout to microseconds for comparison
        int64_t timeout_us = static_cast<int64_t>(timeout_ms) * 1000;

        // Busy-wait with yield (using ESP-IDF native functions)
        int64_t start_time_us = esp_timer_get_time();
        while (!finished()) {
            int64_t elapsed_us = esp_timer_get_time() - start_time_us;

            // Check if buffer filled (success condition)
            if (symbols_received_ >= buffer_size_) {
                FL_DBG("wait(): buffer filled (" << symbols_received_ << ")");
                return RmtRxWaitResult::SUCCESS;
            }

            // Check timeout
            if (elapsed_us >= timeout_us) {
                FL_WARN("wait(): timeout after " << elapsed_us << "us, received " << symbols_received_ << " symbols");
                return RmtRxWaitResult::TIMEOUT;
            }

            taskYIELD();  // Allow other tasks to run
        }

        // Receive completed naturally
        FL_DBG("wait(): receive done, count=" << symbols_received_);
        return RmtRxWaitResult::SUCCESS;
    }

    uint32_t getResolutionHz() const override {
        return resolution_hz_;
    }

    fl::Result<uint32_t, DecodeError> decode(const ChipsetTimingRx &timing,
                                               fl::span<uint8_t> out) override {
        // Get received symbols from last receive operation
        fl::span<const RmtSymbol> symbols = getReceivedSymbols();

        if (symbols.empty()) {
            return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
        }

        // Use the span-based decoder directly
        return decodeRmtSymbols(timing, resolution_hz_, symbols, out);
    }

private:
    /**
     * @brief Allocate buffer and arm receiver (internal helper)
     * @return true on success, false on failure
     *
     * Common logic for allocating internal buffer, enabling channel,
     * and starting receive operation.
     */
    bool allocateAndArm() {
        // Allocate internal buffer
        internal_buffer_.clear();
        internal_buffer_.reserve(buffer_size_);
        for (size_t i = 0; i < buffer_size_; i++) {
            internal_buffer_.push_back(0);
        }

        // Enable channel (may be disabled after previous receive)
        if (!enable()) {
            FL_WARN("allocateAndArm(): failed to enable channel");
            return false;
        }

        // Start receive operation
        if (!startReceive(internal_buffer_.data(), buffer_size_)) {
            FL_WARN("allocateAndArm(): failed to start receive");
            return false;
        }

        return true;
    }

    /**
     * @brief Re-enable RMT RX channel (internal method)
     * @return true on success, false on failure
     *
     * After a receive operation completes (or times out), the channel
     * is automatically disabled by the ESP-IDF RMT driver. This method
     * re-enables it for the next receive operation.
     */
    bool enable() {
        if (!channel_) {
            FL_WARN("enable(): RX channel not initialized");
            return false;
        }

        // Disable first to avoid ESP_ERR_INVALID_STATE (259) if already enabled
        // This is safe to call even if already disabled
        esp_err_t err = rmt_disable(channel_);
        if (err != ESP_OK) {
            FL_DBG("rmt_disable returned: " << static_cast<int>(err) << " (ignoring)");
            // Continue anyway - may already be disabled
        }

        // Now enable the channel
        err = rmt_enable(channel_);
        if (err != ESP_OK) {
            FL_WARN("Failed to enable RX channel: " << static_cast<int>(err));
            return false;
        }

        FL_DBG("RX channel enabled");
        return true;
    }

    /**
     * @brief Get received symbols as a span (internal method)
     * @return Span of const RmtSymbol containing received data
     */
    fl::span<const RmtSymbol> getReceivedSymbols() const {
        if (internal_buffer_.empty()) {
            return fl::span<const RmtSymbol>();
        }
        return fl::span<const RmtSymbol>(internal_buffer_.data(), symbols_received_);
    }

    /**
     * @brief Clear receive state (internal method)
     */
    void clear() {
        receive_done_ = false;
        symbols_received_ = 0;
        FL_DBG("RX state cleared");
    }

    /**
     * @brief Internal method to start receiving RMT symbols
     * @param buffer Buffer to store received symbols (must remain valid until done)
     * @param buffer_size Number of symbols buffer can hold
     * @return true if receive started, false on error
     */
    bool startReceive(RmtSymbol* buffer, size_t buffer_size) {
        if (!channel_) {
            FL_WARN("RX channel not initialized (call begin() first)");
            return false;
        }

        if (!buffer || buffer_size == 0) {
            FL_WARN("Invalid buffer parameters");
            return false;
        }

        // Reset state
        receive_done_ = false;
        symbols_received_ = 0;

        // Configure receive parameters (use values from begin())
        rmt_receive_config_t rx_params = {};
        rx_params.signal_range_min_ns = signal_range_min_ns_;
        rx_params.signal_range_max_ns = signal_range_max_ns_;

        // Cast RmtSymbol* to rmt_symbol_word_t* (safe due to static_assert above)
        auto* rmt_buffer = reinterpret_cast<rmt_symbol_word_t*>(buffer);

        // Start receiving
        esp_err_t err = rmt_receive(channel_, rmt_buffer, buffer_size * sizeof(rmt_symbol_word_t), &rx_params);
        if (err != ESP_OK) {
            FL_WARN("Failed to start RX receive: " << static_cast<int>(err));
            return false;
        }

        FL_DBG("RX receive started (buffer size: " << buffer_size << " symbols)");
        return true;
    }

    /**
     * @brief ISR callback for receive completion
     *
     * IRAM_ATTR specified on definition only (not declaration) to avoid warnings.
     */
    static bool IRAM_ATTR rxDoneCallback(rmt_channel_handle_t channel,
                                         const rmt_rx_done_event_data_t* data,
                                         void* user_data) {
        RmtRxChannelImpl* self = static_cast<RmtRxChannelImpl*>(user_data);
        if (!self) {
            return false;
        }

        // Update receive state
        self->symbols_received_ = data->num_symbols;
        self->receive_done_ = true;

        // No higher-priority task awakened
        return false;
    }

    rmt_channel_handle_t channel_;                ///< RMT channel handle
    gpio_num_t pin_;                              ///< GPIO pin for RX
    uint32_t resolution_hz_;                      ///< Clock resolution in Hz
    size_t buffer_size_;                          ///< Internal buffer size in symbols
    volatile bool receive_done_;                  ///< Set by ISR when receive complete
    volatile size_t symbols_received_;            ///< Number of symbols received (set by ISR)
    uint32_t signal_range_min_ns_;                ///< Minimum pulse width (noise filtering)
    uint32_t signal_range_max_ns_;                ///< Maximum pulse width (idle detection)
    fl::HeapVector<RmtSymbol> internal_buffer_;   ///< Internal buffer for all receive operations
};

// Factory method implementation
fl::shared_ptr<RmtRxChannel> RmtRxChannel::create(int pin, uint32_t resolution_hz, int32_t max_buffer_size) {
    return fl::make_shared<RmtRxChannelImpl>(static_cast<gpio_num_t>(pin), resolution_hz, max_buffer_size);
}

} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32

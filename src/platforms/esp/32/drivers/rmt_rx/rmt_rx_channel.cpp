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

#include "ftl/type_traits.h"

namespace fl {

// Ensure RmtSymbol (uint32_t) and rmt_symbol_word_t have the same size
static_assert(sizeof(RmtSymbol) == sizeof(rmt_symbol_word_t),
              "RmtSymbol must be the same size as rmt_symbol_word_t (32 bits)");
static_assert(fl::is_trivially_copyable<rmt_symbol_word_t>::value,
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
        : mChannel(nullptr)
        , mPin(pin)
        , mResolutionHz(resolution_hz)
        , mBufferSize(max_buffer_size > 0 ? static_cast<size_t>(max_buffer_size) : 4096)
        , mReceiveDone(false)
        , mSymbolsReceived(0)
        , mSignalRangeMinNs(100)
        , mSignalRangeMaxNs(100000)
        , mSkipCounter(0)
        , mInternalBuffer()
    {
        FL_DBG("RmtRxChannel constructed: pin=" << static_cast<int>(mPin)
               << " resolution=" << mResolutionHz << "Hz"
               << " max_buffer_size=" << mBufferSize);
    }

    ~RmtRxChannelImpl() override {
        if (mChannel) {
            FL_DBG("Deleting RMT RX channel");
            rmt_del_channel(mChannel);
            mChannel = nullptr;
        }
    }

    bool begin(uint32_t signal_range_min_ns = 100, uint32_t signal_range_max_ns = 100000, uint32_t skip_signals = 0) override {
        // Store signal range parameters
        mSignalRangeMinNs = signal_range_min_ns;
        mSignalRangeMaxNs = signal_range_max_ns;
        mSkipCounter = skip_signals;

        FL_DBG("RX begin: signal_range_min=" << mSignalRangeMinNs
               << "ns, signal_range_max=" << mSignalRangeMaxNs << "ns"
               << ", skip_signals=" << skip_signals);

        // If already initialized, just re-arm the receiver for a new capture
        if (mChannel) {
            FL_DBG("RX channel already initialized, re-arming receiver");

            // Clear receive state (resets mSkipCounter to skip_signals_)
            clear();

            // Handle skip phase using small discard buffer
            if (!handleSkipPhase()) {
                FL_WARN("Failed to handle skip phase in begin()");
                return false;
            }

            // Allocate buffer and arm receiver for actual capture
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
        rx_config.gpio_num = mPin;
        rx_config.clk_src = RMT_CLK_SRC_DEFAULT;
        rx_config.resolution_hz = mResolutionHz;
        rx_config.mem_block_symbols = 64;  // Use 64 symbols per memory block
        rx_config.intr_priority = 3;  // Match the priority level used in testing

        // Additional flags
        rx_config.flags.invert_in = false;  // No signal inversion
        rx_config.flags.with_dma = false;   // Start with non-DMA (universal compatibility)
        // io_loop_back is deprecated and removed in ESP-IDF 6 - use physical jumper wire instead

        // Create RX channel
        esp_err_t err = rmt_new_rx_channel(&rx_config, &mChannel);
        if (err != ESP_OK) {
            FL_WARN("Failed to create RX channel: " << static_cast<int>(err));
            return false;
        }

        FL_DBG("RX channel created successfully");

        // Register ISR callback
        rmt_rx_event_callbacks_t callbacks = {};
        callbacks.on_recv_done = rxDoneCallback;

        err = rmt_rx_register_event_callbacks(mChannel, &callbacks, this);
        if (err != ESP_OK) {
            FL_WARN("Failed to register RX callbacks: " << static_cast<int>(err));
            rmt_del_channel(mChannel);
            mChannel = nullptr;
            return false;
        }

        FL_DBG("RX callbacks registered successfully");

        // Enable RX channel
        err = rmt_enable(mChannel);
        if (err != ESP_OK) {
            FL_WARN("Failed to enable RX channel: " << static_cast<int>(err));
            rmt_del_channel(mChannel);
            mChannel = nullptr;
            return false;
        }

        FL_DBG("RX channel enabled");

        // Handle skip phase using small discard buffer
        if (!handleSkipPhase()) {
            FL_WARN("Failed to handle skip phase in begin()");
            rmt_del_channel(mChannel);
            mChannel = nullptr;
            return false;
        }

        // Allocate buffer and arm receiver for actual capture
        if (!allocateAndArm()) {
            FL_WARN("Failed to arm receiver in begin()");
            rmt_del_channel(mChannel);
            mChannel = nullptr;
            return false;
        }

        FL_DBG("RX receiver armed and ready");
        return true;
    }

    bool finished() const override {
        return mReceiveDone;
    }

    RmtRxWaitResult wait(uint32_t timeout_ms) override {
        if (!mChannel) {
            FL_WARN("wait(): channel not initialized");
            return RmtRxWaitResult::TIMEOUT;  // Treat as timeout
        }

        FL_DBG("wait(): buffer_size=" << mBufferSize << ", timeout_ms=" << timeout_ms);

        // Only allocate and arm if not already receiving (begin() wasn't called)
        // Check if mInternalBuffer is empty to determine if we need to arm
        if (mInternalBuffer.empty()) {
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
            if (mSymbolsReceived >= mBufferSize) {
                FL_DBG("wait(): buffer filled (" << mSymbolsReceived << ")");
                return RmtRxWaitResult::SUCCESS;
            }

            // Check timeout
            if (elapsed_us >= timeout_us) {
                FL_WARN("wait(): timeout after " << elapsed_us << "us, received " << mSymbolsReceived << " symbols");
                return RmtRxWaitResult::TIMEOUT;
            }

            taskYIELD();  // Allow other tasks to run
        }

        // Receive completed naturally
        FL_DBG("wait(): receive done, count=" << mSymbolsReceived);
        return RmtRxWaitResult::SUCCESS;
    }

    uint32_t getResolutionHz() const override {
        return mResolutionHz;
    }

    fl::Result<uint32_t, DecodeError> decode(const ChipsetTimingRx &timing,
                                               fl::span<uint8_t> out) override {
        // Get received symbols from last receive operation
        fl::span<const RmtSymbol> symbols = getReceivedSymbols();

        if (symbols.empty()) {
            return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
        }

        // Use the span-based decoder directly
        return decodeRmtSymbols(timing, mResolutionHz, symbols, out);
    }

private:
    /**
     * @brief Handle skip phase by discarding symbols until mSkipCounter reaches 0
     * @return true on success, false on failure
     *
     * Uses a small discard buffer to receive and discard symbols without
     * allocating the full buffer. This ensures we don't buffer unwanted symbols.
     * Only allocates memory temporarily during the skip phase.
     */
    bool handleSkipPhase() {
        if (mSkipCounter == 0) {
            FL_DBG("No symbols to skip, skip phase complete");
            return true;
        }

        FL_DBG("Entering skip phase: skipping " << mSkipCounter << " symbols");

        // Use a small discard buffer (64 symbols = 256 bytes)
        constexpr size_t DISCARD_BUFFER_SIZE = 64;
        fl::HeapVector<RmtSymbol> discard_buffer;
        discard_buffer.reserve(DISCARD_BUFFER_SIZE);
        for (size_t i = 0; i < DISCARD_BUFFER_SIZE; i++) {
            discard_buffer.push_back(0);
        }

        // Skip symbols in chunks until mSkipCounter reaches 0
        while (mSkipCounter > 0) {
            size_t chunk_size = (mSkipCounter < DISCARD_BUFFER_SIZE)
                ? mSkipCounter : DISCARD_BUFFER_SIZE;

            FL_DBG("Skip phase: receiving chunk of " << chunk_size
                   << " symbols (remaining: " << mSkipCounter << ")");

            // Enable channel for this receive operation
            if (!enable()) {
                FL_WARN("handleSkipPhase(): failed to enable channel");
                return false;
            }

            // Start receive with discard buffer
            if (!startReceive(discard_buffer.data(), chunk_size)) {
                FL_WARN("handleSkipPhase(): failed to start receive");
                return false;
            }

            // Wait for receive to complete (with timeout)
            constexpr uint32_t SKIP_TIMEOUT_MS = 5000;  // 5 second timeout per chunk
            int64_t start_time_us = esp_timer_get_time();
            int64_t timeout_us = SKIP_TIMEOUT_MS * 1000;

            while (!mReceiveDone) {
                int64_t elapsed_us = esp_timer_get_time() - start_time_us;
                if (elapsed_us >= timeout_us) {
                    FL_WARN("handleSkipPhase(): timeout waiting for symbols");
                    return false;
                }
                taskYIELD();
            }

            // Check if ISR properly decremented mSkipCounter
            // (ISR callback handles decrementing mSkipCounter)
            FL_DBG("Skip phase chunk complete, mSkipCounter now: " << mSkipCounter);

            // Reset mReceiveDone for next iteration
            mReceiveDone = false;
        }

        FL_DBG("Skip phase complete");
        return true;
    }

    /**
     * @brief Allocate buffer and arm receiver (internal helper)
     * @return true on success, false on failure
     *
     * Common logic for allocating internal buffer, enabling channel,
     * and starting receive operation.
     */
    bool allocateAndArm() {
        // Allocate internal buffer
        mInternalBuffer.clear();
        mInternalBuffer.reserve(mBufferSize);
        for (size_t i = 0; i < mBufferSize; i++) {
            mInternalBuffer.push_back(0);
        }

        // Enable channel (may be disabled after previous receive)
        if (!enable()) {
            FL_WARN("allocateAndArm(): failed to enable channel");
            return false;
        }

        // Start receive operation
        if (!startReceive(mInternalBuffer.data(), mBufferSize)) {
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
        if (!mChannel) {
            FL_WARN("enable(): RX channel not initialized");
            return false;
        }

        // Disable first to avoid ESP_ERR_INVALID_STATE (259) if already enabled
        // This is safe to call even if already disabled
        esp_err_t err = rmt_disable(mChannel);
        if (err != ESP_OK) {
            FL_DBG("rmt_disable returned: " << static_cast<int>(err) << " (ignoring)");
            // Continue anyway - may already be disabled
        }

        // Now enable the channel
        err = rmt_enable(mChannel);
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
        if (mInternalBuffer.empty()) {
            return fl::span<const RmtSymbol>();
        }
        return fl::span<const RmtSymbol>(mInternalBuffer.data(), mSymbolsReceived);
    }

    /**
     * @brief Clear receive state (internal method)
     */
    void clear() {
        mReceiveDone = false;
        mSymbolsReceived = 0;
        // mSkipCounter is set in begin(), not here
        FL_DBG("RX state cleared");
    }

    /**
     * @brief Internal method to start receiving RMT symbols
     * @param buffer Buffer to store received symbols (must remain valid until done)
     * @param buffer_size Number of symbols buffer can hold
     * @return true if receive started, false on error
     */
    bool startReceive(RmtSymbol* buffer, size_t buffer_size) {
        if (!mChannel) {
            FL_WARN("RX channel not initialized (call begin() first)");
            return false;
        }

        if (!buffer || buffer_size == 0) {
            FL_WARN("Invalid buffer parameters");
            return false;
        }

        // Reset state
        mReceiveDone = false;
        mSymbolsReceived = 0;

        // Configure receive parameters (use values from begin())
        rmt_receive_config_t rx_params = {};
        rx_params.signal_range_min_ns = mSignalRangeMinNs;
        rx_params.signal_range_max_ns = mSignalRangeMaxNs;

        // Cast RmtSymbol* to rmt_symbol_word_t* (safe due to static_assert above)
        auto* rmt_buffer = reinterpret_cast<rmt_symbol_word_t*>(buffer);

        // Start receiving
        esp_err_t err = rmt_receive(mChannel, rmt_buffer, buffer_size * sizeof(rmt_symbol_word_t), &rx_params);
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
     *
     * Skip logic: When mSkipCounter > 0, we're in skip phase (called from handleSkipPhase).
     * Decrement mSkipCounter and discard symbols. When mSkipCounter == 0, we're in
     * capture phase - store the received symbols normally.
     */
    static bool IRAM_ATTR rxDoneCallback(rmt_channel_handle_t channel,
                                         const rmt_rx_done_event_data_t* data,
                                         void* user_data) {
        RmtRxChannelImpl* self = static_cast<RmtRxChannelImpl*>(user_data);
        if (!self) {
            return false;
        }

        size_t received_count = data->num_symbols;

        // Check if we're in skip phase
        if (self->mSkipCounter > 0) {
            // Discard received symbols and decrement skip counter
            if (self->mSkipCounter >= received_count) {
                self->mSkipCounter -= received_count;
            } else {
                self->mSkipCounter = 0;
            }

            self->mSymbolsReceived = 0;
            self->mReceiveDone = true;  // Signal completion for skip phase
            return false;
        }

        // Capture phase - store received symbols
        self->mSymbolsReceived = received_count;
        self->mReceiveDone = true;

        // No higher-priority task awakened
        return false;
    }

    rmt_channel_handle_t mChannel;                ///< RMT channel handle
    gpio_num_t mPin;                              ///< GPIO pin for RX
    uint32_t mResolutionHz;                      ///< Clock resolution in Hz
    size_t mBufferSize;                          ///< Internal buffer size in symbols
    volatile bool mReceiveDone;                  ///< Set by ISR when receive complete
    volatile size_t mSymbolsReceived;            ///< Number of symbols received (set by ISR)
    uint32_t mSignalRangeMinNs;                ///< Minimum pulse width (noise filtering)
    uint32_t mSignalRangeMaxNs;                ///< Maximum pulse width (idle detection)
    uint32_t mSkipCounter;                       ///< Runtime counter for skipping (decremented in ISR)
    fl::HeapVector<RmtSymbol> mInternalBuffer;   ///< Internal buffer for all receive operations
};

// Factory method implementation
fl::shared_ptr<RmtRxChannel> RmtRxChannel::create(int pin, uint32_t resolution_hz, int32_t max_buffer_size) {
    return fl::make_shared<RmtRxChannelImpl>(static_cast<gpio_num_t>(pin), resolution_hz, max_buffer_size);
}

} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32

#include "gpio_isr_rx.h"

#ifdef ESP32

// Include feature flags to detect FASTLED_RMT5
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/error.h"
#include "ftl/vector.h"
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"
#include "platforms/esp/32/core/delaycycles.h"  // For get_ccount() - force-inlined cycle counter
#include "platforms/esp/32/core/fastpin_esp32.h"  // For pin validation macros

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "soc/rtc.h"       // For rtc_clk_cpu_freq_get_config() - Arduino ESP32
#include "soc/gpio_reg.h"  // For GPIO_IN_REG, GPIO_IN1_REG (direct register access)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"  // For taskYIELD()
FL_EXTERN_C_END

namespace fl {

// ============================================================================
// Pin Validation Helper
// ============================================================================

namespace {

/**
 * @brief Check if GPIO pin is valid for use
 * @param pin GPIO pin number to check
 * @return true if pin is valid and usable, false otherwise
 */
inline bool isValidGpioPin(int pin) {
    if (pin < 0 || pin >= 64) {
        return false;
    }
    // Check against the valid pin mask (excludes unusable pins like UART, flash, etc.)
    return (_FL_VALID_PIN_MASK & (1ULL << pin)) != 0;
}

} // anonymous namespace

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
inline int decodePulseBit(uint32_t high_ns, uint32_t low_ns, const ChipsetTiming4Phase &timing) {
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
inline bool isResetPulse(uint32_t duration_ns, const ChipsetTiming4Phase &timing) {
    uint32_t reset_min_ns = timing.reset_min_us * 1000;
    return duration_ns >= reset_min_ns;
}

/**
 * @brief Decode edge timestamps to bytes (span-based implementation)
 * Internal implementation - not exposed in public header
 * @param timing Chipset timing thresholds
 * @param edges Span of captured edge timestamps (spurious edges already filtered upstream)
 * @param bytes_out Output span for decoded bytes
 *
 * Note: Edge detection/filtering must happen upstream when edges are stored.
 * This function assumes continuous LOW samples at the beginning have been skipped.
 */
fl::Result<uint32_t, DecodeError> decodeEdgeTimestamps(const ChipsetTiming4Phase &timing,
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

    // Note: Edge detection/filtering already happened upstream when edges were stored
    // Input edges array contains only valid data edges (spurious LOW edges already skipped)

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
        } else {
            // Unexpected edge pattern (should be prevented by edge detection)
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
 * Lightweight ISR context - only data needed in interrupt handler
 * Minimizes member access and cache misses for maximum ISR performance
 * Uses CPU cycle counter for fastest possible timestamp capture
 *
 * Note: Completely non-volatile for maximum compiler optimization.
 * Main thread uses memory barrier after detecting receiveDone=true.
 *
 * Alignment: 64-byte aligned to fit in single cache line on ESP32
 * Member ordering: Hot path variables first (writePtr, endPtr, cycles)
 */
struct alignas(64) IsrContext {
    // Hot path - accessed on every ISR invocation (cache line friendly)
    EdgeTimestamp* writePtr;             ///< Current write position
    EdgeTimestamp* endPtr;                ///< End of buffer (for overflow check)
    uint32_t startCycles;                ///< Start cycle count
    uint32_t lastEdgeCycles;             ///< Last edge cycles (for noise filter)

    // GPIO register access (precomputed for speed)
    uint32_t gpioInRegAddr;              ///< GPIO_IN_REG or GPIO_IN1_REG address (precomputed)
    uint8_t gpioBitMask;                 ///< Bit mask for pin (1 << pin_bit, precomputed)

    // Frequently accessed
    bool receiveDone;                     ///< Done flag (set by ISR, polled by main thread)
    uint8_t currentLevel;                 ///< Current pin level (toggled on each edge)
    gpio_num_t pin;                       ///< GPIO pin number (int type)
    uint8_t _pad1;                        ///< Padding for alignment

    // Config values (read-only in ISR)
    uint32_t timeoutCycles;              ///< Timeout in CPU cycles (precomputed)
    uint32_t minPulseCycles;             ///< Minimum pulse width in cycles (noise filter)
    uint32_t skipCounter;                 ///< Edges to skip before recording
    uint32_t cpuFreqMhz;                 ///< CPU frequency in MHz (for conversion)

    // Counter (written on edge capture)
    size_t edgesCounter;                  ///< Edge count (updated by ISR, read by main thread)
};

/**
 * @brief Implementation of GPIO ISR RX Channel
 *
 * All ISR-related methods are marked IRAM_ATTR to ensure they run from IRAM.
 * Uses volatile for all variables accessed from both ISR and main code.
 */
class GpioIsrRxImpl : public GpioIsrRx {
public:
    GpioIsrRxImpl(gpio_num_t pin, size_t buffer_size)
        : mIsrCtx{}
        , mPin(pin)
        , mBufferSize(buffer_size)
        , mEdgeBuffer()
        , mIsrInstalled(false)
        , mNeedsConversion(false)
        , mSignalRangeMinNs(100)
        , mSignalRangeMaxNs(100000)
        , mStartLow(true)
    {
        // Validate pin before initialization
        if (!isValidGpioPin(static_cast<int>(pin))) {
            FL_ERROR("GPIO ISR RX: Invalid pin " << static_cast<int>(pin)
                     << " - pin is reserved for UART, flash, or other system use. "
                     << "Please choose a different GPIO pin.");
            // Note: Constructor completes but begin() will fail
        }

        // Initialize ISR context
        mIsrCtx.writePtr = nullptr;
        mIsrCtx.endPtr = nullptr;
        mIsrCtx.startCycles = 0;
        mIsrCtx.lastEdgeCycles = 0;
        mIsrCtx.currentLevel = 0;
        mIsrCtx.pin = mPin;

        // Precompute GPIO register access (for maximum ISR speed)
        mIsrCtx.gpioInRegAddr = (mPin < 32) ? GPIO_IN_REG : GPIO_IN1_REG;
        uint8_t pin_bit = (mPin < 32) ? mPin : (mPin - 32);
        mIsrCtx.gpioBitMask = (1U << pin_bit);

        mIsrCtx.timeoutCycles = 24000;  // Default ~100us @ 240MHz, updated in begin()
        mIsrCtx.minPulseCycles = 24;    // Default ~100ns @ 240MHz, updated in begin()
        mIsrCtx.skipCounter = 0;
        mIsrCtx.receiveDone = false;
        mIsrCtx.edgesCounter = 0;

        // Get actual CPU frequency (Arduino ESP32 method)
        rtc_cpu_freq_config_t freq_config;
        rtc_clk_cpu_freq_get_config(&freq_config);
        mIsrCtx.cpuFreqMhz = freq_config.freq_mhz;

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

    bool begin(const RxConfig& config) override {
        // Store configuration parameters
        mSignalRangeMinNs = config.signal_range_min_ns;
        mSignalRangeMaxNs = config.signal_range_max_ns;
        mStartLow = config.start_low;

        // Convert nanoseconds to CPU cycles for ISR
        // cycles = (nanoseconds * CPU_MHz) / 1000
        uint32_t cpuMhz = mIsrCtx.cpuFreqMhz;
        mIsrCtx.timeoutCycles = (config.signal_range_max_ns * cpuMhz) / 1000;
        mIsrCtx.minPulseCycles = (config.signal_range_min_ns * cpuMhz) / 1000;
        if (mIsrCtx.minPulseCycles == 0) mIsrCtx.minPulseCycles = 1;  // Minimum 1 cycle
        mIsrCtx.skipCounter = config.skip_signals;

        FL_DBG("GPIO ISR RX begin: signal_range_min=" << mSignalRangeMinNs
               << "ns, signal_range_max=" << mSignalRangeMaxNs << "ns"
               << ", skip_signals=" << config.skip_signals
               << ", start_low=" << (mStartLow ? "true" : "false"));

        // If already initialized, just re-arm the receiver for a new capture
        if (mIsrInstalled) {
            FL_DBG("GPIO ISR already initialized, re-arming receiver");

            // Check if ISR is still armed from previous capture (error condition)
            if (!mIsrCtx.receiveDone) {
                FL_WARN("ERROR: GPIO ISR is still armed from previous capture - call wait() or check finished() before calling begin() again");
                return false;
            }

            // Clear receive state
            clear();

            // Re-enable GPIO interrupt
            esp_err_t err = gpio_intr_enable(mPin);
            if (err != ESP_OK) {
                FL_WARN("Failed to re-enable GPIO interrupt: " << static_cast<int>(err));
                return false;
            }

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

        // Allocate edge buffer (ISR writes cycles, main thread converts to ns)
        mEdgeBuffer.clear();
        mEdgeBuffer.reserve(mBufferSize);
        for (size_t i = 0; i < mBufferSize; i++) {
            mEdgeBuffer.push_back({0, 0});
        }

        // Initialize ISR context pointers
        mIsrCtx.writePtr = mEdgeBuffer.data();
        mIsrCtx.endPtr = mEdgeBuffer.data() + mBufferSize;

        // Add ISR handler for specific GPIO pin (pass IsrContext directly for faster access)
        // This automatically enables the GPIO interrupt if successful
        err = gpio_isr_handler_add(mPin, gpioIsrHandler, &mIsrCtx);
        if (err != ESP_OK) {
            FL_WARN("Failed to add GPIO ISR handler: " << static_cast<int>(err));
            return false;
        }

        mIsrInstalled = true;

        // Verify ISR is armed by checking that receiveDone is false (initial state)
        if (mIsrCtx.receiveDone) {
            FL_WARN("GPIO ISR handler added but receiver state is invalid (receiveDone=true)");
            return false;
        }

        FL_DBG("GPIO ISR handler added successfully - ISR armed and ready");

        // Mark buffer as needing conversion (ISR will write cycles)
        mNeedsConversion = true;

        return true;
    }

    bool finished() const override {
        bool done = mIsrCtx.receiveDone;
        if (done) {
            // Memory barrier: ensure all ISR writes are visible
            __asm__ __volatile__("" ::: "memory");  // Compiler barrier
            #if CONFIG_IDF_TARGET_ARCH_XTENSA
            asm volatile("memw" ::: "memory");      // Hardware barrier (Xtensa)
            #elif CONFIG_IDF_TARGET_ARCH_RISCV
            asm volatile("fence" ::: "memory");     // Hardware barrier (RISC-V)
            #endif
        }
        return done;
    }

    RxWaitResult wait(uint32_t timeout_ms) override {
        if (!mIsrInstalled) {
            FL_WARN("wait(): GPIO ISR not initialized");
            return RxWaitResult::TIMEOUT;
        }

        FL_DBG("wait(): buffer_size=" << mBufferSize << ", timeout_ms=" << timeout_ms);

        // Convert timeout to microseconds for comparison
        int64_t timeout_us = static_cast<int64_t>(timeout_ms) * 1000;

        // Busy-wait with yield (using ESP-IDF native functions)
        int64_t wait_start_us = esp_timer_get_time();
        while (!finished()) {
            int64_t elapsed_us = esp_timer_get_time() - wait_start_us;

            // Check if buffer filled (success condition)
            if (mIsrCtx.edgesCounter >= mBufferSize) {
                FL_DBG("wait(): buffer filled (" << mIsrCtx.edgesCounter << ")");
                return RxWaitResult::SUCCESS;
            }

            // Check wait timeout
            if (elapsed_us >= timeout_us) {
                FL_WARN("wait(): timeout after " << elapsed_us << "us, captured " << mIsrCtx.edgesCounter << " edges");
                return RxWaitResult::TIMEOUT;
            }

            taskYIELD();  // Allow other tasks to run
        }

        // Receive completed (50µs timeout)
        FL_DBG("wait(): receive done, count=" << mIsrCtx.edgesCounter);
        return RxWaitResult::SUCCESS;
    }

    fl::span<const EdgeTimestamp> getEdges() const override {
        if (mEdgeBuffer.empty()) {
            return fl::span<const EdgeTimestamp>();
        }

        // Memory barrier: ensure all ISR writes are visible to main thread
        // ISR uses non-volatile for speed, so we need explicit synchronization
        __asm__ __volatile__("" ::: "memory");  // Compiler barrier
        #if CONFIG_IDF_TARGET_ARCH_XTENSA
        asm volatile("memw" ::: "memory");      // Hardware barrier (Xtensa)
        #elif CONFIG_IDF_TARGET_ARCH_RISCV
        asm volatile("fence" ::: "memory");     // Hardware barrier (RISC-V)
        #endif

        // Convert cycles to nanoseconds in-place (only once, when needed)
        if (mNeedsConversion) {
            uint32_t cpuMhz = mIsrCtx.cpuFreqMhz;
            // Need to cast away const for mutable buffer access (logically const operation)
            EdgeTimestamp* buffer = const_cast<EdgeTimestamp*>(mEdgeBuffer.data());
            for (size_t i = 0; i < mIsrCtx.edgesCounter; i++) {
                // Convert: ns = (cycles * 1000) / CPU_MHz
                // EdgeTimestamp.cycles and .time_ns are union, so this overwrites cycles with ns
                buffer[i].time_ns = (buffer[i].cycles * 1000) / cpuMhz;
            }
            // Mark as converted (const_cast needed since this is logically const but modifies cache)
            const_cast<GpioIsrRxImpl*>(this)->mNeedsConversion = false;
        }

        return fl::span<const EdgeTimestamp>(mEdgeBuffer.data(), mIsrCtx.edgesCounter);
    }

    fl::Result<uint32_t, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                               fl::span<uint8_t> out) override {
        // Get captured edges from last receive operation
        fl::span<const EdgeTimestamp> edges = getEdges();

        if (edges.empty()) {
            return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
        }

        // Use the edge timestamp decoder (edge detection already done in ISR)
        return decodeEdgeTimestamps(timing, edges, out);
    }

private:
    /**
     * @brief Clear receive state (internal method)
     */
    void clear() {
        mIsrCtx.receiveDone = false;
        mIsrCtx.edgesCounter = 0;

        // Reset ISR context
        mIsrCtx.writePtr = mEdgeBuffer.data();
        mIsrCtx.lastEdgeCycles = 0;
        mIsrCtx.startCycles = 0;  // Will be set on first edge
        mIsrCtx.currentLevel = mStartLow ? 0 : 1;

        // Mark buffer as needing conversion (will contain cycles after ISR)
        mNeedsConversion = true;

        FL_DBG("GPIO ISR RX state cleared");
    }

    /**
     * @brief Ultra-fast GPIO ISR handler
     *
     * Optimizations:
     * - IsrContext passed directly as void* arg (no member access needed)
     * - Check done flag FIRST (exit if already complete)
     * - Check pin state second (exit if no change)
     * - Uses CPU cycle counter via __clock_cycles() (fastest timestamp method)
     * - Direct GPIO register access with precomputed address/mask
     * - Lightweight ISR context (single structure)
     * - Pointer-based buffer writes (no array indexing)
     * - No gpio_intr_disable calls (expensive, done by main thread)
     * - Minimal branches, optimized for fast path
     * - FL_IRAM placement for zero-wait-state execution
     * - O3 optimization for maximum speed
     */
    FL_IRAM static void __attribute__((optimize("O3"))) gpioIsrHandler(void* arg) {
        // Cast directly to IsrContext (passed as arg for maximum speed)
        IsrContext* ctx = static_cast<IsrContext*>(arg);

        // Check if already done - exit immediately (fastest check)
        if (ctx->receiveDone) {
            // Disable interrupt on first check after done (only happens once)
            gpio_intr_disable(ctx->pin);
            return;
        }

        // Read pin state directly from register (fastest method - uses precomputed address & mask)
        uint32_t gpio_in_reg = REG_READ(ctx->gpioInRegAddr);
        uint8_t new_level = (gpio_in_reg & ctx->gpioBitMask) ? 1 : 0;

        if (new_level == ctx->currentLevel) {
            return;  // Spurious interrupt, no actual edge
        }

        // Get CPU cycle count (fastest timestamp method - uses FastLED's optimized version)
        uint32_t now_cycles = __clock_cycles();

        // Initialize start cycles on first edge
        EdgeTimestamp* ptr = ctx->writePtr;
        bool is_first_edge = (ctx->startCycles == 0);
        if (is_first_edge) {
            ctx->startCycles = now_cycles;
            ctx->lastEdgeCycles = now_cycles;  // Initialize last edge to start
        }

        // Timeout check (cycles since last edge, not since start)
        uint32_t cycles_since_last_edge = now_cycles - ctx->lastEdgeCycles;
        if (cycles_since_last_edge >= ctx->timeoutCycles) {
            ctx->receiveDone = true;
            return;  // Main thread will disable ISR
        }

        // Noise filter (cycles since last edge) - skip on first edge
        if (!is_first_edge && cycles_since_last_edge < ctx->minPulseCycles) {
            return;  // Glitch - pulse too short
        }

        // Calculate elapsed time since start for storage
        uint32_t elapsed_cycles = now_cycles - ctx->startCycles;

        // Update last edge timestamp
        ctx->lastEdgeCycles = now_cycles;

        // Skip counter
        if (ctx->skipCounter > 0) {
            ctx->skipCounter--;
            ctx->currentLevel = new_level;  // Update level even when skipping
            return;
        }

        // Update level
        ctx->currentLevel = new_level;

        // Buffer full check
        if (ptr >= ctx->endPtr) {
            ctx->receiveDone = true;
            return;  // Next cycle will disarm.
        }

        // Write edge (fastest path - store cycles directly)
        ptr->cycles = elapsed_cycles;
        ptr->level = new_level;
        ptr++;

        // Update pointers
        ctx->writePtr = ptr;
        ctx->edgesCounter++;

        // Check if now full
        if (ptr >= ctx->endPtr) {
            ctx->receiveDone = true;
            // Next cycle will disarm.
        }
    }

    size_t getRawEdgeTimes(fl::span<EdgeTime> out) const override {
        // Ensure conversion from cycles to nanoseconds has happened
        fl::span<const EdgeTimestamp> edges = getEdges();

        if (edges.empty()) {
            return 0;
        }

        size_t out_index = 0;

        // Process edges: accumulate time when consecutive edges have same state
        for (size_t i = 0; i < edges.size() && out_index < out.size(); ) {
            // Current edge: level = state AFTER transition
            uint8_t current_state = edges[i].level;
            uint32_t state_start_ns = (i == 0) ? 0 : edges[i-1].time_ns;
            uint32_t accumulated_ns = edges[i].time_ns - state_start_ns;

            // Look ahead: if next edges have the same state (glitch/noise), accumulate their time
            size_t next_i = i + 1;
            while (next_i < edges.size() && edges[next_i].level == current_state) {
                accumulated_ns = edges[next_i].time_ns - state_start_ns;
                next_i++;
            }

            // Filter out transitions shorter than minimum threshold
            if (accumulated_ns >= mSignalRangeMinNs) {
                // Emit accumulated duration for this state
                out[out_index] = EdgeTime(current_state == 1, accumulated_ns);
                out_index++;
            }

            i = next_i;
        }

        return out_index;
    }

    const char* name() const override {
        return "ISR";
    }

    // ISR-optimized context - single structure for fast access
    IsrContext mIsrCtx;

    // Non-ISR members
    gpio_num_t mPin;                              ///< GPIO pin for RX
    size_t mBufferSize;                          ///< Buffer size for edge timestamps
    fl::HeapVector<EdgeTimestamp> mEdgeBuffer;   ///< Buffer (ISR writes cycles, getEdges() converts to ns)
    bool mIsrInstalled;                          ///< True if ISR handler is installed
    bool mNeedsConversion;                       ///< True if buffer has cycles (not yet converted to ns)
    uint32_t mSignalRangeMinNs;                ///< Minimum pulse width (noise filtering)
    uint32_t mSignalRangeMaxNs;                ///< Maximum pulse width (idle detection)
    bool mStartLow;                              ///< Pin idle state: true=LOW (WS2812B), false=HIGH (inverted)
};

// Factory method implementation
fl::shared_ptr<GpioIsrRx> GpioIsrRx::create(int pin, size_t buffer_size) {
    return fl::make_shared<GpioIsrRxImpl>(static_cast<gpio_num_t>(pin), buffer_size);
}

} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32

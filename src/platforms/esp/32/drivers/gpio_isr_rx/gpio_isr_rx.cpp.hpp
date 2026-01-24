#include "gpio_isr_rx.h"

#ifdef ESP32

// Include feature flags to detect FASTLED_RMT5
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/error.h"
#include "fl/stl/vector.h"
#include "platforms/esp/32/drivers/rmt_rx/rmt_rx_channel.h"
#include "platforms/esp/32/core/delaycycles.h"  // For get_ccount() - force-inlined cycle counter
#include "platforms/esp/32/core/fastpin_esp32.h"  // For pin validation macros

// RX device logging: Disabled by default to reduce noise
// Enable with: #define FASTLED_RX_LOG_ENABLED 1
#ifndef FASTLED_RX_LOG_ENABLED
#define FASTLED_RX_LOG_ENABLED 0
#endif

#if FASTLED_RX_LOG_ENABLED
#define FL_LOG_RX(X) FL_DBG(X)
#else
#define FL_LOG_RX(X) FL_DBG_NO_OP(X)
#endif

FL_EXTERN_C_BEGIN
#include "driver/gpio.h"
#include "driver/gptimer.h"  // For hardware timer
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
    const size_t edge_count = edges.size();
    const size_t bytes_capacity = bytes_out.size();

    if (edge_count == 0) {
        FL_WARN("decodeEdgeTimestamps: edges span is empty");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    if (bytes_capacity == 0) {
        FL_WARN("decodeEdgeTimestamps: bytes_out span is empty");
        return fl::Result<uint32_t, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    FL_LOG_RX("decodeEdgeTimestamps: decoding " << edge_count << " edges into buffer of " << bytes_capacity << " bytes");

    // Log first 100 edges for detailed analysis (using FL_WARN to ensure output)
    size_t log_edge_limit = (edge_count < 100) ? edge_count : 100;
    for (size_t idx = 0; idx < log_edge_limit; idx++) {
        FL_WARN("Edge[" << idx << "]: time_ns=" << edges[idx].time_ns << " level=" << static_cast<int>(edges[idx].level));
    }

    // Decoding state
    size_t error_count = 0;
    uint32_t bytes_decoded = 0;
    uint8_t current_byte = 0;
    int bit_index = 0; // 0-7, MSB first

    // Precompute reset threshold
    const uint32_t reset_min_ns = timing.reset_min_us * 1000;

    // Cache pointer for faster access
    const EdgeTimestamp* edge_ptr = edges.data();
    uint8_t* out_ptr = bytes_out.data();

    // Process edges in pairs
    size_t i = 0;
    while (i + 2 < edge_count) {
        const EdgeTimestamp &edge0 = edge_ptr[i];
        const EdgeTimestamp &edge1 = edge_ptr[i + 1];
        const EdgeTimestamp &edge2 = edge_ptr[i + 2];

        // Check for reset pulse (long low period indicates frame end)
        uint32_t pulse0_duration = edge1.time_ns - edge0.time_ns;
        if (edge0.level == 0 && pulse0_duration >= reset_min_ns) {
            FL_LOG_RX("decodeEdgeTimestamps: reset pulse detected at edge " << i);

            // Flush partial byte if needed
            if (bit_index != 0) {
                FL_WARN("decodeEdgeTimestamps: partial byte at reset (bit_index=" << bit_index << ")");
                current_byte <<= (8 - bit_index);
                if (bytes_decoded < bytes_capacity) {
                    out_ptr[bytes_decoded++] = current_byte;
                } else {
                    FL_WARN("decodeEdgeTimestamps: buffer overflow");
                    return fl::Result<uint32_t, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
                }
            }
            break;
        }

        // WS2812B protocol: expect HIGH -> LOW pattern
        if (edge0.level != 1 || edge1.level != 0) {
            FL_LOG_RX("decodeEdgeTimestamps: unexpected edge pattern at " << i);
            error_count++;
            i++;
            continue;
        }

        // Calculate pulse timings
        uint32_t high_ns = edge1.time_ns - edge0.time_ns;
        uint32_t low_ns = edge2.time_ns - edge1.time_ns;

        // Inline bit decoding for speed
        int bit = -1;

        // Check bit 0 pattern
        if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns &&
            low_ns >= timing.t0l_min_ns && low_ns <= timing.t0l_max_ns) {
            bit = 0;
        }
        // Check bit 1 pattern
        else if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns &&
                 low_ns >= timing.t1l_min_ns && low_ns <= timing.t1l_max_ns) {
            bit = 1;
        }
        // Check for gap pulse (transmission gap like PARLIO DMA gap)
        // Gap tolerance: LOW duration extended beyond normal but within tolerance
        // Decode bit using only HIGH duration, ignore extended LOW
        else if (timing.gap_tolerance_ns > 0 && low_ns > timing.t0l_max_ns && low_ns <= timing.gap_tolerance_ns) {
            // Try to decode bit from HIGH duration only (LOW is extended by gap)
            if (high_ns >= timing.t0h_min_ns && high_ns <= timing.t0h_max_ns) {
                bit = 0;
                FL_LOG_RX("decodeEdgeTimestamps: gap detected at edge " << i << " (low=" << low_ns << "ns), decoded bit 0 from high duration");
            }
            else if (high_ns >= timing.t1h_min_ns && high_ns <= timing.t1h_max_ns) {
                bit = 1;
                FL_LOG_RX("decodeEdgeTimestamps: gap detected at edge " << i << " (low=" << low_ns << "ns), decoded bit 1 from high duration");
            }
        }

        if (bit < 0) {
            error_count++;
            FL_LOG_RX("decodeEdgeTimestamps: invalid pulse at edge " << i << " (high=" << high_ns << "ns, low=" << low_ns << "ns)");
            i += 2;
            continue;
        }

        // Accumulate bit into byte (MSB first)
        current_byte = (current_byte << 1) | static_cast<uint8_t>(bit);
        bit_index++;

        // Log detailed info for first 3 bytes (24 bits = first LED's RGB) - using FL_WARN to ensure output
        if (bytes_decoded < 3) {
            FL_WARN("Bit[byte=" << bytes_decoded << ", bit=" << (bit_index-1) << "]: value=" << bit
                   << " (high=" << high_ns << "ns, low=" << low_ns << "ns) current_byte=0x"
                   << fl::hex << static_cast<int>(current_byte) << fl::dec);
        }

        // Byte complete?
        if (bit_index == 8) {
            if (bytes_decoded >= bytes_capacity) {
                FL_WARN("decodeEdgeTimestamps: buffer overflow at byte " << bytes_decoded);
                return fl::Result<uint32_t, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
            }
            out_ptr[bytes_decoded++] = current_byte;

            // Log completed byte for first 3 bytes - using FL_WARN to ensure output
            if (bytes_decoded <= 3) {
                FL_WARN("Byte[" << (bytes_decoded-1) << "] completed: 0x" << fl::hex << static_cast<int>(current_byte) << fl::dec);
            }

            current_byte = 0;
            bit_index = 0;
        }

        i += 2;
    }

    // Flush partial byte if we reached end
    if (bit_index != 0) {
        FL_WARN("decodeEdgeTimestamps: partial byte at end (bit_index=" << bit_index << ")");
        current_byte <<= (8 - bit_index);
        if (bytes_decoded < bytes_capacity) {
            out_ptr[bytes_decoded++] = current_byte;
        } else {
            return fl::Result<uint32_t, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
        }
    }

    FL_LOG_RX("decodeEdgeTimestamps: decoded " << bytes_decoded << " bytes, " << error_count << " errors");

    // Calculate error rate (avoid division by zero)
    size_t total_pulses = edge_count / 2;
    if (total_pulses > 0 && error_count >= (total_pulses / 10)) {
        FL_WARN("decodeEdgeTimestamps: high error rate: " << error_count << "/" << total_pulses);
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
 * Member ordering: Hot path variables first (most frequently accessed)
 */
struct alignas(64) IsrContext {
    // Hot path - accessed on EVERY ISR invocation (highest priority)
    EdgeTimestamp* writePtr;             ///< Current write position (read/write every edge)
    EdgeTimestamp* endPtr;                ///< End of buffer (read every edge for overflow check)
    uint32_t gpioInRegAddr;              ///< GPIO_IN_REG or GPIO_IN1_REG address (read every call)
    uint32_t gpioBitMask;                ///< Bit mask for pin (read every call)

    // Medium-hot path - accessed on edge detection only
    uint32_t startCycles;                ///< Start cycle count (read on edge, write once)
    uint32_t lastEdgeCycles;             ///< Last edge cycles (read/write on edge)
    uint32_t timeoutCycles;              ///< Timeout in CPU cycles (read when no edge)
    uint32_t minPulseCycles;             ///< Minimum pulse width in cycles (read on edge)

    // State variables - accessed on edge or state change
    uint8_t currentLevel;                 ///< Current pin level (read every call, write on edge)
    bool receiveDone;                     ///< Done flag (read every call, write on timeout/full)
    uint8_t _pad0[2];                     ///< Padding for alignment
    uint32_t skipCounter;                 ///< Edges to skip before recording (read/write on edge)
    size_t edgesCounter;                  ///< Edge count (write on edge, read by main)

    // Config values - rarely accessed in ISR (read-only after init)
    gpio_num_t pin;                       ///< GPIO pin number (stored for debug)
    uint32_t cpuFreqMhz;                 ///< CPU frequency in MHz (main thread conversion)

    // Timer handle - accessed only on done condition
    gptimer_handle_t hwTimer;            ///< Hardware timer handle
    bool timerStarted;                    ///< Track if timer has been started
};

/**
 * @brief Implementation of GPIO ISR RX Channel
 *
 * All ISR-related methods are marked IRAM_ATTR to ensure they run from IRAM.
 * Uses volatile for all variables accessed from both ISR and main code.
 */
class GpioIsrRxImpl : public GpioIsrRx {
public:
    GpioIsrRxImpl(int pin)
        : mIsrCtx{}
        , mPin(static_cast<gpio_num_t>(pin))
        , mBufferSize(0)
        , mEdgeBuffer()
        , mIsrInstalled(false)
        , mNeedsConversion(false)
        , mSignalRangeMinNs(100)
        , mSignalRangeMaxNs(100000)
        , mStartLow(true)
    {
        FL_LOG_RX("GpioIsrRx constructed with pin=" << pin << " (other hardware params will be set in begin())");

        // Initialize ISR context in optimal order (matching struct layout)
        // Hot path variables
        mIsrCtx.writePtr = nullptr;
        mIsrCtx.endPtr = nullptr;
        // Set pin-specific values early (will be validated in begin())
#ifdef GPIO_IN1_REG
        // ESP32/S2/S3 have >32 pins and use GPIO_IN1_REG for pins 32+
        mIsrCtx.gpioInRegAddr = (mPin < 32) ? GPIO_IN_REG : GPIO_IN1_REG;
        uint8_t pin_bit = (mPin < 32) ? mPin : (mPin - 32);
#else
        // ESP32-C3/C6/H2 have ≤32 pins and only use GPIO_IN_REG
        mIsrCtx.gpioInRegAddr = GPIO_IN_REG;
        uint8_t pin_bit = mPin;
#endif
        mIsrCtx.gpioBitMask = (1U << pin_bit);

        // Medium-hot path
        mIsrCtx.startCycles = 0;
        mIsrCtx.lastEdgeCycles = 0;
        mIsrCtx.timeoutCycles = 24000;  // Default ~100us @ 240MHz, updated in begin()
        mIsrCtx.minPulseCycles = 24;    // Default ~100ns @ 240MHz, updated in begin()

        // State variables
        mIsrCtx.currentLevel = 0;
        mIsrCtx.receiveDone = false;
        mIsrCtx.skipCounter = 0;
        mIsrCtx.edgesCounter = 0;

        // Config values
        mIsrCtx.pin = mPin;
        mIsrCtx.hwTimer = nullptr;
        mIsrCtx.timerStarted = false;

        // Get actual CPU frequency (Arduino ESP32 method)
        rtc_cpu_freq_config_t freq_config;
        rtc_clk_cpu_freq_get_config(&freq_config);
        mIsrCtx.cpuFreqMhz = freq_config.freq_mhz;
    }

    ~GpioIsrRxImpl() override {
        // Stop and delete hardware timer if created
        if (mIsrCtx.hwTimer != nullptr) {
            if (mIsrCtx.timerStarted) {
                gptimer_stop(mIsrCtx.hwTimer);
                mIsrCtx.timerStarted = false;
            }
            gptimer_disable(mIsrCtx.hwTimer);
            gptimer_del_timer(mIsrCtx.hwTimer);
            mIsrCtx.hwTimer = nullptr;
        }

        mIsrInstalled = false;
    }

    bool begin(const RxConfig& config) override {
        // Validate and extract hardware parameters on first call
        if (mIsrCtx.hwTimer == nullptr) {
            // First-time initialization - extract hardware parameters from config
            if (config.buffer_size == 0) {
                FL_WARN("GPIO ISR RX begin: Invalid buffer_size in config (buffer_size=0)");
                return false;
            }

            // Validate pin (set in constructor)
            if (!isValidGpioPin(static_cast<int>(mPin))) {
                FL_ERROR("GPIO ISR RX: Invalid pin " << static_cast<int>(mPin)
                         << " - pin is reserved for UART, flash, or other system use. "
                         << "Please choose a different GPIO pin.");
                return false;
            }

            mBufferSize = config.buffer_size;

            FL_LOG_RX("GPIO ISR RX first-time init: pin=" << static_cast<int>(mPin)
                   << ", buffer_size=" << mBufferSize);
        }

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

        FL_LOG_RX("GPIO ISR RX begin: signal_range_min=" << mSignalRangeMinNs
               << "ns, signal_range_max=" << mSignalRangeMaxNs << "ns"
               << ", skip_signals=" << config.skip_signals
               << ", start_low=" << (mStartLow ? "true" : "false"));

        // Create hardware timer ISR if not already created
        if (mIsrCtx.hwTimer == nullptr) {
            gptimer_config_t timer_config = {
                .clk_src = GPTIMER_CLK_SRC_DEFAULT,
                .direction = GPTIMER_COUNT_UP,
                .resolution_hz = 1000000,  // 1MHz = 1µs resolution
            };

            esp_err_t err = gptimer_new_timer(&timer_config, &mIsrCtx.hwTimer);
            if (err != ESP_OK) {
                FL_WARN("Failed to create hardware timer: " << static_cast<int>(err));
                return false;
            }

            // Register timer alarm callback
            gptimer_event_callbacks_t cbs = {
                .on_alarm = timerPollingISR,
            };
            err = gptimer_register_event_callbacks(mIsrCtx.hwTimer, &cbs, &mIsrCtx);
            if (err != ESP_OK) {
                FL_WARN("Failed to register timer callback: " << static_cast<int>(err));
                gptimer_del_timer(mIsrCtx.hwTimer);
                mIsrCtx.hwTimer = nullptr;
                return false;
            }

            // Configure alarm: fire every 10µs (original timing - WORKS)
            gptimer_alarm_config_t alarm_config = {
                .alarm_count = 10,  // 10µs interval (with 1MHz resolution)
                .reload_count = 0,
                .flags = {
                    .auto_reload_on_alarm = true,
                },
            };
            err = gptimer_set_alarm_action(mIsrCtx.hwTimer, &alarm_config);
            if (err != ESP_OK) {
                FL_WARN("Failed to set timer alarm: " << static_cast<int>(err));
                gptimer_del_timer(mIsrCtx.hwTimer);
                mIsrCtx.hwTimer = nullptr;
                return false;
            }

            // Enable timer
            err = gptimer_enable(mIsrCtx.hwTimer);
            if (err != ESP_OK) {
                FL_WARN("Failed to enable timer: " << static_cast<int>(err));
                gptimer_del_timer(mIsrCtx.hwTimer);
                mIsrCtx.hwTimer = nullptr;
                return false;
            }

            FL_LOG_RX("Hardware timer created successfully");
        }

        // If already initialized, just re-arm the receiver for a new capture
        if (mIsrInstalled) {
            FL_LOG_RX("Timer ISR already initialized, re-arming receiver");

            // Check if ISR is still armed from previous capture (error condition)
            if (!mIsrCtx.receiveDone) {
                FL_ERROR("Timer ISR is still armed from previous capture - call wait() or check finished() before calling begin() again");
                return false;
            }

            // Clear receive state
            clear();

            // Start timer ISR
            esp_err_t err = gptimer_start(mIsrCtx.hwTimer);
            if (err != ESP_OK) {
                FL_WARN("Failed to start timer: " << static_cast<int>(err));
                return false;
            }
            mIsrCtx.timerStarted = true;

            FL_LOG_RX("Hardware timer receiver re-armed and ready");
            return true;
        }

        // First-time initialization
        // Clear receive state
        clear();

        // Allocate edge buffer (ISR writes cycles, main thread converts to ns)
        mEdgeBuffer.clear();
        mEdgeBuffer.reserve(mBufferSize);
        for (size_t i = 0; i < mBufferSize; i++) {
            mEdgeBuffer.push_back({0, 0});
        }

        // Initialize ISR context pointers
        mIsrCtx.writePtr = mEdgeBuffer.data();
        mIsrCtx.endPtr = mEdgeBuffer.data() + mBufferSize;

        // Start hardware timer
        esp_err_t err = gptimer_start(mIsrCtx.hwTimer);
        if (err != ESP_OK) {
            FL_WARN("Failed to start timer: " << static_cast<int>(err));
            return false;
        }
        mIsrCtx.timerStarted = true;

        mIsrInstalled = true;

        // Verify ISR is armed by checking that receiveDone is false (initial state)
        if (mIsrCtx.receiveDone) {
            FL_WARN("Timer ISR started but receiver state is invalid (receiveDone=true)");
            return false;
        }

        FL_LOG_RX("Timer ISR started successfully - polling at 2µs intervals for ±1µs precision");

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

        FL_LOG_RX("wait(): buffer_size=" << mBufferSize << ", timeout_ms=" << timeout_ms);

        // Convert timeout to microseconds for comparison
        int64_t timeout_us = static_cast<int64_t>(timeout_ms) * 1000;

        // Busy-wait with yield (using ESP-IDF native functions)
        int64_t wait_start_us = esp_timer_get_time();
        while (!finished()) {
            int64_t elapsed_us = esp_timer_get_time() - wait_start_us;

            // Check if buffer filled (success condition)
            if (mIsrCtx.edgesCounter >= mBufferSize) {
                FL_LOG_RX("wait(): buffer filled (" << mIsrCtx.edgesCounter << ")");
                return RxWaitResult::SUCCESS;
            }

            // Check wait timeout
            if (elapsed_us >= timeout_us) {
                FL_WARN("wait(): timeout after " << elapsed_us << "us, captured " << mIsrCtx.edgesCounter << " edges");
                return RxWaitResult::TIMEOUT;
            }

            taskYIELD();  // Allow other tasks to run
        }

        // Receive completed (ISR timeout triggered)
        FL_LOG_RX("wait(): receive done, count=" << mIsrCtx.edgesCounter);
        return RxWaitResult::SUCCESS;
    }

    fl::span<const EdgeTimestamp> getEdges() const override {
        if (mEdgeBuffer.empty()) {
            return fl::span<const EdgeTimestamp>();
        }

        // Memory barrier: ensure all ISR writes are visible to main thread
        __asm__ __volatile__("" ::: "memory");  // Compiler barrier
        #if CONFIG_IDF_TARGET_ARCH_XTENSA
        asm volatile("memw" ::: "memory");      // Hardware barrier (Xtensa)
        #elif CONFIG_IDF_TARGET_ARCH_RISCV
        asm volatile("fence" ::: "memory");     // Hardware barrier (RISC-V)
        #endif

        // Convert cycles to nanoseconds in-place (only once, when needed)
        if (mNeedsConversion) {
            const uint32_t cpuMhz = mIsrCtx.cpuFreqMhz;
            const size_t count = mIsrCtx.edgesCounter;
            EdgeTimestamp* buffer = const_cast<EdgeTimestamp*>(mEdgeBuffer.data());

            // Optimized conversion loop - compiler can vectorize this
            for (size_t i = 0; i < count; i++) {
                // Convert: ns = (cycles * 1000) / CPU_MHz
                buffer[i].time_ns = (buffer[i].cycles * 1000) / cpuMhz;
            }
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
        // Stop timer if running
        if (mIsrCtx.timerStarted && mIsrCtx.hwTimer != nullptr) {
            gptimer_stop(mIsrCtx.hwTimer);
            mIsrCtx.timerStarted = false;
        }

        mIsrCtx.receiveDone = false;
        mIsrCtx.edgesCounter = 0;

        // Reset ISR context
        mIsrCtx.writePtr = mEdgeBuffer.data();
        mIsrCtx.lastEdgeCycles = 0;
        mIsrCtx.startCycles = 0;  // Will be set on first edge
        mIsrCtx.currentLevel = mStartLow ? 0 : 1;

        // Mark buffer as needing conversion (will contain cycles after ISR)
        mNeedsConversion = true;

        FL_LOG_RX("GPIO ISR RX state cleared");
    }

    /**
     * @brief Timer-based ISR that polls GPIO and detects edges
     *
     * Fires periodically at 2µs interval to poll GPIO pin state.
     * Detects edges by comparing current level to previous level.
     * Also handles idle timeout detection (no edges for timeout period).
     *
     * WARNING: 2µs interval is below ESP-IDF recommended 5µs minimum.
     * This polling provides ±1µs precision (5x better than original 10µs).
     *
     * Optimizations:
     * - FL_IRAM placement for zero-wait-state execution
     * - Direct GPIO register access with precomputed address/mask
     * - CPU cycle counter for accurate timestamps
     * - Minimal branches, optimized for fast path
     */
    FL_IRAM static bool __attribute__((optimize("O3"),hot)) timerPollingISR(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx) {
        // Cast user context to IsrContext
        IsrContext* ctx = static_cast<IsrContext*>(user_ctx);

        // CRITICAL PATH OPTIMIZATION: Minimize checks before GPIO read
        // Read pin state directly from register FIRST (fastest method - uses precomputed address & mask)
        uint32_t gpio_in_reg = REG_READ(ctx->gpioInRegAddr);
        uint8_t new_level = (gpio_in_reg & ctx->gpioBitMask) ? 1 : 0;

        // Fast path: no edge detected (most common case at 500ns polling)
        uint8_t current_level = ctx->currentLevel;
        if (__builtin_expect(new_level == current_level, 1)) {
            // Check if already done
            if (__builtin_expect(ctx->receiveDone, 0)) {
                // Stop timer from ISR (only once)
                if (ctx->timerStarted) {
                    gptimer_stop(ctx->hwTimer);
                    ctx->timerStarted = false;
                }
                return false;  // Don't yield from ISR
            }

            // Check for idle timeout only if we have captured edges
            if (__builtin_expect(ctx->edgesCounter > 0, 0)) {
                uint32_t now_cycles = __clock_cycles();
                uint32_t cycles_since_last = now_cycles - ctx->lastEdgeCycles;
                if (cycles_since_last >= ctx->timeoutCycles) {
                    ctx->receiveDone = true;
                }
            }
            return false;
        }

        // Edge detected - get timestamp
        uint32_t now_cycles = __clock_cycles();

        // Initialize start cycles on first edge
        uint32_t start_cycles = ctx->startCycles;
        bool is_first_edge = (start_cycles == 0);
        if (__builtin_expect(is_first_edge, 0)) {
            ctx->startCycles = now_cycles;
            ctx->lastEdgeCycles = now_cycles;
            start_cycles = now_cycles;  // Cache for elapsed calculation below
        } else {
            // Noise filter: reject pulses shorter than minimum (skip on first edge)
            uint32_t cycles_since_last = now_cycles - ctx->lastEdgeCycles;
            if (__builtin_expect(cycles_since_last < ctx->minPulseCycles, 0)) {
                ctx->currentLevel = new_level;
                return false;
            }
            // Update last edge timestamp
            ctx->lastEdgeCycles = now_cycles;
        }

        // Update level
        ctx->currentLevel = new_level;

        // Skip counter (honor skip_signals config)
        uint32_t skip = ctx->skipCounter;
        if (__builtin_expect(skip > 0, 0)) {
            ctx->skipCounter = skip - 1;
            return false;
        }

        // Load write pointer once
        EdgeTimestamp* ptr = ctx->writePtr;

        // Buffer full check
        if (__builtin_expect(ptr >= ctx->endPtr, 0)) {
            ctx->receiveDone = true;
            return false;  // Next cycle will disarm.
        }

        // Write edge (fastest path - store cycles directly)
        uint32_t elapsed_cycles = now_cycles - start_cycles;
        ptr->cycles = elapsed_cycles;
        ptr->level = new_level;

        // Advance pointer and counter
        ctx->writePtr = ptr + 1;
        ctx->edgesCounter++;

        // Check if buffer is now full
        if ((ptr + 1) >= ctx->endPtr) {
            ctx->receiveDone = true;
        }

        return false;
    }

    size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) override {
        // Ensure conversion from cycles to nanoseconds has happened
        fl::span<const EdgeTimestamp> edges = getEdges();

        if (edges.empty()) {
            return 0;
        }

        // First pass: Count total valid edges
        size_t total_edges = 0;
        for (size_t i = 0; i < edges.size(); ) {
            uint8_t current_state = edges[i].level;
            uint32_t state_start_ns = edges[i].time_ns;

            size_t next_i = i + 1;
            while (next_i < edges.size() && edges[next_i].level == current_state) {
                next_i++;
            }

            if (next_i < edges.size()) {
                uint32_t accumulated_ns = edges[next_i].time_ns - state_start_ns;
                if (accumulated_ns >= mSignalRangeMinNs) {
                    total_edges++;
                }
            }

            i = next_i;
        }

        // Calculate range to extract
        size_t start_edge = offset;
        size_t end_edge = offset + out.size();  // span size = max count

        if (start_edge >= total_edges) return 0;
        if (end_edge > total_edges) end_edge = total_edges;

        // Second pass: Extract edges in requested range
        size_t current_edge = 0;
        size_t out_index = 0;

        for (size_t i = 0; i < edges.size() && out_index < out.size(); ) {
            uint8_t current_state = edges[i].level;
            uint32_t state_start_ns = edges[i].time_ns;

            size_t next_i = i + 1;
            while (next_i < edges.size() && edges[next_i].level == current_state) {
                next_i++;
            }

            uint32_t accumulated_ns;
            if (next_i < edges.size()) {
                accumulated_ns = edges[next_i].time_ns - state_start_ns;
            } else {
                break;
            }

            if (accumulated_ns >= mSignalRangeMinNs) {
                if (current_edge >= start_edge && current_edge < end_edge) {
                    out[out_index] = EdgeTime(current_state == 1, accumulated_ns);
                    out_index++;
                }
                current_edge++;
                if (current_edge >= end_edge) break;
            }

            i = next_i;
        }

        return out_index;
    }

    const char* name() const override {
        return "ISR";
    }

    int getPin() const override {
        return static_cast<int>(mPin);
    }

    bool injectEdges(fl::span<const EdgeTime> edges) override {
        if (edges.empty()) {
            FL_WARN("injectEdges(): empty edges span");
            return false;
        }

        // Allocate edge buffer if needed
        if (mEdgeBuffer.empty()) {
            mEdgeBuffer.clear();
            mEdgeBuffer.reserve(edges.size());
            for (size_t i = 0; i < edges.size(); i++) {
                mEdgeBuffer.push_back({0, 0});
            }
        } else if (mEdgeBuffer.size() < edges.size()) {
            FL_WARN("injectEdges(): edge buffer too small (need " << edges.size()
                    << ", have " << mEdgeBuffer.size() << ")");
            return false;
        }

        FL_LOG_RX("injectEdges(): injecting " << edges.size() << " edges into GPIO ISR RX buffer");

        // Convert EdgeTime to EdgeTimestamp (accumulate nanoseconds)
        // EdgeTime stores durations, EdgeTimestamp stores transition times
        uint32_t accumulated_ns = 0;
        for (size_t i = 0; i < edges.size(); i++) {
            const EdgeTime& edge = edges[i];

            // Store transition time BEFORE adding duration
            // This represents when the signal transitions TO this level
            mEdgeBuffer[i].time_ns = accumulated_ns;
            mEdgeBuffer[i].level = edge.high ? 1 : 0;

            // Accumulate timestamp AFTER storing
            accumulated_ns += edge.ns;

            FL_LOG_RX("  Edge[" << i << "]: time_ns=" << mEdgeBuffer[i].time_ns
                   << ", level=" << (int)mEdgeBuffer[i].level
                   << " (duration=" << edge.ns << "ns)");
        }

        // Update state
        mIsrCtx.edgesCounter = edges.size();
        mIsrCtx.receiveDone = true;
        mNeedsConversion = false;  // Data is already in nanoseconds

        FL_LOG_RX("injectEdges(): injected " << edges.size() << " edges successfully");
        return true;
    }


    // ISR-optimized context - single structure for fast access
    IsrContext mIsrCtx;

    // Non-ISR members
    gpio_num_t mPin;                              ///< GPIO pin for RX
    size_t mBufferSize;                          ///< Buffer size for edge timestamps
    fl::vector<EdgeTimestamp> mEdgeBuffer;   ///< Buffer (ISR writes cycles, getEdges() converts to ns)
    bool mIsrInstalled;                          ///< True if ISR handler is installed
    bool mNeedsConversion;                       ///< True if buffer has cycles (not yet converted to ns)
    uint32_t mSignalRangeMinNs;                ///< Minimum pulse width (noise filtering)
    uint32_t mSignalRangeMaxNs;                ///< Maximum pulse width (idle detection)
    bool mStartLow;                              ///< Pin idle state: true=LOW (WS2812B), false=HIGH (inverted)
};

// Factory method implementation
fl::shared_ptr<GpioIsrRx> GpioIsrRx::create(int pin) {
    return fl::make_shared<GpioIsrRxImpl>(pin);
}

} // namespace fl

#endif // FASTLED_RMT5
#endif // ESP32

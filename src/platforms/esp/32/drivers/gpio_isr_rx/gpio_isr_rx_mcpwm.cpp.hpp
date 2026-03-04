// IWYU pragma: private

/**
 * @file gpio_isr_rx_mcpwm.cpp.hpp
 * @brief Dual-ISR GPIO RX implementation with MCPWM hardware timestamps
 *
 * Architecture:
 * - Fast ISR (RISC-V assembly, GPIO edge-triggered): Captures edges with MCPWM timestamps
 * - Slow ISR (C, GPTimer 10µs interval): Processes buffer, applies filtering, manages completion
 *
 * Performance:
 * - Fast ISR: 25-31 cycles (~104-129 ns @ 240 MHz)
 * - Timestamp resolution: 12.5 ns (MCPWM @ 80 MHz)
 * - Edge capture rate: >1 MHz
 * - CPU load target: <20% during active capture
 *
 * Integration:
 * - Uses DualIsrContext from dual_isr_context.h
 * - Uses gpio_fast_edge_isr from fast_isr.S
 * - Uses mcpwm_timer functions from mcpwm_timer.cpp
 */

#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "gpio_isr_rx.h"
#include "dual_isr_context.h"
#include "mcpwm_timer.h"

// Include feature flags to detect FASTLED_RMT5
#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "fl/dbg.h"
#include "fl/warn.h"
#include "fl/error.h"
// IWYU pragma: begin_keep
#include "fl/stl/vector.h"
// IWYU pragma: end_keep
#include "platforms/esp/32/core/fastpin_esp32.h"  // For pin validation macros

FL_EXTERN_C_BEGIN
// IWYU pragma: begin_keep
#include "driver/gpio.h"
#include "driver/gptimer.h"  // For GPTimer (slow ISR)
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"  // For esp_intr_alloc, intr_handle_t
#include "soc/gpio_reg.h"  // For GPIO_IN_REG, GPIO_IN1_REG
#include "soc/interrupts.h"  // For ETS_GPIO_INTR_SOURCE
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// IWYU pragma: end_keep
FL_EXTERN_C_END

#ifdef __riscv
// ============================================================================
// RISC-V High-Priority Interrupt Support
// ============================================================================

extern "C" {
    // Our fast ISR handler (implemented in fast_isr.S)
    extern void gpio_fast_edge_isr(void);
}

namespace {

/**
 * @brief Install direct GPIO ISR using esp_intr_alloc() with high priority
 *
 * Uses ESP-IDF's interrupt allocation API with Level 3-5 priority for
 * low-latency interrupt handling (~300-400ns). This is the safe, supported
 * approach that avoids conflicts with ESP-IDF's interrupt system.
 *
 * @param gpio_num GPIO pin number
 * @param isr_context Pointer to DualIsrContext for ISR
 * @param interrupt_handle Output parameter for allocated interrupt handle
 * @return true on success, false on failure
 */
bool install_direct_gpio_isr(int gpio_num, void* isr_context, intr_handle_t* interrupt_handle) {
    FL_DBG("Installing direct GPIO ISR for pin " << gpio_num);

    // Step 1: Determine GPIO interrupt source
    int intr_source = ETS_GPIO_INTR_SOURCE;
    FL_DBG("GPIO interrupt source: " << intr_source);

    // Step 2: Allocate interrupt with high priority (Level 3-5)
    // ESP_INTR_FLAG_LEVEL3 = Higher priority than default (Level 1)
    // ESP_INTR_FLAG_IRAM = ISR code must be in IRAM (already is)
    int intr_flags = ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM;

    esp_err_t err = esp_intr_alloc(
        intr_source,
        intr_flags,
        reinterpret_cast<intr_handler_t>(gpio_fast_edge_isr),  // ok reinterpret cast
        isr_context,
        interrupt_handle
    );

    if (err != ESP_OK) {
        FL_WARN("esp_intr_alloc failed: " << esp_err_to_name(err));
        return false;
    }

    FL_DBG("Allocated interrupt handle: " << *interrupt_handle);

    // Step 3: Configure GPIO pin
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << gpio_num);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;

    err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        FL_WARN("gpio_config failed: " << esp_err_to_name(err));
        esp_intr_free(*interrupt_handle);
        *interrupt_handle = nullptr;
        return false;
    }

    // Step 4: Enable GPIO interrupt
    err = gpio_intr_enable(static_cast<gpio_num_t>(gpio_num));
    if (err != ESP_OK) {
        FL_WARN("gpio_intr_enable failed: " << esp_err_to_name(err));
        esp_intr_free(*interrupt_handle);
        *interrupt_handle = nullptr;
        return false;
    }

    FL_DBG("Direct GPIO ISR installed successfully (expected latency: ~300-400ns)");
    return true;
}

} // anonymous namespace

#endif // __riscv

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

// ============================================================================
// Edge Timestamp Decoder (Ported from gpio_isr_rx.cpp.disabled)
// ============================================================================

/**
 * @brief Decode a single pulse bit from HIGH and LOW timings
 * @param high_ns HIGH duration in nanoseconds
 * @param low_ns LOW duration in nanoseconds
 * @param timing Chipset timing thresholds
 * @return 0 for bit 0, 1 for bit 1, -1 for invalid timing
 */
inline int decodePulseBit(u32 high_ns, u32 low_ns, const ChipsetTiming4Phase &timing) {
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
inline bool isResetPulse(u32 duration_ns, const ChipsetTiming4Phase &timing) {
    u32 reset_min_ns = timing.reset_min_us * 1000;
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
fl::Result<u32, DecodeError> decodeEdgeTimestamps(const ChipsetTiming4Phase &timing,
                                                         fl::span<const EdgeTimestamp> edges,
                                                         fl::span<u8> bytes_out) {
    const size_t edge_count = edges.size();
    const size_t bytes_capacity = bytes_out.size();

    if (edge_count == 0) {
        FL_WARN("decodeEdgeTimestamps: edges span is empty");
        return fl::Result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    if (bytes_capacity == 0) {
        FL_WARN("decodeEdgeTimestamps: bytes_out span is empty");
        return fl::Result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }

    FL_DBG("decodeEdgeTimestamps: decoding " << edge_count << " edges into buffer of " << bytes_capacity << " bytes");

    // Decoding state
    size_t error_count = 0;
    u32 bytes_decoded = 0;
    u8 current_byte = 0;
    int bit_index = 0; // 0-7, MSB first

    // Precompute reset threshold
    const u32 reset_min_ns = timing.reset_min_us * 1000;

    // Cache pointer for faster access
    const EdgeTimestamp* edge_ptr = edges.data();
    u8* out_ptr = bytes_out.data();

    // Process edges in pairs
    size_t i = 0;
    while (i + 2 < edge_count) {
        const EdgeTimestamp &edge0 = edge_ptr[i];
        const EdgeTimestamp &edge1 = edge_ptr[i + 1];
        const EdgeTimestamp &edge2 = edge_ptr[i + 2];

        // Check for reset pulse (long low period indicates frame end)
        u32 pulse0_duration = edge1.time_ns - edge0.time_ns;
        if (edge0.level == 0 && pulse0_duration >= reset_min_ns) {
            FL_DBG("decodeEdgeTimestamps: reset pulse detected at edge " << i);

            // Flush partial byte if needed
            if (bit_index != 0) {
                FL_WARN("decodeEdgeTimestamps: partial byte at reset (bit_index=" << bit_index << ")");
                current_byte <<= (8 - bit_index);
                if (bytes_decoded < bytes_capacity) {
                    out_ptr[bytes_decoded++] = current_byte;
                } else {
                    FL_WARN("decodeEdgeTimestamps: buffer overflow");
                    return fl::Result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
                }
            }
            break;
        }

        // WS2812B protocol: expect HIGH -> LOW pattern
        if (edge0.level != 1 || edge1.level != 0) {
            FL_DBG("decodeEdgeTimestamps: unexpected edge pattern at " << i);
            error_count++;
            i++;
            continue;
        }

        // Calculate pulse timings
        u32 high_ns = edge1.time_ns - edge0.time_ns;
        u32 low_ns = edge2.time_ns - edge1.time_ns;

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

        if (bit < 0) {
            error_count++;
            FL_DBG("decodeEdgeTimestamps: invalid pulse at edge " << i);
            i += 2;
            continue;
        }

        // Accumulate bit into byte (MSB first)
        current_byte = (current_byte << 1) | static_cast<u8>(bit);
        bit_index++;

        // Byte complete?
        if (bit_index == 8) {
            if (bytes_decoded >= bytes_capacity) {
                FL_WARN("decodeEdgeTimestamps: buffer overflow at byte " << bytes_decoded);
                return fl::Result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
            }
            out_ptr[bytes_decoded++] = current_byte;
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
            return fl::Result<u32, DecodeError>::failure(DecodeError::BUFFER_OVERFLOW);
        }
    }

    FL_DBG("decodeEdgeTimestamps: decoded " << bytes_decoded << " bytes, " << error_count << " errors");

    // Calculate error rate (avoid division by zero)
    size_t total_pulses = edge_count / 2;
    if (total_pulses > 0 && error_count >= (total_pulses / 10)) {
        FL_WARN("decodeEdgeTimestamps: high error rate: " << error_count << "/" << total_pulses);
        return fl::Result<u32, DecodeError>::failure(DecodeError::HIGH_ERROR_RATE);
    }

    return fl::Result<u32, DecodeError>::success(bytes_decoded);
}

} // anonymous namespace

// ============================================================================
// GPIO ISR RX MCPWM Implementation
// ============================================================================

/**
 * @brief Dual-ISR GPIO RX implementation with MCPWM timestamps
 *
 * Uses hardware timestamps from MCPWM timer (12.5 ns resolution) captured
 * by ultra-fast RISC-V assembly ISR, then processed by management ISR.
 */
class GpioIsrRxMcpwm : public GpioIsrRx {
public:
    GpioIsrRxMcpwm(int pin)
        : mPin(static_cast<gpio_num_t>(pin))
        , mBufferSize(0)
        , mOutputBufferSize(0)
        , mEdgeBuffer()
        , mOutputBuffer()
        , mIsrContext{}
        , mInitialized(false)
    {
        FL_DBG("GpioIsrRxMcpwm constructed with pin=" << pin);
    }

    ~GpioIsrRxMcpwm() override {
        cleanup();
    }

    bool begin(const RxConfig& config) override {
        // Validate pin
        if (!isValidGpioPin(static_cast<int>(mPin))) {
            FL_ERROR("GPIO ISR RX MCPWM: Invalid pin " << static_cast<int>(mPin)
                     << " - pin is reserved for UART, flash, or other system use");
            return false;
        }

        // Validate buffer size
        if (config.buffer_size == 0) {
            FL_WARN("GPIO ISR RX MCPWM begin: Invalid buffer_size (must be > 0)");
            return false;
        }

        // Buffer size must be power of 2 for fast modulo optimization
        if ((config.buffer_size & (config.buffer_size - 1)) != 0) {
            FL_ERROR("GPIO ISR RX MCPWM begin: buffer_size must be power of 2 (got "
                     << config.buffer_size << ")");
            return false;
        }

        // First-time initialization
        if (!mInitialized) {
            mBufferSize = config.buffer_size;
            mOutputBufferSize = config.buffer_size;  // Same size for simplicity

            FL_DBG("GPIO ISR RX MCPWM first-time init: pin=" << static_cast<int>(mPin)
                   << ", buffer_size=" << mBufferSize);

            // Allocate circular buffer
            mEdgeBuffer.clear();
            mEdgeBuffer.reserve(mBufferSize);
            for (size_t i = 0; i < mBufferSize; i++) {
                mEdgeBuffer.push_back({0, 0, {0, 0, 0}});
            }

            // Allocate output buffer
            mOutputBuffer.clear();
            mOutputBuffer.reserve(mOutputBufferSize);
            for (size_t i = 0; i < mOutputBufferSize; i++) {
                mOutputBuffer.push_back({0, 0, {0, 0, 0}});
            }

            // Initialize DualIsrContext
            mIsrContext.buffer = mEdgeBuffer.data();
            mIsrContext.buffer_size = static_cast<u32>(mBufferSize);
            mIsrContext.buffer_size_mask = static_cast<u32>(mBufferSize - 1);
            mIsrContext.write_index = 0;
            mIsrContext.read_index = 0;
            mIsrContext.armed = false;  // Not armed yet
            mIsrContext.pin = static_cast<int>(mPin);
            mIsrContext.gptimer_handle = nullptr;
            mIsrContext.intr_handle = nullptr;  // Initialize interrupt handle
            mIsrContext.output_buffer = mOutputBuffer.data();
            mIsrContext.output_buffer_size = static_cast<u32>(mOutputBufferSize);
            mIsrContext.output_count = 0;
            mIsrContext.last_edge_timestamp = 0;
            mIsrContext.done = false;

            // Configure GPIO pin for input (if not already configured)
            gpio_config_t io_conf = {};
            io_conf.pin_bit_mask = (1ULL << mPin);
            io_conf.mode = GPIO_MODE_INPUT;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.intr_type = GPIO_INTR_ANYEDGE;
            esp_err_t err = gpio_config(&io_conf);
            if (err != ESP_OK) {
                FL_WARN("Failed to configure GPIO: " << static_cast<int>(err));
                return false;
            }

            // Set GPIO register address and bit mask for fast ISR
            mIsrContext.gpio_in_reg_addr = (mPin < 32) ? GPIO_IN_REG : GPIO_IN1_REG;
            u8 pin_bit = (mPin < 32) ? mPin : (mPin - 32);
            mIsrContext.gpio_bit_mask = (1U << pin_bit);

            // Initialize MCPWM timer
            if (mcpwm_timer_init(&mIsrContext, static_cast<int>(mPin)) != 0) {
                FL_WARN("Failed to initialize MCPWM timer");
                return false;
            }

            // Create GPTimer for slow ISR (10µs interval)
            gptimer_config_t timer_config = {
                .clk_src = GPTIMER_CLK_SRC_DEFAULT,
                .direction = GPTIMER_COUNT_UP,
                .resolution_hz = 1000000,  // 1 MHz = 1µs resolution
            };

            err = gptimer_new_timer(&timer_config, reinterpret_cast<gptimer_handle_t*>(&mIsrContext.gptimer_handle)); // ok reinterpret cast
            if (err != ESP_OK) {
                FL_WARN("Failed to create GPTimer: " << static_cast<int>(err));
                mcpwm_timer_cleanup();
                return false;
            }

            // Register slow ISR callback
            gptimer_event_callbacks_t cbs = {
                .on_alarm = slowManagementISR,
            };
            err = gptimer_register_event_callbacks(
                reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle), // ok reinterpret cast
                &cbs,
                &mIsrContext);
            if (err != ESP_OK) {
                FL_WARN("Failed to register GPTimer callback: " << static_cast<int>(err));
                gptimer_del_timer(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
                mIsrContext.gptimer_handle = nullptr;
                mcpwm_timer_cleanup();
                return false;
            }

            // Configure GPTimer alarm: fire every 10µs
            gptimer_alarm_config_t alarm_config = {
                .alarm_count = 10,  // 10µs interval (with 1MHz resolution)
                .reload_count = 0,
                .flags = {
                    .auto_reload_on_alarm = true,
                },
            };
            err = gptimer_set_alarm_action(
                reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle), // ok reinterpret cast
                &alarm_config);
            if (err != ESP_OK) {
                FL_WARN("Failed to set GPTimer alarm: " << static_cast<int>(err));
                gptimer_del_timer(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
                mIsrContext.gptimer_handle = nullptr;
                mcpwm_timer_cleanup();
                return false;
            }

            // Enable GPTimer
            err = gptimer_enable(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
            if (err != ESP_OK) {
                FL_WARN("Failed to enable GPTimer: " << static_cast<int>(err));
                gptimer_del_timer(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
                mIsrContext.gptimer_handle = nullptr;
                mcpwm_timer_cleanup();
                return false;
            }

            // Configure GPIO interrupt with platform-specific implementation
            #ifdef __riscv
            // RISC-V: Use esp_intr_alloc() with high priority (Level 3-5)
            // This provides low-latency interrupt handling (~300-400ns) while remaining
            // fully compatible with ESP-IDF's interrupt system
            if (!install_direct_gpio_isr(static_cast<int>(mPin), &mIsrContext, &mInterruptHandle)) {
                FL_WARN("Failed to install direct GPIO ISR for pin " << static_cast<int>(mPin));
                gptimer_disable(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
                gptimer_del_timer(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
                mIsrContext.gptimer_handle = nullptr;
                mcpwm_timer_cleanup();
                return false;
            }

            FL_DBG("Using RISC-V high-priority ISR (Level 3) - expect ~300-400ns latency");
            #else
            // Xtensa: Use ESP-IDF interrupt allocation (existing implementation)
            // Direct allocation with esp_intr_alloc() is required for assembly ISR handlers
            // that use mret instruction. GPIO ISR service (gpio_install_isr_service) uses
            // C callback wrappers which are incompatible with mret.
            //
            // Per ESP-IDF docs (esp_intr_alloc.h:138-139):
            // "Must be NULL when an interrupt of level >3 is requested"
            int flags = ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL5;

            FL_DBG("Allocating GPIO interrupt: source=" << ETS_GPIO_INTR_SOURCE
                   << " flags=0x" << fl::hex << flags << fl::dec
                   << " handler=nullptr (Level 5 requires nullptr per ESP-IDF docs)"
                   << " arg=nullptr (cannot pass context with nullptr handler)");

            err = esp_intr_alloc(
                ETS_GPIO_INTR_SOURCE,                           // GPIO interrupt source
                flags,                                           // IRAM + Level 5
                nullptr,                                         // MUST be nullptr for Level 5
                nullptr,                                         // Cannot pass context with nullptr handler
                reinterpret_cast<intr_handle_t*>(&mIsrContext.intr_handle)  // ok reinterpret cast  // Store handle
            );

            FL_DBG("esp_intr_alloc result: " << esp_err_to_name(err) << " (0x" << fl::hex << err << fl::dec << ")");

            if (err != ESP_OK) {
                FL_WARN("Failed to register GPIO interrupt: " << esp_err_to_name(err)
                        << " - interrupt source=" << ETS_GPIO_INTR_SOURCE
                        << " flags=0x" << fl::hex << flags << fl::dec);
                gptimer_disable(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
                gptimer_del_timer(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
                mIsrContext.gptimer_handle = nullptr;
                mcpwm_timer_cleanup();
                return false;
            }

            // Enable GPIO interrupt
            err = gpio_intr_enable(mPin);
            if (err != ESP_OK) {
                FL_WARN("Failed to enable GPIO interrupt: " << esp_err_to_name(err));
                esp_intr_free(reinterpret_cast<intr_handle_t>(mIsrContext.intr_handle)); // ok reinterpret cast
                mIsrContext.intr_handle = nullptr;
                gptimer_disable(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
                gptimer_del_timer(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
                mIsrContext.gptimer_handle = nullptr;
                mcpwm_timer_cleanup();
                return false;
            }

            FL_DBG("GPIO interrupt allocated successfully at Level 5 (Xtensa)");
            #endif

            mInitialized = true;
            FL_DBG("GPIO ISR RX MCPWM initialized successfully");
        }

        // Store configuration parameters
        mIsrContext.skip_signals = config.skip_signals;

        // Convert nanoseconds to MCPWM ticks (80 MHz = 12.5 ns per tick)
        // ticks = nanoseconds / 12.5 = (nanoseconds * 8) / 100 = (nanoseconds * 2) / 25
        mIsrContext.min_pulse_ticks = (config.signal_range_min_ns * 2) / 25;
        mIsrContext.timeout_ticks = (config.signal_range_max_ns * 2) / 25;

        if (mIsrContext.min_pulse_ticks == 0) {
            mIsrContext.min_pulse_ticks = 1;  // Minimum 1 tick
        }

        FL_DBG("GPIO ISR RX MCPWM begin: signal_range_min=" << config.signal_range_min_ns
               << "ns (" << mIsrContext.min_pulse_ticks << " ticks)"
               << ", signal_range_max=" << config.signal_range_max_ns << "ns ("
               << mIsrContext.timeout_ticks << " ticks)"
               << ", skip_signals=" << config.skip_signals);

        // Clear receive state
        mIsrContext.write_index = 0;
        mIsrContext.read_index = 0;
        mIsrContext.output_count = 0;
        mIsrContext.last_edge_timestamp = 0;
        mIsrContext.done = false;

        // Start MCPWM timer
        if (mcpwm_timer_start() != 0) {
            FL_WARN("Failed to start MCPWM timer");
            return false;
        }

        // Start GPTimer
        esp_err_t err = gptimer_start(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
        if (err != ESP_OK) {
            FL_WARN("Failed to start GPTimer: " << static_cast<int>(err));
            mcpwm_timer_stop();
            return false;
        }

        // Arm fast ISR (atomic store with release semantics)
        __atomic_store_n(&mIsrContext.armed, true, __ATOMIC_RELEASE);

        FL_DBG("GPIO ISR RX MCPWM armed and ready");
        return true;
    }

    bool finished() const override {
        // Atomic load with acquire semantics
        return __atomic_load_n(&mIsrContext.done, __ATOMIC_ACQUIRE);
    }

    RxWaitResult wait(u32 timeout_ms) override {
        if (!mInitialized) {
            FL_WARN("wait(): GPIO ISR RX MCPWM not initialized");
            return RxWaitResult::TIMEOUT;
        }

        FL_DBG("wait(): timeout_ms=" << timeout_ms);

        // Convert timeout to microseconds
        i64 timeout_us = static_cast<i64>(timeout_ms) * 1000;
        i64 wait_start_us = esp_timer_get_time();

        // Busy-wait with yield
        while (!finished()) {
            i64 elapsed_us = esp_timer_get_time() - wait_start_us;

            // Check wait timeout
            if (elapsed_us >= timeout_us) {
                FL_WARN("wait(): timeout after " << elapsed_us << "us, captured "
                        << mIsrContext.output_count << " edges");
                return RxWaitResult::TIMEOUT;
            }

            taskYIELD();  // Allow other tasks to run
        }

        // Receive completed
        FL_DBG("wait(): receive done, count=" << mIsrContext.output_count);
        return RxWaitResult::SUCCESS;
    }

    fl::span<const EdgeTimestamp> getEdges() const override {
        if (mOutputBuffer.empty()) {
            return fl::span<const EdgeTimestamp>();
        }

        // Memory barrier: ensure all ISR writes are visible
        __asm__ __volatile__("" ::: "memory");  // Compiler barrier
        #ifdef FL_IS_ESP32_RISCV
        asm volatile("fence" ::: "memory");     // Hardware barrier (RISC-V)
        #endif

        // Convert MCPWM ticks to nanoseconds
        // ns = ticks * 12.5 = (ticks * 25) / 2
        const size_t count = mIsrContext.output_count;
        EdgeEntry* output_ptr = const_cast<EdgeEntry*>(mOutputBuffer.data());
        auto* result_ptr = reinterpret_cast<EdgeTimestamp*>(output_ptr);  // ok reinterpret cast

        for (size_t i = 0; i < count; i++) {
            result_ptr[i].time_ns = (output_ptr[i].timestamp * 25) / 2;
            result_ptr[i].level = output_ptr[i].level;
        }

        return fl::span<const EdgeTimestamp>(result_ptr, count);
    }

    fl::Result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                               fl::span<u8> out) override {
        // Get captured edges
        fl::span<const EdgeTimestamp> edges = getEdges();

        if (edges.empty()) {
            return fl::Result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
        }

        // Use the edge timestamp decoder (ported from gpio_isr_rx.cpp.disabled)
        return decodeEdgeTimestamps(timing, edges, out);
    }

    size_t getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset = 0) override {
        // Get converted edges
        fl::span<const EdgeTimestamp> edges = getEdges();

        if (edges.empty()) {
            return 0;
        }

        size_t out_index = 0;
        size_t edges_emitted = 0;

        // Process edges: Calculate duration between transitions
        for (size_t i = 0; i < edges.size() && out_index < out.size(); ) {
            u8 current_state = edges[i].level;
            u32 state_start_ns = edges[i].time_ns;

            // Find next transition
            size_t next_i = i + 1;
            while (next_i < edges.size() && edges[next_i].level == current_state) {
                next_i++;
            }

            // Calculate duration
            u32 duration_ns;
            if (next_i < edges.size()) {
                duration_ns = edges[next_i].time_ns - state_start_ns;
            } else {
                break;  // Last edge - incomplete duration
            }

            // Skip edges before offset
            if (edges_emitted >= offset) {
                out[out_index] = EdgeTime(current_state == 1, duration_ns);
                out_index++;
            }
            edges_emitted++;

            i = next_i;
        }

        return out_index;
    }

    bool injectEdges(fl::span<const EdgeTime> edges) override {
        // Ensure output buffer is large enough
        if (edges.size() > mOutputBuffer.size()) {
            mOutputBuffer.clear();
            mOutputBuffer.reserve(edges.size());
            for (size_t i = 0; i < edges.size(); i++) {
                mOutputBuffer.push_back({0, 0, {0, 0, 0}});
            }
        }

        // Convert EdgeTime entries to EdgeTimestamp via EdgeEntry
        // EdgeTime has duration (relative), we need to build absolute timestamps
        u32 current_time_ns = 0;
        for (size_t i = 0; i < edges.size(); i++) {
            EdgeEntry& entry = mOutputBuffer[i];
            entry.timestamp = (current_time_ns * 2) / 25; // Convert ns to MCPWM ticks
            entry.level = edges[i].high ? 1 : 0;
            current_time_ns += edges[i].ns;
        }

        mIsrContext.output_count = static_cast<u32>(edges.size());
        mIsrContext.done = true;
        return true;
    }

    const char* name() const override {
        return "ISR_MCPWM";
    }

    int getPin() const override {
        return static_cast<int>(mPin);
    }

private:
    /**
     * @brief Slow management ISR - processes circular buffer and manages state
     *
     * Triggered by GPTimer every 10µs. Reads circular buffer from read_index
     * to write_index, applies filtering, detects timeout, and signals completion.
     *
     * Optimizations:
     * - FL_IRAM placement for zero-wait-state execution
     * - Atomic loads for write_index
     * - Minimal branches in hot path
     */
    FL_IRAM static bool slowManagementISR(gptimer_handle_t timer,
                                          const gptimer_alarm_event_data_t *edata,
                                          void *user_ctx) {
        DualIsrContext* ctx = static_cast<DualIsrContext*>(user_ctx);

        // Fast path: check if already done
        if (__builtin_expect(ctx->done, 0)) {
            return false;  // Don't yield from ISR
        }

        // Load write_index atomically (acquire semantics)
        u32 current_write = __atomic_load_n(&ctx->write_index, __ATOMIC_ACQUIRE);
        u32 read_idx = ctx->read_index;

        // Process circular buffer entries
        while (read_idx != current_write) {
            EdgeEntry edge = ctx->buffer[read_idx];

            // Skip signals filtering
            if (ctx->skip_signals > 0) {
                ctx->skip_signals--;
                read_idx = (read_idx + 1) & ctx->buffer_size_mask;
                ctx->read_index = read_idx;
                continue;
            }

            // Jitter filter: reject pulses < min_pulse_ticks
            if (ctx->last_edge_timestamp != 0) {
                u32 duration = edge.timestamp - ctx->last_edge_timestamp;
                if (duration < ctx->min_pulse_ticks) {
                    read_idx = (read_idx + 1) & ctx->buffer_size_mask;
                    ctx->read_index = read_idx;
                    continue;
                }
            }

            // Accept edge - write to output buffer
            u32 out_count = ctx->output_count;
            if (out_count >= ctx->output_buffer_size) {
                // Output buffer full - signal done
                ctx->done = true;
                __atomic_store_n(&ctx->armed, false, __ATOMIC_RELEASE);
                return false;
            }

            ctx->output_buffer[out_count] = edge;
            ctx->output_count = out_count + 1;
            ctx->last_edge_timestamp = edge.timestamp;

            // Advance read pointer
            read_idx = (read_idx + 1) & ctx->buffer_size_mask;
            ctx->read_index = read_idx;
        }

        // Timeout detection (only if we have captured edges)
        if (ctx->output_count > 0) {
            // Get current MCPWM timer value for timeout check
            // Note: Using direct register read would be faster, but mcpwm_timer_get_value() is safer
            u32 now_timestamp = mcpwm_timer_get_value();
            u32 elapsed_ticks = now_timestamp - ctx->last_edge_timestamp;

            if (elapsed_ticks >= ctx->timeout_ticks) {
                // Timeout elapsed - signal done
                ctx->done = true;
                __atomic_store_n(&ctx->armed, false, __ATOMIC_RELEASE);
            }
        }

        return false;  // Don't yield from ISR
    }

    /**
     * @brief Clean up resources
     */
    void cleanup() {
        if (!mInitialized) {
            return;
        }

        // Disarm fast ISR
        __atomic_store_n(&mIsrContext.armed, false, __ATOMIC_RELEASE);

        // Platform-specific GPIO interrupt cleanup
        #ifdef __riscv
        // RISC-V: Free interrupt handle and disable GPIO interrupt
        if (mInterruptHandle != nullptr) {
            gpio_intr_disable(mPin);
            esp_intr_free(mInterruptHandle);
            mInterruptHandle = nullptr;
        }
        #else
        // Xtensa: Free GPIO interrupt handle and disable interrupt
        if (mIsrContext.intr_handle != nullptr) {
            gpio_intr_disable(mPin);
            esp_intr_free(reinterpret_cast<intr_handle_t>(mIsrContext.intr_handle)); // ok reinterpret cast
            mIsrContext.intr_handle = nullptr;
        }
        #endif

        // Stop GPTimer
        if (mIsrContext.gptimer_handle != nullptr) {
            gptimer_stop(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
            gptimer_disable(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
            gptimer_del_timer(reinterpret_cast<gptimer_handle_t>(mIsrContext.gptimer_handle)); // ok reinterpret cast
            mIsrContext.gptimer_handle = nullptr;
        }

        // Stop MCPWM timer
        mcpwm_timer_stop();
        mcpwm_timer_cleanup();

        mInitialized = false;
        FL_DBG("GPIO ISR RX MCPWM cleaned up");
    }

    gpio_num_t mPin;
    size_t mBufferSize;
    size_t mOutputBufferSize;
    fl::vector<EdgeEntry> mEdgeBuffer;     ///< Circular buffer
    fl::vector<EdgeEntry> mOutputBuffer;   ///< Filtered output buffer
    DualIsrContext mIsrContext;
    bool mInitialized;
#ifdef __riscv
    intr_handle_t mInterruptHandle = nullptr;  ///< ESP-IDF interrupt handle for cleanup
#endif
};

// Factory method for MCPWM-based implementation
// Note: This is a separate factory - doesn't override the existing GpioIsrRx::create()
fl::shared_ptr<GpioIsrRx> GpioIsrRxMcpwm_create(int pin) {
    return fl::make_shared<GpioIsrRxMcpwm>(pin);
}

} // namespace fl

#endif // FASTLED_RMT5
#endif // FL_IS_ESP32

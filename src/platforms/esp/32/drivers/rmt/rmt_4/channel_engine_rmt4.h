/// @file channel_engine_rmt4.h
/// @brief RMT4 interface for ChannelEngine (ESP32 IDF 4.x)
///
/// This header defines both the public interface and implementation for RMT4 ChannelEngine.
/// IRAM_ATTR functions are inlined here to avoid duplicate attribute warnings.

#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

// Skip entirely if platform has no RMT hardware (ESP32-C2)
#if !FASTLED_ESP32_HAS_RMT
// No RMT hardware available
#elif FASTLED_ESP32_RMT5_ONLY_PLATFORM

// Detect misconfiguration: RMT5-only chip with FASTLED_RMT5=0
#if FASTLED_RMT5 == 0
#error "ESP32-C6/C5/P4/H2 only support RMT5 (new driver). These chips do not have RMT4 hardware. Remove -DFASTLED_RMT5=0 from build flags or set FASTLED_RMT5=1."
#endif

// Skip RMT4 implementation for RMT5-only chips
#elif !FASTLED_RMT5  // Only compile for RMT4 (IDF 4.x)

#include "fl/channels/engine.h"
#include "fl/channels/data.h"
#include "fl/stl/span.h"
#include "fl/register.h"
#include "platforms/esp/32/core/clock_cycles.h"
#include "fl/stl/vector.h"

FL_EXTERN_C_BEGIN
#include "esp32-hal.h"
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR > 3
#include "esp_intr_alloc.h"
#else
#include "esp_intr.h"
#endif
#include "driver/gpio.h"
#include "driver/rmt.h"
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
#include "esp_private/periph_ctrl.h"
#include "soc/gpio_periph.h"
#define gpio_matrix_out esp_rom_gpio_connect_out_signal
#else
#include "driver/periph_ctrl.h"
#endif
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "soc/rmt_struct.h"

// Flash lock support (optional feature)
#if FASTLED_ESP32_FLASH_LOCK == 1
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 0, 0)
#include "esp_flash.h"
#else
#include "esp_spi_flash.h"
#endif
#endif

FL_EXTERN_C_END

// ═══════════════════════════════════════════════════════════════════════════
// Platform-specific RMT memory configuration
// ═══════════════════════════════════════════════════════════════════════════

// 64 for ESP32, ESP32-S2
// 48 for ESP32-S3, ESP32-C3, ESP32-H2, ESP32-C6
#ifndef FASTLED_RMT_MEM_WORDS_PER_CHANNEL
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL SOC_RMT_MEM_WORDS_PER_CHANNEL
#else
// ESP32 value (only chip variant supported on older IDF)
#define FASTLED_RMT_MEM_WORDS_PER_CHANNEL 64
#endif
#endif

// By default use two memory blocks for each RMT channel instead of 1.
#ifndef FASTLED_RMT_MEM_BLOCKS
#define FASTLED_RMT_MEM_BLOCKS 2
#endif

#define MAX_PULSES_RMT4 (FASTLED_RMT_MEM_WORDS_PER_CHANNEL * FASTLED_RMT_MEM_BLOCKS)
#define PULSES_PER_FILL_RMT4 (MAX_PULSES_RMT4 / 2)

// Clock divider
#define DIVIDER_RMT4 2

// Default transmission timeout (milliseconds)
// Set to 0 to disable timeout detection
#ifndef FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS
#define FASTLED_RMT4_TRANSMISSION_TIMEOUT_MS 2000
#endif

// Flash lock configuration (default: disabled)
#ifndef FASTLED_ESP32_FLASH_LOCK
#define FASTLED_ESP32_FLASH_LOCK 0
#endif

// Max RMT TX channels (platform-specific)
#ifndef FASTLED_RMT_MAX_CHANNELS
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#define FASTLED_RMT_MAX_CHANNELS SOC_RMT_TX_CANDIDATES_PER_GROUP
#else
#ifdef CONFIG_IDF_TARGET_ESP32S2
#define FASTLED_RMT_MAX_CHANNELS 4
#else
#define FASTLED_RMT_MAX_CHANNELS 8
#endif
#endif
#endif

// ═══════════════════════════════════════════════════════════════════════════
// Timing Conversion Macros
// ═══════════════════════════════════════════════════════════════════════════

// Manual clock frequency override for ESP32-C6 and ESP32-H2 (40MHz vs 80MHz)
#ifndef F_CPU_RMT_CLOCK_MANUALLY_DEFINED
#if defined(CONFIG_IDF_TARGET_ESP32C6) && CONFIG_IDF_TARGET_ESP32C6 == 1
#define F_CPU_RMT_CLOCK_MANUALLY_DEFINED (80 * 1000000)
#elif defined(CONFIG_IDF_TARGET_ESP32H2) && CONFIG_IDF_TARGET_ESP32H2 == 1
#define F_CPU_RMT_CLOCK_MANUALLY_DEFINED (80 * 1000000)
#endif

#ifdef F_CPU_RMT_CLOCK_MANUALLY_DEFINED
#define F_CPU_RMT (F_CPU_RMT_CLOCK_MANUALLY_DEFINED)
#else
#define F_CPU_RMT (APB_CLK_FREQ)
#endif
#endif

// Convert nanoseconds to ESP32 CPU cycles
#define NS_TO_ESP_CYCLES(ns) (((ns) * (F_CPU / 1000000UL) + 500) / 1000)

// Convert ESP32 CPU cycles to RMT device cycles (accounting for divider)
#define RMT_CYCLES_PER_SEC (F_CPU_RMT / DIVIDER_RMT4)
#define RMT_CYCLES_PER_ESP_CYCLE (F_CPU / RMT_CYCLES_PER_SEC)
#define ESP_TO_RMT_CYCLES(n) ((n) / (RMT_CYCLES_PER_ESP_CYCLE))

namespace fl {

// Forward declarations
class ChannelEngineRMT4;
FASTLED_SHARED_PTR(ChannelEngineRMT4);

/// @brief RMT4-based ChannelEngine interface
///
/// Factory-based interface for RMT4 hardware driver.
/// Use create() to instantiate the concrete implementation.
///
/// Key Features:
/// - Double-buffer ISR-driven refill (WiFi interference resistant)
/// - Time-multiplexing support (>8 strips via channel sharing)
/// - Direct hardware memory access for performance
/// - Zero global state (everything encapsulated in engine)
class ChannelEngineRMT4 : public IChannelEngine {
public:
    /// @brief Create RMT4 engine instance
    /// @return Shared pointer to engine implementation
    static ChannelEngineRMT4Ptr create();

    virtual ~ChannelEngineRMT4() = default;

    /// @brief Get the engine name for affinity binding
    /// @return "RMT"
    const char* getName() const override { return "RMT"; }

protected:
    ChannelEngineRMT4() = default;
};

// ═══════════════════════════════════════════════════════════════════════════
// Concrete Implementation Class
// ═══════════════════════════════════════════════════════════════════════════

/// @brief Concrete RMT4 engine implementation
///
/// This class contains all implementation details including:
/// - Channel state tracking
/// - ISR handlers (IRAM attributes)
/// - Double-buffer management
/// - Time-multiplexing queue
class ChannelEngineRMT4Impl final : public ChannelEngineRMT4 {
public:
    ChannelEngineRMT4Impl();
    ~ChannelEngineRMT4Impl() override;

    /// @brief Enqueue channel data for transmission
    void enqueue(ChannelDataPtr channelData) override;

    /// @brief Trigger transmission of enqueued data
    void show() override;

    /// @brief Query engine state (hardware polling implementation)
    EngineState poll() override;

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // Channel State Structure
    // ═══════════════════════════════════════════════════════════════════════════
    struct ChannelState {
        // Hardware Configuration
        rmt_channel_t channel;
        gpio_num_t pin;
        bool inUse;

        // Timing Symbols
        rmt_item32_t zero;
        rmt_item32_t one;

        // Transmission State
        volatile bool transmissionComplete;

        // Double-Buffer State
        int whichHalf;
        volatile rmt_item32_t* memPtr;
        volatile rmt_item32_t* memStart;

        // Pixel Data Buffer
        const uint8_t* pixelData;
        size_t pixelDataSize;
        volatile size_t pixelDataPos;

        // Performance Monitoring
        uint32_t cyclesPerFill;
        uint32_t maxCyclesPerFill;
        volatile uint32_t lastFill;

        // Timeout Detection
        uint32_t transmissionStartTime;  // millis() when transmission started

        // Source reference
        ChannelDataPtr sourceData;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // Member Variables
    // ═══════════════════════════════════════════════════════════════════════════

    fl::vector_inlined<ChannelState, FASTLED_RMT_MAX_CHANNELS> mChannels;
    fl::vector_inlined<ChannelDataPtr, 16> mEnqueuedChannels;  // Batched between enqueue() and show()
    fl::vector_inlined<ChannelDataPtr, 16> mPendingChannels;    // Awaiting HW channels
    intr_handle_t mRMT_intr_handle;
    portMUX_TYPE mRmtSpinlock;
    bool mInitialized;

    // ═══════════════════════════════════════════════════════════════════════════
    // ISR Handlers (IRAM) - INLINED TO AVOID DUPLICATE IRAM_ATTR
    // ═══════════════════════════════════════════════════════════════════════════

    static inline IRAM_ATTR void handleInterrupt(void* arg) {
        // Main ISR dispatcher
        auto* engine = static_cast<ChannelEngineRMT4Impl*>(arg);

        // Enter critical section and read interrupt status
        portENTER_CRITICAL_ISR(&engine->mRmtSpinlock);
        uint32_t intr_st = RMT.int_st.val;
        portEXIT_CRITICAL_ISR(&engine->mRmtSpinlock);

        // Check each channel for interrupts
        for (uint8_t channel = 0; channel < FASTLED_RMT_MAX_CHANNELS; channel++) {
#if CONFIG_IDF_TARGET_ESP32S2
            int tx_done_bit = channel * 3;
            int tx_next_bit = channel + 12;
#elif CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3
            int tx_done_bit = channel;
            int tx_next_bit = channel + 8;
#elif CONFIG_IDF_TARGET_ESP32H2
            int tx_done_bit = channel;
            int tx_next_bit = channel + 8;
#elif CONFIG_IDF_TARGET_ESP32C6
            int tx_done_bit = channel;
            int tx_next_bit = channel + 8;
#elif CONFIG_IDF_TARGET_ESP32
            int tx_done_bit = channel * 3;
            int tx_next_bit = channel + 24;
#else
#error "Unknown ESP32 target for RMT interrupt bit positions"
#endif

            // Check if this channel is active
            ChannelState* state = engine->findChannelByNumber(channel);
            if (state != nullptr && state->inUse) {
                // Handle threshold interrupt (half-buffer empty, needs refill)
                if (intr_st & BIT(tx_next_bit)) {
                    engine->onThresholdInterrupt(channel);
                    RMT.int_clr.val = BIT(tx_next_bit);
                }

                // Handle TX done interrupt (transmission complete)
                if (intr_st & BIT(tx_done_bit)) {
                    engine->onTxDoneInterrupt(channel);
                    RMT.int_clr.val = BIT(tx_done_bit);
                }
            }
        }
    }

    inline IRAM_ATTR void onThresholdInterrupt(int channelNum) {
        // Threshold interrupt: Half of the RMT buffer has been transmitted
        ChannelState* state = findChannelByNumber(channelNum);
        if (state == nullptr || !state->inUse) {
            return;
        }

        // Fill the next buffer half (with WiFi interference check enabled)
        fillNextBuffer(state, true);
    }

    inline IRAM_ATTR void onTxDoneInterrupt(int channelNum) {
        // TX done interrupt: Transmission is complete on this channel
        ChannelState* state = findChannelByNumber(channelNum);
        if (state == nullptr || !state->inUse) {
            return;
        }

        // Disconnect the GPIO from the RMT controller
        gpio_matrix_out(state->pin, SIG_GPIO_OUT_IDX, 0, 0);

        // Disable TX interrupts for this channel (platform-specific)
#if CONFIG_IDF_TARGET_ESP32C3
        RMT.int_ena.val &= ~(1 << channelNum);
#elif CONFIG_IDF_TARGET_ESP32H2
        RMT.int_ena.val &= ~(1 << channelNum);
#elif CONFIG_IDF_TARGET_ESP32S3
        RMT.int_ena.val &= ~(1 << channelNum);
#elif CONFIG_IDF_TARGET_ESP32C6
        RMT.int_ena.val &= ~(1 << channelNum);
#elif CONFIG_IDF_TARGET_ESP32S2
        RMT.int_ena.val &= ~(1 << (channelNum * 3));
#elif CONFIG_IDF_TARGET_ESP32
        RMT.int_ena.val &= ~(1 << (channelNum * 3));
#else
#error "Unknown ESP32 target for RMT interrupt disable"
#endif

        // Mark transmission as complete (checked by pollDerived())
        state->transmissionComplete = true;
    }

    inline IRAM_ATTR void fillNextBuffer(ChannelState* state, bool checkTime) {
        // Fill the next half of the RMT double-buffer with pixel data
        if (!state) {
            return;
        }

        // WiFi interference detection: Measure time between buffer refills
        uint32_t now = __clock_cycles();
        if (checkTime && state->lastFill != 0) {
            int32_t delta = static_cast<int32_t>(now - state->lastFill);
            if (delta > static_cast<int32_t>(state->maxCyclesPerFill)) {
                // Too much time elapsed - WiFi interrupt interference detected
                state->pixelDataPos = state->pixelDataSize;
            }
        }
        state->lastFill = now;

        // Get timing symbols into local registers (fast access)
        FASTLED_REGISTER uint32_t one_val = state->one.val;
        FASTLED_REGISTER uint32_t zero_val = state->zero.val;

        // Calculate pointer to current buffer half
        volatile FASTLED_REGISTER rmt_item32_t* pItem = state->memPtr;

        // Fill one half of the buffer
        for (FASTLED_REGISTER int i = 0; i < PULSES_PER_FILL_RMT4 / 8; i++) {
            if (state->pixelDataPos < state->pixelDataSize) {
                // Convert next byte to 8 RMT symbols
                uint8_t byteval = state->pixelData[state->pixelDataPos];
                convertByteToRMT(byteval, state->zero, state->one, pItem);
                pItem += 8;
                state->pixelDataPos++;
            } else {
                // No more data - fill rest of buffer with zeros
                pItem->val = 0;
                pItem++;
            }
        }

        // Toggle to the other buffer half for next refill
        state->whichHalf++;
        if (state->whichHalf == 2) {
            pItem = state->memStart;
            state->whichHalf = 0;
        }

        // Update pointer for next refill
        state->memPtr = pItem;
    }

    static inline IRAM_ATTR void convertByteToRMT(
        uint8_t byteval,
        const rmt_item32_t& zero,
        const rmt_item32_t& one,
        volatile rmt_item32_t* pItem
    ) {
        // Convert 1 byte → 8 RMT symbols (MSB first)
        uint32_t pixel_u32 = byteval;
        pixel_u32 <<= 24;  // Shift to MSB position for bit extraction

        uint32_t tmp[8];
        for (uint32_t j = 0; j < 8; j++) {
            // Extract MSB and select symbol
            uint32_t new_val = (pixel_u32 & 0x80000000L) ? one.val : zero.val;
            pixel_u32 <<= 1;

            // Write to non-volatile buffer (compiler can optimize)
            tmp[j] = new_val;
        }

        // Copy to volatile hardware memory (unrolled for performance)
        pItem[0].val = tmp[0];
        pItem[1].val = tmp[1];
        pItem[2].val = tmp[2];
        pItem[3].val = tmp[3];
        pItem[4].val = tmp[4];
        pItem[5].val = tmp[5];
        pItem[6].val = tmp[6];
        pItem[7].val = tmp[7];
    }

    static inline IRAM_ATTR void tx_start(ChannelState* state) {
        // Start RMT transmission by setting hardware flag
        if (!state) {
            return;
        }

        rmt_channel_t channel = state->channel;

        // Platform-specific register access
#if CONFIG_IDF_TARGET_ESP32C3
        RMT.tx_conf[channel].mem_rd_rst = 1;
        RMT.tx_conf[channel].mem_rd_rst = 0;
        RMT.tx_conf[channel].mem_rst = 1;
        RMT.tx_conf[channel].mem_rst = 0;
        RMT.int_clr.val = (1 << channel);
        RMT.int_ena.val |= (1 << channel);
        RMT.tx_conf[channel].conf_update = 1;
        RMT.tx_conf[channel].tx_start = 1;

#elif CONFIG_IDF_TARGET_ESP32H2
        RMT.chnconf0[channel].mem_rd_rst_chn = 1;
        RMT.chnconf0[channel].mem_rd_rst_chn = 0;
        RMT.chnconf0[channel].apb_mem_rst_chn = 1;
        RMT.chnconf0[channel].apb_mem_rst_chn = 0;
        RMT.int_clr.val = (1 << channel);
        RMT.int_ena.val |= (1 << channel);
        RMT.chnconf0[channel].conf_update_chn = 1;
        RMT.chnconf0[channel].tx_start_chn = 1;

#elif CONFIG_IDF_TARGET_ESP32S3
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        RMT.chnconf0[channel].mem_rd_rst_chn = 1;
        RMT.chnconf0[channel].mem_rd_rst_chn = 0;
        RMT.chnconf0[channel].apb_mem_rst_chn = 1;
        RMT.chnconf0[channel].apb_mem_rst_chn = 0;
        RMT.int_clr.val = (1 << channel);
        RMT.int_ena.val |= (1 << channel);
        RMT.chnconf0[channel].conf_update_chn = 1;
        RMT.chnconf0[channel].tx_start_chn = 1;
#else
        RMT.chnconf0[channel].mem_rd_rst_n = 1;
        RMT.chnconf0[channel].mem_rd_rst_n = 0;
        RMT.chnconf0[channel].apb_mem_rst_n = 1;
        RMT.chnconf0[channel].apb_mem_rst_n = 0;
        RMT.int_clr.val = (1 << channel);
        RMT.int_ena.val |= (1 << channel);
        RMT.chnconf0[channel].conf_update_n = 1;
        RMT.chnconf0[channel].tx_start_n = 1;
#endif

#elif CONFIG_IDF_TARGET_ESP32C6
        RMT.chnconf0[channel].mem_rd_rst_chn = 1;
        RMT.chnconf0[channel].mem_rd_rst_chn = 0;
        RMT.chnconf0[channel].apb_mem_rst_chn = 1;
        RMT.chnconf0[channel].apb_mem_rst_chn = 0;
        RMT.int_clr.val = (1 << channel);
        RMT.int_ena.val |= (1 << channel);
        RMT.chnconf0[channel].conf_update_chn = 1;
        RMT.chnconf0[channel].tx_start_chn = 1;

#elif CONFIG_IDF_TARGET_ESP32S2
        RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
        RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
        RMT.conf_ch[channel].conf1.apb_mem_rst = 1;
        RMT.conf_ch[channel].conf1.apb_mem_rst = 0;
        RMT.int_clr.val = (1 << (channel * 3));
        RMT.int_ena.val |= (1 << (channel * 3));
        RMT.conf_ch[channel].conf1.tx_start = 1;

#else
        // ESP32 (original)
        RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
        RMT.conf_ch[channel].conf1.mem_rd_rst = 0;
        RMT.conf_ch[channel].conf1.apb_mem_rst = 1;
        RMT.conf_ch[channel].conf1.apb_mem_rst = 0;
        RMT.int_clr.val = (1 << (channel * 3));
        RMT.int_ena.val |= (1 << (channel * 3));
        RMT.conf_ch[channel].conf1.tx_start = 1;
#endif
    }

    inline IRAM_ATTR ChannelState* findChannelByNumber(int channelNum) {
        // Linear search through active channels
        for (auto& state : mChannels) {
            if (state.inUse && state.channel == channelNum) {
                return &state;
            }
        }
        return nullptr;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Channel Management
    // ═══════════════════════════════════════════════════════════════════════════

    /// @brief Begin LED data transmission for all channels (internal helper)
    void beginTransmission(fl::span<const ChannelDataPtr> channelData);

    ChannelState* acquireChannel(gpio_num_t pin, const ChipsetTimingConfig& timing);
    void releaseChannel(ChannelState* state);
    bool configureChannel(ChannelState* state, gpio_num_t pin, const ChipsetTimingConfig& timing);
    void processPendingChannels();
    void startTransmission(ChannelState* state, const ChannelDataPtr& data);

    // ═══════════════════════════════════════════════════════════════════════════
    // Helpers
    // ═══════════════════════════════════════════════════════════════════════════

    static rmt_item32_t makeZeroSymbol(const ChipsetTimingConfig& timing);
    static rmt_item32_t makeOneSymbol(const ChipsetTimingConfig& timing);
};

} // namespace fl

#endif // !FASTLED_RMT5 and not RMT5-only chip

#endif // ESP32

#ifdef ESP32

#include "sdkconfig.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker.h"
#include "rmt5_worker_lut.h"
#include "rmt5_device.h"

FL_EXTERN_C_BEGIN

#include "esp32-hal.h"
#include "esp_intr_alloc.h"
#include "driver/gpio.h"
#include "soc/soc.h"
#include "soc/interrupts.h"  // For ETS_RMT_INTR_SOURCE
#include "soc/rmt_struct.h"
#include "rom/gpio.h"  // For gpio_matrix_out
#include "rom/ets_sys.h"  // For ets_printf (ISR-safe logging)
#include "esp_log.h"

FL_EXTERN_C_END

#include "fl/force_inline.h"
#include "ftl/assert.h"
#include "fl/cstring.h"
#include "fl/compiler_control.h"
#include "fl/log.h"
#include "fl/register.h"
#include "fl/slice.h"
#include "esp_debug_helpers.h"  // For esp_backtrace_print()

#define RMT5_WORKER_TAG "rmt5_worker"

// RMT interrupt handling - Always use direct ISR
// The ESP-IDF v5.x RMT driver does NOT provide a threshold callback in rmt_tx_event_callbacks_t,
// only on_trans_done. Since we need threshold interrupts for ping-pong buffer refill,
// we must use direct ISR with manual register access (no alternative exists).

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT_SIG_PAD_IDX RMT_SIG_PAD_OUT0_IDX
#else
#define RMT_SIG_PAD_IDX RMT_SIG_OUT0_IDX
#endif

// Define rmt_block_mem_t for IDF5 (removed from public headers)
// Note: rmt_item32_t is defined in rmt5_worker_lut.h within fl:: namespace
typedef struct {
    struct {
        fl::rmt_item32_t data32[SOC_RMT_MEM_WORDS_PER_CHANNEL];
    } chan[SOC_RMT_CHANNELS_PER_GROUP];
} rmt_block_mem_t;

// RMTMEM address is declared in <target>.peripherals.ld
extern rmt_block_mem_t RMTMEM;

//=============================================================================
// SHARED GLOBAL ISR INFRASTRUCTURE (like RMT4)
//=============================================================================

/*
 * Global shared interrupt handler architecture
 *
 * DESIGN: Like RMT4, we use a SINGLE shared ISR for ALL RMT channels.
 * This prevents race conditions and missed interrupts when multiple channels
 * fire simultaneously. The shared ISR reads RMT.int_st.val once and processes
 * all pending channel interrupts in a single pass.
 *
 * THREAD SAFETY: The gOnChannel[] array is written during worker allocation
 * (before transmission starts) and read in the ISR. Workers are assigned to
 * channels before interrupts are enabled, so no race conditions exist.
 */

// Note: Global worker registry, interrupt handle, and ISR moved to RmtWorkerIsrMgr

/**
 * NMI-Safe Buffer Refill Wrapper
 *
 * This function provides a Level 7 NMI-safe entry point for RMT buffer
 * refill operations. It MUST NOT use any FreeRTOS APIs.
 *
 * REQUIREMENTS:
 * - Marked IRAM_ATTR (code in IRAM, not flash)
 * - Marked extern "C" (for ASM_2_C_SHIM.h linkage)
 * - No FreeRTOS API calls (no portENTER_CRITICAL_ISR, no xSemaphore*, etc.)
 * - Fast execution (<500ns target for WS2812 timing)
 *
 *
 *   // Install Level 7 NMI
 *   esp_intr_alloc(ETS_RMT_INTR_SOURCE,
 *                  ESP_INTR_FLAG_LEVEL7 | ESP_INTR_FLAG_IRAM,
 *                  nullptr, nullptr, &handle);
 *
 *
 * TIMING:
 * - Assembly shim overhead: ~65ns (register save/restore)
 * - fillNextHalf() execution: ~500ns (120 instructions at 240 MHz)
 
 */


//=============================================================================

namespace fl {

RmtWorker::RmtWorker()
    : mChannel(nullptr)
    , mWorkerId(0)
    , mChannelId(0xFF)
    , mCurrentPin(GPIO_NUM_NC)
    , mTiming{0, 0, 0, 0, nullptr}  // ChipsetTiming initialization
    , mAvailable(true)
    , mHandleResult(Result<RmtIsrHandle, RmtRegisterError>::failure(
        RmtRegisterError::INVALID_CHANNEL,
        "Not yet registered"
    ))
{
    // ISR handle will be acquired from manager during transmission
    // Workers start in available state
}

RmtWorker::~RmtWorker() {
    // Unregister from ISR manager (handles both ISR data and interrupt deallocation)
    if (mHandleResult.ok()) {
        RmtWorkerIsrMgr::getInstance().stopTransmission(mHandleResult.value());
    }

    // Clean up channel
    if (mChannel) {
        rmt_disable(mChannel);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
    }
}

bool RmtWorker::initialize(uint8_t worker_id) {
    mWorkerId = worker_id;
    // FIXED: Availability tracked by volatile boolean (already initialized to true in constructor)

    // Channel creation is deferred to configure() where we know the actual GPIO pin.
    // This avoids needing placeholder GPIOs and is safe for static initialization.
    FL_LOG_RMT("RmtWorker[" << (int)worker_id << "]: Initialized (channel creation deferred to first configure)");

    return true;
}



bool RmtWorker::createRMTChannel(gpio_num_t pin) {
    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Creating RMT TX channel for GPIO " << (int)pin);

    // Flush logs before potentially failing operation
    esp_log_level_set("*", ESP_LOG_VERBOSE);

    // Create RMT TX channel with 2 memory blocks
    rmt_tx_channel_config_t tx_config = {};
    tx_config.gpio_num = pin;
    tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_config.resolution_hz = FASTLED_RMT5_CLOCK_HZ;
    tx_config.mem_block_symbols = 2 * FASTLED_RMT_MEM_WORDS_PER_CHANNEL;  // 2 blocks for ping-pong
    tx_config.trans_queue_depth = 1;
    tx_config.flags.invert_out = false;
    tx_config.flags.with_dma = false;  // No DMA

    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: About to call rmt_new_tx_channel for GPIO " << (int)pin);
    esp_err_t ret = rmt_new_tx_channel(&tx_config, &mChannel);
    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: rmt_new_tx_channel returned: " << esp_err_to_name(ret) << " (0x" << ret << ")");

    if (ret != ESP_OK) {
        FL_WARN("RmtWorker[" << (int)mWorkerId << "]: Failed to create RMT TX channel: " << esp_err_to_name(ret) << " (0x" << ret << ")");
        return false;
    }

    // Extract channel ID from handle (relies on internal IDF structure)
    mChannelId = getChannelIdFromHandle(mChannel);
    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Created channel_id=" << (unsigned)mChannelId);

    // Note: RMT memory pointers will be initialized during first transmit() when ISR data is registered

    // Configure threshold interrupt for ping-pong buffer refill
    // Threshold = half of total buffer size, triggering refill when first half is transmitted
    // With 2 blocks × 64 words = 128 total words, threshold = 64 words (PULSES_PER_FILL)
    // However, hardware requires threshold in bytes: 64 words × 4 bytes/word = 256 bytes
    // But register expects word count, not byte count, so we use 48 (empirically determined)
    // TODO: Investigate exact threshold calculation - may need platform-specific tuning
    constexpr uint32_t RMT_THRESHOLD_LIMIT = PULSES_PER_FILL;  // 48 words for ping-pong refill

    // Set threshold limit using RMT device macro
    RMT5_SET_THRESHOLD_LIMIT(mChannelId, RMT_THRESHOLD_LIMIT);

    // NOTE: Threshold interrupt setup moved to allocateInterrupt() for lazy initialization
    // This prevents interrupt watchdog timeout on ESP32-C6 during early boot

    // NOTE: Interrupt allocation deferred to first transmit() call
    // This prevents interrupt watchdog timeout on ESP32-C6 (RISC-V) during early boot


    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Channel created successfully");
    return true;
}

void RmtWorker::tearDownRMTChannel(gpio_num_t old_pin) {
    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Tearing down RMT channel (old pin=" << (int)old_pin << ")");

    // Unregister from ISR manager (handles both ISR data and interrupt deallocation)
    if (mHandleResult.ok()) {
        RmtWorkerIsrMgr::getInstance().stopTransmission(mHandleResult.value());
    }

    // Disable and delete the RMT channel
    if (mChannel) {
        rmt_disable(mChannel);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
    }

    // Clean up old GPIO pin: set to input with pulldown
    if (old_pin != GPIO_NUM_NC) {
        // Disconnect GPIO from RMT controller BEFORE reconfiguration
        // This prevents the RMT from driving the old pin during pin changes
        gpio_matrix_out(old_pin, SIG_GPIO_OUT_IDX, 0, 0);
        FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Disconnected GPIO " << (int)old_pin << " from RMT");
        gpio_set_direction(old_pin, GPIO_MODE_INPUT);
        gpio_pulldown_en(old_pin);
        gpio_pullup_dis(old_pin);
        FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Old pin " << (int)old_pin << " set to input pulldown");
    }
}

bool RmtWorker::configure(gpio_num_t pin, const ChipsetTiming& timing) {
    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: configure() called - pin=" << (int)pin
               << ", t1=" << timing.T1 << ", t2=" << timing.T2 << ", t3=" << timing.T3 << ", reset_us=" << timing.RESET);

    // Channel creation deferred to first transmit() call
    // This prevents ESP32-C6 (RISC-V) boot hang during RMT hardware initialization

    // Check if reconfiguration needed (compare entire timing struct and pin)
    if (mCurrentPin == pin &&
        mTiming.T1 == timing.T1 &&
        mTiming.T2 == timing.T2 &&
        mTiming.T3 == timing.T3 &&
        mTiming.RESET == timing.RESET) {
        FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Already configured with same parameters - skipping");
        return true;  // Already configured
    }

    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Reconfiguration needed (previous pin=" << (int)mCurrentPin << ")");

    // Wait for active transmission
    if (!isAvailable()) {
        FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Waiting for active transmission to complete");
        waitForCompletion();
    }

    // Save old pin before updating (needed for cleanup and channel recreation check)
    gpio_num_t old_pin = mCurrentPin;

    // Detect pin change - requires channel teardown and recreation
    bool pin_changed = (old_pin != GPIO_NUM_NC && old_pin != pin);

    if (pin_changed && mChannel != nullptr) {
        FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Pin changed from " << (int)old_pin
                   << " to " << (int)pin << " - calling tearDownRMTChannel()");
        tearDownRMTChannel(old_pin);
    }

    // Update configuration (direct struct copy)
    mCurrentPin = pin;
    mTiming = timing;

    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Timing configured: T1=" << timing.T1
               << "ns, T2=" << timing.T2 << "ns, T3=" << timing.T3 << "ns");

    // Note: Timing conversion (ns to ticks) and LUT building happens in ISR manager
    // GPIO configuration deferred to first transmit() when channel exists

    return true;
}

void RmtWorker::transmit(const uint8_t* pixel_data, int num_bytes) {
    FL_ASSERT(isAvailable(), "RmtWorker::transmit called while already transmitting");
    FL_ASSERT(pixel_data != nullptr, "RmtWorker::transmit called with null pixel data");

    // Safety check in case FL_ASSERT is compiled out
    if (pixel_data == nullptr || !isAvailable()) {
        FL_WARN("Worker[" << (int)mWorkerId << "]: Invalid transmit state - pixel_data=" << (void*)pixel_data
                << ", available=" << isAvailable());
        return;
    }

    // Create RMT channel on first transmit (lazy initialization)
    // This prevents ESP32-C6 (RISC-V) boot hang during hardware initialization
    if (mChannel == nullptr) {
        if (!createRMTChannel(mCurrentPin)) {
            FL_WARN("Worker[" << (int)mWorkerId << "]: Failed to create channel - aborting transmit");
            return;
        }

        // Configure GPIO and enable channel (only on first creation)
        FL_LOG_RMT("Worker[" << (int)mWorkerId << "]: Configuring GPIO " << (int)mCurrentPin << " for RMT output");
        gpio_set_direction(mCurrentPin, GPIO_MODE_OUTPUT);


        int signal_idx = RMT_SIG_PAD_IDX + mChannelId;
        gpio_matrix_out(mCurrentPin, signal_idx, false, false);

        esp_err_t ret = rmt_enable(mChannel);
        if (ret != ESP_OK) {
            FL_WARN("Worker[" << (int)mWorkerId << "]: Failed to enable channel: " << esp_err_to_name(ret));
            return;
        }
        FL_LOG_RMT("Worker[" << (int)mWorkerId << "]: Channel enabled and ready");
    }

    // Configure ISR data right before transmission
    // Note: Interrupt allocation happens automatically in startTransmission()

    // Get RMT memory pointer for this channel
    uint8_t channel_id = getChannelIdFromHandle(mChannel);
    volatile rmt_item32_t* rmt_mem_start =
        reinterpret_cast<volatile rmt_item32_t*>(&RMTMEM.chan[channel_id].data32[0]);

    // Create spans for RMT memory buffer and pixel data
    fl::span<volatile rmt_item32_t> rmt_mem(rmt_mem_start, MAX_PULSES);
    fl::span<const uint8_t> pixel_data_span(pixel_data, num_bytes);

    // Register with ISR manager and start transmission
    // The manager will build the LUT, configure ISR data, fill buffers, and start hardware
    // Pass pointer to our availability flag so ISR can signal completion
    mHandleResult = RmtWorkerIsrMgr::getInstance().startTransmission(
        channel_id,
        &mAvailable,
        rmt_mem,
        pixel_data_span,
        mTiming
    );
    if (!mHandleResult.ok()) {
        FL_WARN("Worker[" << (int)mWorkerId << "]: Failed to register with ISR manager: "
                << (int)mHandleResult.error() << " - aborting transmit");
        return;
    }

    // Debug: Log transmission start (transmission now starts automatically in startTransmission)
    FL_LOG_RMT("Worker[" << (int)mWorkerId << "]: TX START - " << num_bytes << " bytes (" << (num_bytes / 3) << " LEDs)");
}

void RmtWorker::waitForCompletion() {
    // FIXED: No semaphore needed - engine polls isAvailable() instead
    // Spin-wait until ISR sets mAvailable = true
    while (!isAvailable()) {
        // Yield to other tasks while waiting
        taskYIELD();
    }
}

void RmtWorker::markAsAvailable() {
    // Unregister from ISR manager if we have a valid handle
    if (mHandleResult.ok()) {
        RmtWorkerIsrMgr::getInstance().stopTransmission(mHandleResult.value());
        // Reset to failure state
        mHandleResult = Result<RmtIsrHandle, RmtRegisterError>::failure(
            RmtRegisterError::INVALID_CHANNEL,
            "Channel unregistered"
        );
    }

    // Set availability flag to true
    mAvailable = true;
}

// Extract channel ID from opaque handle
uint32_t RmtWorker::getChannelIdFromHandle(rmt_channel_handle_t handle) {
    // SAFETY WARNING: This relies on internal ESP-IDF structure layout
    // which may change between IDF versions. This is a fragile workaround
    // until ESP-IDF provides an official API to query channel ID.
    //
    // Tested on ESP-IDF 5.x series. If this breaks:
    // 1. Check if ESP-IDF added rmt_get_channel_id() or similar API
    // 2. Update this code to use official API
    // 3. If no API exists, inspect rmt_tx_channel_t definition in:
    //    components/esp_driver_rmt/src/rmt_tx.c

    if (handle == nullptr) {
        FL_WARN("getChannelIdFromHandle: null handle");
        return 0;
    }

    struct rmt_tx_channel_t {
        void* base;  // rmt_channel_t base (offset 0)
        uint32_t channel_id;  // offset sizeof(void*)
        // ... other fields we don't care about
    };

    rmt_tx_channel_t* tx_chan = reinterpret_cast<rmt_tx_channel_t*>(handle);
    uint32_t channel_id = tx_chan->channel_id;

    // Sanity check - channel ID should be in valid range
    if (channel_id >= SOC_RMT_CHANNELS_PER_GROUP) {
        FL_WARN("getChannelIdFromHandle: invalid channel_id " << channel_id << " (max " << (SOC_RMT_CHANNELS_PER_GROUP - 1) << ")");
        return 0;
    }

    return channel_id;
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32

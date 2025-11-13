#ifdef ESP32

#include "sdkconfig.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker.h"
#include "rmt5_worker_lut.h"

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
#include "esp_debug_helpers.h"  // For esp_backtrace_print()

#define RMT5_WORKER_TAG "rmt5_worker"

// RMT interrupt handling - Always use direct ISR
// The ESP-IDF v5.x RMT driver does NOT provide a threshold callback in rmt_tx_event_callbacks_t,
// only on_trans_done. Since we need threshold interrupts for ping-pong buffer refill,
// we must use direct ISR with manual register access (no alternative exists).

#define FASTLED_RMT5_HZ 10000000

#if defined(CONFIG_IDF_TARGET_ESP32P4)
#define RMT_SIG_PAD_IDX RMT_SIG_PAD_OUT0_IDX
#else
#define RMT_SIG_PAD_IDX RMT_SIG_OUT0_IDX
#endif

// Define rmt_item32_t union (compatible with RMT4)
union rmt_item32_t {
    struct {
        uint32_t duration0 : 15;
        uint32_t level0 : 1;
        uint32_t duration1 : 15;
        uint32_t level1 : 1;
    };
    uint32_t val;
};

// Define rmt_block_mem_t for IDF5 (removed from public headers)
typedef struct {
    struct {
        rmt_item32_t data32[SOC_RMT_MEM_WORDS_PER_CHANNEL];
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

// Global worker registry - maps channel ID to active worker
static fl::RmtWorker* DRAM_ATTR gOnChannel[SOC_RMT_CHANNELS_PER_GROUP] = {nullptr};

// Global interrupt handle (shared by all channels)
static intr_handle_t DRAM_ATTR gRMT5_intr_handle = nullptr;

// Maximum channel number (initialized at runtime)
static uint8_t gMaxChannel = SOC_RMT_CHANNELS_PER_GROUP;

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
    , mInterruptAllocated(false)
    , mCurrentPin(GPIO_NUM_NC)
    , mT1(0)
    , mT2(0)
    , mT3(0)
    , mResetNs(0)
    , mAvailable(true)
{
    // Initialize ISR data (handled by RmtWorkerIsrData constructor)
    // Workers start in available state
}

RmtWorker::~RmtWorker() {
    // wait tilll done
    waitForCompletion();
    // Unregister from global channel map
    if (mInterruptAllocated && mIsrData.mChannelId < SOC_RMT_CHANNELS_PER_GROUP) {
        gOnChannel[mIsrData.mChannelId] = nullptr;
    }

    // Clean up channel
    if (mChannel) {
        rmt_disable(mChannel);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
    }

    // Note: We do NOT free gRMT5_intr_handle because it's shared by all workers
    // The shared ISR will remain active as long as any worker exists
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
    tx_config.resolution_hz = 10000000;  // 10MHz (100ns resolution)
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
    mIsrData.mChannelId = getChannelIdFromHandle(mChannel);
    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Created channel_id=" << (unsigned)mIsrData.mChannelId);

    // Get direct pointer to RMT memory
    mIsrData.mRMT_mem_start = reinterpret_cast<volatile rmt_item32_t*>(&RMTMEM.chan[mIsrData.mChannelId].data32[0]);
    mIsrData.mRMT_mem_ptr = mIsrData.mRMT_mem_start;

    // Configure threshold interrupt for ping-pong buffer refill
    // Threshold = half of total buffer size, triggering refill when first half is transmitted
    // With 2 blocks × 64 words = 128 total words, threshold = 64 words (PULSES_PER_FILL)
    // However, hardware requires threshold in bytes: 64 words × 4 bytes/word = 256 bytes
    // But register expects word count, not byte count, so we use 48 (empirically determined)
    // TODO: Investigate exact threshold calculation - may need platform-specific tuning
    constexpr uint32_t RMT_THRESHOLD_LIMIT = PULSES_PER_FILL;  // 48 words for ping-pong refill

#if defined(CONFIG_IDF_TARGET_ESP32)
    RMT.tx_lim_ch[mIsrData.mChannelId].limit = RMT_THRESHOLD_LIMIT;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    RMT.chn_tx_lim[mIsrData.mChannelId].tx_lim_chn = RMT_THRESHOLD_LIMIT;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    RMT.tx_lim[mIsrData.mChannelId].limit = RMT_THRESHOLD_LIMIT;
#elif defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
    RMT.chn_tx_lim[mIsrData.mChannelId].tx_lim_chn = RMT_THRESHOLD_LIMIT;
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
    RMT.chn_tx_lim[mIsrData.mChannelId].tx_lim_chn = RMT_THRESHOLD_LIMIT;
#else
#error "RMT5 worker threshold setup not yet implemented for this ESP32 variant"
#endif

    // NOTE: Threshold interrupt setup moved to allocateInterrupt() for lazy initialization
    // This prevents interrupt watchdog timeout on ESP32-C6 during early boot

    // NOTE: Interrupt allocation deferred to first transmit() call
    // This prevents interrupt watchdog timeout on ESP32-C6 (RISC-V) during early boot


    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Channel created successfully");
    return true;
}

bool RmtWorker::allocateInterrupt() {
    // Interrupt already allocated - skip
    if (mInterruptAllocated) {
        return true;
    }

    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Allocating interrupt (first transmit)");

    // Register this worker in the global channel map
    gOnChannel[mIsrData.mChannelId] = this;

    // Enable threshold interrupt for this channel using direct register access
    uint32_t thresh_int_bit = 8 + mIsrData.mChannelId;  // Bits 8-11 for channels 0-3
    RMT.int_ena.val |= (1 << thresh_int_bit);

    // Allocate shared global ISR (only once for all channels, like RMT4)
    if (gRMT5_intr_handle == nullptr) {
        FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Allocating shared global ISR for all RMT channels");

        esp_err_t ret = esp_intr_alloc(
            ETS_RMT_INTR_SOURCE,
            ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
            &RmtWorker::sharedGlobalISR,
            nullptr,  // No user data - ISR reads gOnChannel[] directly
            &gRMT5_intr_handle
        );

        if (ret != ESP_OK) {
            FL_WARN("RmtWorker[" << (int)mWorkerId << "]: Failed to allocate shared ISR: " << esp_err_to_name(ret) << " (0x" << ret << ")");
            gOnChannel[mIsrData.mChannelId] = nullptr;  // Clean up registration
            return false;
        }

        FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Shared global ISR allocated successfully (Level 3, ETS_RMT_INTR_SOURCE)");
    }

    mInterruptAllocated = true;
    return true;
}

void RmtWorker::tearDownRMTChannel(gpio_num_t old_pin) {
    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Tearing down RMT channel (old pin=" << (int)old_pin << ")");

    // Disable and delete the RMT channel
    if (mChannel) {
        rmt_disable(mChannel);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
    }

    // Unregister from global channel map
    if (mInterruptAllocated && mIsrData.mChannelId < SOC_RMT_CHANNELS_PER_GROUP) {
        gOnChannel[mIsrData.mChannelId] = nullptr;
        mInterruptAllocated = false;
    }

    // Note: We do NOT free gRMT5_intr_handle here because it's shared by all workers
    // It will be cleaned up when the last worker is destroyed (see destructor)

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
    // Extract timing values from struct
    uint32_t t1 = timing.T1;
    uint32_t t2 = timing.T2;
    uint32_t t3 = timing.T3;
    uint32_t reset_ns = timing.RESET * 1000;

    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: configure() called - pin=" << (int)pin
               << ", t1=" << t1 << ", t2=" << t2 << ", t3=" << t3 << ", reset_us=" << timing.RESET);

    // Channel creation deferred to first transmit() call
    // This prevents ESP32-C6 (RISC-V) boot hang during RMT hardware initialization

    // Check if reconfiguration needed
    if (mCurrentPin == pin && mT1 == t1 && mT2 == t2 && mT3 == t3 && mResetNs == reset_ns) {
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

    // Update configuration
    mCurrentPin = pin;
    mT1 = t1;
    mT2 = t2;
    mT3 = t3;
    mResetNs = reset_ns;

    // Recalculate RMT symbols
    // 10MHz = 100ns per tick
    const uint32_t TICKS_PER_NS = 10;  // 10MHz / 1GHz = 10 ticks per ns (actually 0.01, so we need to scale)
    // Actually: 10MHz means 1 tick = 100ns, so ticks = ns / 100

    // For WS2812B:
    // T0H: 400ns = 4 ticks, T0L: 850ns = 8.5 ticks
    // T1H: 800ns = 8 ticks, T1L: 450ns = 4.5 ticks

    // Convert from ns to ticks (divide by 100 since 1 tick = 100ns at 10MHz)
    auto ns_to_ticks = [](uint32_t ns) -> uint16_t {
        static_assert(FASTLED_RMT5_HZ == 10000000, "FASTLED_RMT5_HZ is not 10MHz");
        return static_cast<uint16_t>((ns + 50) / 100);  // Round to nearest tick
    };

    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Timing configured: T1=" << t1
               << "ns, T2=" << t2 << "ns, T3=" << t3 << "ns");

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
        

        int signal_idx = RMT_SIG_PAD_IDX + mIsrData.mChannelId;
        gpio_matrix_out(mCurrentPin, signal_idx, false, false);

        esp_err_t ret = rmt_enable(mChannel);
        if (ret != ESP_OK) {
            FL_WARN("Worker[" << (int)mWorkerId << "]: Failed to enable channel: " << esp_err_to_name(ret));
            return;
        }
        FL_LOG_RMT("Worker[" << (int)mWorkerId << "]: Channel enabled and ready");
    }

    // Allocate interrupt on first transmit (lazy initialization)
    // This prevents interrupt watchdog timeout on ESP32-C6 during early boot
    if (!mInterruptAllocated) {
        if (!allocateInterrupt()) {
            FL_WARN("Worker[" << (int)mWorkerId << "]: Failed to allocate interrupt - aborting transmit");
            return;
        }
    }

    // Configure ISR data right before transmission
    // Build RMT items and LUT on the stack from stored timing parameters
    auto ns_to_ticks = [](uint32_t ns) -> uint16_t {
        static_assert(FASTLED_RMT5_HZ == 10000000, "FASTLED_RMT5_HZ is not 10MHz");
        return static_cast<uint16_t>((ns + 50) / 100);  // Round to nearest tick
    };

    uint16_t t1_ticks = ns_to_ticks(mT1);
    uint16_t t2_ticks = ns_to_ticks(mT2);
    uint16_t t3_ticks = ns_to_ticks(mT3);

    // Build RMT items for 0 and 1 bits on the stack
    RmtWorkerIsrData::rmt_item32_t zero_item;
    zero_item.level0 = 1;
    zero_item.duration0 = t1_ticks;
    zero_item.level1 = 0;
    zero_item.duration1 = t2_ticks + t3_ticks;

    RmtWorkerIsrData::rmt_item32_t one_item;
    one_item.level0 = 1;
    one_item.duration0 = t1_ticks + t2_ticks;
    one_item.level1 = 0;
    one_item.duration1 = t3_ticks;

    // Build nibble LUT on the stack using helper function
    rmt_nibble_lut_t nibble_lut;
    buildNibbleLut(nibble_lut, zero_item.val, one_item.val);

    // Configure ISR data with all transmission parameters
    mIsrData.config(
        this,
        mIsrData.mChannelId,
        mIsrData.mRMT_mem_start,
        pixel_data,
        num_bytes,
        nibble_lut
    );

    // Debug: Log transmission start
    FL_LOG_RMT("Worker[" << (int)mWorkerId << "]: TX START - " << num_bytes << " bytes (" << (num_bytes / 3) << " LEDs)");

    // Availability is controlled by semaphore (taken by pool before transmit)

    // Fill both halves initially (like RMT4)
    fillNextHalf();  // Fill half 0
    fillNextHalf();  // Fill half 1

    // Reset memory pointer and start transmission
    mIsrData.mWhichHalf = 0;
    mIsrData.mRMT_mem_ptr = mIsrData.mRMT_mem_start;

    FL_LOG_RMT("Worker[" << (int)mWorkerId << "]: Both halves filled, starting HW transmission");

    tx_start();
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
    // Set availability flag to true
    mAvailable = true;
    // Also clear worker pointer
    mIsrData.mWorker = nullptr;
}

void RmtWorker::markAsUnavailable() {
    // Set availability flag to false
    mAvailable = false;
    // Also set worker pointer for ISR access
    mIsrData.mWorker = this;
}

// Convert byte to RMT symbol (8 RMT items) using nibble lookup table
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
FASTLED_FORCE_INLINE void IRAM_ATTR RmtWorker::convertByteToRmt(
    uint8_t byte_val,
    const rmt_nibble_lut_t& lut,
    volatile RmtWorkerIsrData::rmt_item32_t* out
) {
    // Copy 4 RMT items from LUT for high nibble (bits 7-4)
    const RmtWorkerIsrData::rmt_item32_t* high_lut = lut[byte_val >> 4];
    out[0].val = high_lut[0].val;
    out[1].val = high_lut[1].val;
    out[2].val = high_lut[2].val;
    out[3].val = high_lut[3].val;

    // Copy 4 RMT items from LUT for low nibble (bits 3-0)
    const RmtWorkerIsrData::rmt_item32_t* low_lut = lut[byte_val & 0x0F];
    out[4].val = low_lut[0].val;
    out[5].val = low_lut[1].val;
    out[6].val = low_lut[2].val;
    out[7].val = low_lut[3].val;
}
FL_DISABLE_WARNING_POP

// Fill next half of RMT buffer (interrupt context)
// OPTIMIZATION: Matches RMT4 approach - no defensive checks, trust the buffer sizing math
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
void IRAM_ATTR RmtWorker::fillNextHalf() {
    // OPTIMIZATION: Cache volatile member variables to avoid repeated access
    // Since we're in ISR context and own the buffer state, we can safely cache and update once
    FASTLED_REGISTER int cur = mIsrData.mCur;
    FASTLED_REGISTER int num_bytes = mIsrData.mNumBytes;
    const uint8_t* pixel_data = mIsrData.mPixelData;
    // Cache LUT reference for ISR performance (avoid repeated member access)
    const rmt_nibble_lut_t& lut = mIsrData.mNibbleLut;
    // Remember that volatile writes are super fast, volatile reads
    // are super slow.
    volatile FASTLED_REGISTER RmtWorkerIsrData::rmt_item32_t* pItem = mIsrData.mRMT_mem_ptr;

    // Fill PULSES_PER_FILL / 8 bytes (since each byte = 8 pulses)
    // Note: Boundary checking removed - if buffer sizing is correct, overflow is impossible
    // Buffer size: MAX_PULSES = PULSES_PER_FILL * 2 (ping-pong halves)
    constexpr int i_end = PULSES_PER_FILL / 8;

    for (FASTLED_REGISTER int i = 0; i < i_end; i++) {
        if (cur < num_bytes) {
            convertByteToRmt(pixel_data[cur], lut, pItem);
            pItem += 8;
            cur++;
        } else {
            // End marker - zero duration signals end of transmission
            pItem->val = 0;
            pItem++;
        }
    }

    // Write back updated position (single volatile write instead of many)
    mIsrData.mCur = cur;

    // Flip to other half (matches RMT4 pattern)
    mIsrData.mWhichHalf++;
    if (mIsrData.mWhichHalf == 2) {
        pItem = mIsrData.mRMT_mem_start;
        mIsrData.mWhichHalf = 0;
    }
    mIsrData.mRMT_mem_ptr = pItem;
}
FL_DISABLE_WARNING_POP

// Start RMT transmission
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
void IRAM_ATTR RmtWorker::tx_start() {
    // Use direct register access like RMT4
    // This is platform-specific and based on RMT4's approach
#if defined(CONFIG_IDF_TARGET_ESP32)
    // Reset RMT memory read pointer
    RMT.conf_ch[mIsrData.mChannelId].conf1.mem_rd_rst = 1;
    RMT.conf_ch[mIsrData.mChannelId].conf1.mem_rd_rst = 0;
    RMT.conf_ch[mIsrData.mChannelId].conf1.apb_mem_rst = 1;
    RMT.conf_ch[mIsrData.mChannelId].conf1.apb_mem_rst = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + mIsrData.mChannelId;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << mIsrData.mChannelId) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << mIsrData.mChannelId) | (1 << thresh_bit);

    // Start transmission
    RMT.conf_ch[mIsrData.mChannelId].conf1.tx_start = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // Reset RMT memory read pointer
    RMT.chnconf0[mIsrData.mChannelId].mem_rd_rst_chn = 1;
    RMT.chnconf0[mIsrData.mChannelId].mem_rd_rst_chn = 0;
    RMT.chnconf0[mIsrData.mChannelId].apb_mem_rst_chn = 1;
    RMT.chnconf0[mIsrData.mChannelId].apb_mem_rst_chn = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + mIsrData.mChannelId;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << mIsrData.mChannelId) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << mIsrData.mChannelId) | (1 << thresh_bit);

    // Start transmission
    RMT.chnconf0[mIsrData.mChannelId].conf_update_chn = 1;
    RMT.chnconf0[mIsrData.mChannelId].tx_start_chn = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    // Reset RMT memory read pointer
    RMT.tx_conf[mIsrData.mChannelId].mem_rd_rst = 1;
    RMT.tx_conf[mIsrData.mChannelId].mem_rd_rst = 0;
    RMT.tx_conf[mIsrData.mChannelId].mem_rst = 1;
    RMT.tx_conf[mIsrData.mChannelId].mem_rst = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + mIsrData.mChannelId;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << mIsrData.mChannelId) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << mIsrData.mChannelId) | (1 << thresh_bit);

    // Start transmission
    RMT.tx_conf[mIsrData.mChannelId].conf_update = 1;
    RMT.tx_conf[mIsrData.mChannelId].tx_start = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
    // Reset RMT memory read pointer
    RMT.chnconf0[mIsrData.mChannelId].mem_rd_rst_chn = 1;
    RMT.chnconf0[mIsrData.mChannelId].mem_rd_rst_chn = 0;
    RMT.chnconf0[mIsrData.mChannelId].apb_mem_rst_chn = 1;
    RMT.chnconf0[mIsrData.mChannelId].apb_mem_rst_chn = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + mIsrData.mChannelId;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << mIsrData.mChannelId) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << mIsrData.mChannelId) | (1 << thresh_bit);

    // Start transmission
    RMT.chnconf0[mIsrData.mChannelId].conf_update_chn = 1;
    RMT.chnconf0[mIsrData.mChannelId].tx_start_chn = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
    // Reset RMT memory read pointer
    RMT.chnconf0[mIsrData.mChannelId].mem_rd_rst_chn = 1;
    RMT.chnconf0[mIsrData.mChannelId].mem_rd_rst_chn = 0;
    RMT.chnconf0[mIsrData.mChannelId].apb_mem_rst_chn = 1;
    RMT.chnconf0[mIsrData.mChannelId].apb_mem_rst_chn = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + mIsrData.mChannelId;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << mIsrData.mChannelId) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << mIsrData.mChannelId) | (1 << thresh_bit);

    // Start transmission
    RMT.chnconf0[mIsrData.mChannelId].conf_update_chn = 1;
    RMT.chnconf0[mIsrData.mChannelId].tx_start_chn = 1;
#else
#error "RMT5 worker not yet implemented for this ESP32 variant"
#endif
}
FL_DISABLE_WARNING_POP

// Shared Global ISR - Handles ALL RMT channels in one pass (like RMT4)
//
// DESIGN: This ISR reads RMT.int_st.val once and processes all pending channel
// interrupts atomically. This prevents race conditions and missed interrupts
// when multiple channels fire simultaneously.
//
// PERFORMANCE: Single ISR invocation with O(n) channel scan is faster than
// multiple per-channel ISR invocations due to reduced context switch overhead.
void IRAM_ATTR RmtWorker::sharedGlobalISR(void* arg) {
    // Read interrupt status once - captures all pending channel interrupts atomically
    uint32_t intr_st = RMT.int_st.val;

    // Process all channels in a single pass
    for (uint8_t channel = 0; channel < gMaxChannel; channel++) {
        RmtWorker* worker = gOnChannel[channel];

        // Skip inactive channels
        if (worker == nullptr) {
            continue;
        }

        // Platform-specific bit positions (from RMT4)
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
        int tx_done_bit = channel;
        int tx_next_bit = channel + 8;  // Threshold interrupt
#else
#error "RMT5 worker ISR not yet implemented for this ESP32 variant"
#endif

        // Check threshold interrupt (buffer half empty) - refill needed
        if (intr_st & (1 << tx_next_bit)) {
            worker->fillNextHalf();
            RMT.int_clr.val = (1 << tx_next_bit);
        }

        // Check done interrupt (transmission complete)
        if (intr_st & (1 << tx_done_bit)) {
            // Signal completion by setting availability flag
            worker->mAvailable = true;
            // Also clear worker pointer
            worker->mIsrData.mWorker = nullptr;
            RMT.int_clr.val = (1 << tx_done_bit);
        }
    }
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

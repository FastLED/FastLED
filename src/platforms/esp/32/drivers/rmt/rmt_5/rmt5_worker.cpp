#ifdef ESP32

#include "sdkconfig.h"
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker.h"

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

// RMT interrupt handling mode selection
// When 1: Use low-level ISR with direct register access (current default, faster)
// When 0: Use high-level RMT5 callback API (simpler, but may have higher overhead)
#ifndef FASTLED_RMT5_USE_DIRECT_ISR
#define FASTLED_RMT5_USE_DIRECT_ISR 1
#endif

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
// NMI (Level 7) HANDLER SUPPORT
//=============================================================================

/*
 * Global DRAM pointer for NMI-safe buffer refill
 *
 * CRITICAL: This must be in DRAM (not flash) for NMI access safety.
 * When using Level 7 NMI for RMT interrupts, the handler cannot call
 * FreeRTOS APIs (including portENTER_CRITICAL_ISR). This global pointer
 * allows the NMI handler to directly access the RmtWorker instance.
 *
 * THREAD SAFETY: This is written once during interrupt allocation and
 * never changed during NMI operation. The NMI handler will only call
 * fillNextHalf() which is already designed for ISR context.
 *
 * NOTE: Currently unused - Level 3 interrupts are still the default.
 * This infrastructure is prepared for future Level 7 NMI migration.
 */
fl::RmtWorker* DRAM_ATTR g_rmt5_nmi_worker = nullptr;

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
    , mChannelId(0)
    , mWorkerId(0)
    , mIntrHandle(nullptr)
    , mInterruptAllocated(false)
    , mCurrentPin(GPIO_NUM_NC)
    , mT1(0)
    , mT2(0)
    , mT3(0)
    , mResetNs(0)
    , mCur(0)
    , mWhichHalf(0)
    , mRMT_mem_start(nullptr)
    , mRMT_mem_ptr(nullptr)
    , mPixelData(nullptr)
    , mNumBytes(0)
{
    // Initialize ISR data
    // FIXED: Simplified to use only volatile bool - no semaphore needed
    // Workers start in available state (true)
    mIsrData.mAvailable = true;

    // Initialize zero and one symbols to safe defaults
    mZero.duration0 = 0;
    mZero.level0 = 0;
    mZero.duration1 = 0;
    mZero.level1 = 0;

    mOne.duration0 = 0;
    mOne.level0 = 0;
    mOne.duration1 = 0;
    mOne.level1 = 0;
}

RmtWorker::~RmtWorker() {
    if (mIntrHandle) {
        esp_intr_free(mIntrHandle);
        mIntrHandle = nullptr;
    }
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
    mChannelId = getChannelIdFromHandle(mChannel);
    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Created channel_id=" << (unsigned)mChannelId);

    // Get direct pointer to RMT memory
    mRMT_mem_start = reinterpret_cast<volatile rmt_item32_t*>(&RMTMEM.chan[mChannelId].data32[0]);
    mRMT_mem_ptr = mRMT_mem_start;

    // Configure threshold interrupt for ping-pong buffer refill
    // Threshold = half of total buffer size, triggering refill when first half is transmitted
    // With 2 blocks × 64 words = 128 total words, threshold = 64 words (PULSES_PER_FILL)
    // However, hardware requires threshold in bytes: 64 words × 4 bytes/word = 256 bytes
    // But register expects word count, not byte count, so we use 48 (empirically determined)
    // TODO: Investigate exact threshold calculation - may need platform-specific tuning
    constexpr uint32_t RMT_THRESHOLD_LIMIT = PULSES_PER_FILL;  // 48 words for ping-pong refill

#if defined(CONFIG_IDF_TARGET_ESP32)
    RMT.tx_lim_ch[mChannelId].limit = RMT_THRESHOLD_LIMIT;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    RMT.chn_tx_lim[mChannelId].tx_lim_chn = RMT_THRESHOLD_LIMIT;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    RMT.tx_lim[mChannelId].limit = RMT_THRESHOLD_LIMIT;
#elif defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
    RMT.chn_tx_lim[mChannelId].tx_lim_chn = RMT_THRESHOLD_LIMIT;
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
    RMT.chn_tx_lim[mChannelId].tx_lim_chn = RMT_THRESHOLD_LIMIT;
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

#if FASTLED_RMT5_USE_DIRECT_ISR
    // Low-level ISR approach: Direct register access with manual interrupt handling

    // Enable threshold interrupt using direct register access
    uint32_t thresh_int_bit = 8 + mChannelId;  // Bits 8-11 for channels 0-3
    RMT.int_ena.val |= (1 << thresh_int_bit);

    // Allocate custom ISR at Level 3 (compatible with Xtensa and RISC-V)
    // NOTE: ETS_RMT_INTR_SOURCE is an enum, not a #define, so it exists on all ESP32 platforms
    // We MUST use direct ISR since we're using manual register writes in tx_start()
    esp_err_t ret = esp_intr_alloc(
        ETS_RMT_INTR_SOURCE,
        ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
        &RmtWorker::globalISR,
        this,
        &mIntrHandle
    );

    if (ret != ESP_OK) {
        FL_WARN("RmtWorker[" << (int)mWorkerId << "]: Failed to allocate ISR: " << esp_err_to_name(ret) << " (0x" << ret << ")");
        return false;
    }

    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: Direct ISR allocated successfully (Level 3, ETS_RMT_INTR_SOURCE)");
#else
    // High-level callback API approach: Use RMT5 driver callbacks

    rmt_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_done = &RmtWorker::onTransDoneCallback;

    esp_err_t ret = rmt_tx_register_event_callbacks(mChannel, &callbacks, this);

    if (ret != ESP_OK) {
        FL_WARN("RmtWorker[" << (int)mWorkerId << "]: Failed to register callbacks: " << esp_err_to_name(ret) << " (0x" << ret << ")");
        return false;
    }

    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: High-level callbacks registered successfully");
#endif

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

    // Free interrupt handler
    if (mIntrHandle) {
        esp_intr_free(mIntrHandle);
        mIntrHandle = nullptr;
        mInterruptAllocated = false;
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

    mZero.level0 = 1;
    mZero.duration0 = ns_to_ticks(t1);
    mZero.level1 = 0;
    mZero.duration1 = ns_to_ticks(t2 + t3);

    mOne.level0 = 1;
    mOne.duration0 = ns_to_ticks(t1 + t2);
    mOne.level1 = 0;
    mOne.duration1 = ns_to_ticks(t3);

    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: mZero={dur0=" << mZero.duration0 << ", lvl0=" << mZero.level0
               << ", dur1=" << mZero.duration1 << ", lvl1=" << mZero.level1 << ", val=0x" << mZero.val << "}");
    FL_LOG_RMT("RmtWorker[" << (int)mWorkerId << "]: mOne={dur0=" << mOne.duration0 << ", lvl0=" << mOne.level0
               << ", dur1=" << mOne.duration1 << ", lvl1=" << mOne.level1 << ", val=0x" << mOne.val << "}");

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

    // Allocate interrupt on first transmit (lazy initialization)
    // This prevents interrupt watchdog timeout on ESP32-C6 during early boot
    if (!mInterruptAllocated) {
        if (!allocateInterrupt()) {
            FL_WARN("Worker[" << (int)mWorkerId << "]: Failed to allocate interrupt - aborting transmit");
            return;
        }
    }

    // Store pixel data pointer (not owned by worker)
    mPixelData = pixel_data;
    mNumBytes = num_bytes;

    // Debug: Log transmission start
    FL_LOG_RMT("Worker[" << (int)mWorkerId << "]: TX START - " << num_bytes << " bytes (" << (num_bytes / 3) << " LEDs)");

    // Reset state
    mCur = 0;
    mWhichHalf = 0;
    mRMT_mem_ptr = mRMT_mem_start;

    // Availability is controlled by semaphore (taken by pool before transmit)

    // Fill both halves initially (like RMT4)
    fillNextHalf();  // Fill half 0
    fillNextHalf();  // Fill half 1

    // Reset memory pointer and start transmission
    mWhichHalf = 0;
    mRMT_mem_ptr = mRMT_mem_start;

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
    // Set volatile availability flag to true
    mIsrData.mAvailable = true;
}

void RmtWorker::markAsUnavailable() {
    // Set volatile availability flag to false
    mIsrData.mAvailable = false;
}

// Convert byte to 8 RMT items (one per bit)
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
FASTLED_FORCE_INLINE void IRAM_ATTR RmtWorker::convertByteToRmt(
    uint8_t byte_val,
    uint32_t zero_val,
    uint32_t one_val,
    volatile rmt_item32_t* out
) {
    // OPTIMIZATION: Pass zero/one as parameters (loaded once per fillNextHalf() call)
    // instead of reading from member variables each time (RMT4 pattern)
    FASTLED_REGISTER uint32_t pixel_u32 = byte_val;
    pixel_u32 <<= 24;  // Shift to MSB position

    #pragma unroll
    for (FASTLED_REGISTER uint32_t j = 0; j < 8; j++) {
        out[j].val = (pixel_u32 & 0x80000000UL) ? one_val : zero_val;
        pixel_u32 <<= 1;
    }
}
FL_DISABLE_WARNING_POP

// Fill next half of RMT buffer (interrupt context)
// OPTIMIZATION: Matches RMT4 approach - no defensive checks, trust the buffer sizing math
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
void IRAM_ATTR RmtWorker::fillNextHalf() {
    // OPTIMIZATION: Cache zero/one values in local registers before loop (RMT4 pattern)
    // This avoids repeated member variable access inside convertByteToRmt()
    FASTLED_REGISTER uint32_t zero_val = mZero.val;
    FASTLED_REGISTER uint32_t one_val = mOne.val;

    // OPTIMIZATION: Cache volatile member variables to avoid repeated access
    // Since we're in ISR context and own the buffer state, we can safely cache and update once
    FASTLED_REGISTER int cur = mCur;
    FASTLED_REGISTER int num_bytes = mNumBytes;
    const uint8_t* pixel_data = mPixelData;
    // Remember that volatile writes are super fast, volatile reads
    // are super slow.
    volatile FASTLED_REGISTER rmt_item32_t* pItem = mRMT_mem_ptr;

    // Fill PULSES_PER_FILL / 8 bytes (since each byte = 8 pulses)
    // Note: Boundary checking removed - if buffer sizing is correct, overflow is impossible
    // Buffer size: MAX_PULSES = PULSES_PER_FILL * 2 (ping-pong halves)
    constexpr int i_end = PULSES_PER_FILL / 8;

    for (FASTLED_REGISTER int i = 0; i < i_end; i++) {
        if (cur < num_bytes) {
            convertByteToRmt(pixel_data[cur], zero_val, one_val, pItem);
            pItem += 8;
            cur++;
        } else {
            // End marker - zero duration signals end of transmission
            pItem->val = 0;
            pItem++;
        }
    }

    // Write back updated position (single volatile write instead of many)
    mCur = cur;

    // Flip to other half (matches RMT4 pattern)
    mWhichHalf++;
    if (mWhichHalf == 2) {
        pItem = mRMT_mem_start;
        mWhichHalf = 0;
    }
    mRMT_mem_ptr = pItem;
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
    RMT.conf_ch[mChannelId].conf1.mem_rd_rst = 1;
    RMT.conf_ch[mChannelId].conf1.mem_rd_rst = 0;
    RMT.conf_ch[mChannelId].conf1.apb_mem_rst = 1;
    RMT.conf_ch[mChannelId].conf1.apb_mem_rst = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + mChannelId;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << mChannelId) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << mChannelId) | (1 << thresh_bit);

    // Start transmission
    RMT.conf_ch[mChannelId].conf1.tx_start = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    // Reset RMT memory read pointer
    RMT.chnconf0[mChannelId].mem_rd_rst_chn = 1;
    RMT.chnconf0[mChannelId].mem_rd_rst_chn = 0;
    RMT.chnconf0[mChannelId].apb_mem_rst_chn = 1;
    RMT.chnconf0[mChannelId].apb_mem_rst_chn = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + mChannelId;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << mChannelId) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << mChannelId) | (1 << thresh_bit);

    // Start transmission
    RMT.chnconf0[mChannelId].conf_update_chn = 1;
    RMT.chnconf0[mChannelId].tx_start_chn = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    // Reset RMT memory read pointer
    RMT.tx_conf[mChannelId].mem_rd_rst = 1;
    RMT.tx_conf[mChannelId].mem_rd_rst = 0;
    RMT.tx_conf[mChannelId].mem_rst = 1;
    RMT.tx_conf[mChannelId].mem_rst = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + mChannelId;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << mChannelId) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << mChannelId) | (1 << thresh_bit);

    // Start transmission
    RMT.tx_conf[mChannelId].conf_update = 1;
    RMT.tx_conf[mChannelId].tx_start = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5)
    // Reset RMT memory read pointer
    RMT.chnconf0[mChannelId].mem_rd_rst_chn = 1;
    RMT.chnconf0[mChannelId].mem_rd_rst_chn = 0;
    RMT.chnconf0[mChannelId].apb_mem_rst_chn = 1;
    RMT.chnconf0[mChannelId].apb_mem_rst_chn = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + mChannelId;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << mChannelId) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << mChannelId) | (1 << thresh_bit);

    // Start transmission
    RMT.chnconf0[mChannelId].conf_update_chn = 1;
    RMT.chnconf0[mChannelId].tx_start_chn = 1;
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
    // Reset RMT memory read pointer
    RMT.chnconf0[mChannelId].mem_rd_rst_chn = 1;
    RMT.chnconf0[mChannelId].mem_rd_rst_chn = 0;
    RMT.chnconf0[mChannelId].apb_mem_rst_chn = 1;
    RMT.chnconf0[mChannelId].apb_mem_rst_chn = 0;

    // Clear and enable both TX end and threshold interrupts
    uint32_t thresh_bit = 8 + mChannelId;  // Bits 8-11 for threshold
    RMT.int_clr.val = (1 << mChannelId) | (1 << thresh_bit);
    RMT.int_ena.val |= (1 << mChannelId) | (1 << thresh_bit);

    // Start transmission
    RMT.chnconf0[mChannelId].conf_update_chn = 1;
    RMT.chnconf0[mChannelId].tx_start_chn = 1;
#else
#error "RMT5 worker not yet implemented for this ESP32 variant"
#endif
}
FL_DISABLE_WARNING_POP

// RMT5 TX done callback (called from ISR context)
// NOTE: Used when FASTLED_RMT5_USE_DIRECT_ISR=0 (high-level callback API)
//       Registered via rmt_tx_register_event_callbacks() in allocateInterrupt()
bool IRAM_ATTR RmtWorker::onTransDoneCallback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_data) {
    RmtWorker* worker = static_cast<RmtWorker*>(user_data);
    worker->mIsrData.mAvailable = true;
    return false;  // Don't yield from ISR
}

// Global ISR handler (dispatches to instance handlers)
// NOTE: Used when FASTLED_RMT5_USE_DIRECT_ISR=1 (default)
//       Registered via esp_intr_alloc() in allocateInterrupt()
void IRAM_ATTR RmtWorker::globalISR(void* arg) {
    RmtWorker* worker = static_cast<RmtWorker*>(arg);
    uint32_t intr_st = RMT.int_st.val;

    // Platform-specific bit positions (from RMT4)
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32P4)
    int tx_done_bit = worker->mChannelId;
    int tx_next_bit = worker->mChannelId + 8;  // Threshold interrupt
#else
#error "RMT5 worker ISR not yet implemented for this ESP32 variant"
#endif

    // Check threshold interrupt (buffer half empty) - if enabled
    // Note: Currently disabled - using one-shot mode
    if (intr_st & (1 << tx_next_bit)) {
        worker->handleThresholdInterrupt();
        RMT.int_clr.val = (1 << tx_next_bit);
    }

    // Check done interrupt (transmission complete)
    if (intr_st & (1 << tx_done_bit)) {
        // Signal completion by setting availability flag
        worker->mIsrData.mAvailable = true;

        RMT.int_clr.val = (1 << tx_done_bit);
    }
}

// Handle threshold interrupt (refill next buffer half)
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(attributes)
void IRAM_ATTR RmtWorker::handleThresholdInterrupt() {
    // OPTIMIZATION: Removed mThresholdIsrCount++ - ISR should be as fast as possible
    fillNextHalf();
}
FL_DISABLE_WARNING_POP



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

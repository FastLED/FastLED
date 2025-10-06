#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"
#include "esp_intr_alloc.h"
#include "driver/gpio.h"
#include "soc/soc.h"  // For ETS_RMT_INTR_SOURCE
#include "soc/rmt_struct.h"
#include "rom/gpio.h"  // For gpio_matrix_out
#include "rom/ets_sys.h"  // For ets_printf (ISR-safe logging)
#include "esp_log.h"

#ifdef __cplusplus
}
#endif

#include "fl/force_inline.h"
#include "fl/assert.h"
#include "fl/memfill.h"

#define RMT5_WORKER_TAG "rmt5_worker"

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

namespace fl {

// Static spinlock for ISR synchronization
portMUX_TYPE RmtWorker::sRmtSpinlock = portMUX_INITIALIZER_UNLOCKED;

RmtWorker::RmtWorker()
    : mChannel(nullptr)
    , mChannelId(0)
    , mWorkerId(0)
    , mIntrHandle(nullptr)
    , mCurrentPin(GPIO_NUM_NC)
    , mT1(0)
    , mT2(0)
    , mT3(0)
    , mResetNs(0)
    , mCur(0)
    , mWhichHalf(0)
    , mRMT_mem_start(nullptr)
    , mRMT_mem_ptr(nullptr)
    , mAvailable(true)
    , mTransmitting(false)
    , mPixelData(nullptr)
    , mNumBytes(0)
{
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

    // Create RMT TX channel with double memory blocks
    rmt_tx_channel_config_t tx_config = {};
    tx_config.gpio_num = GPIO_NUM_NC;  // Will be set during configure()
    tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_config.resolution_hz = 10000000;  // 10MHz (100ns resolution)
    tx_config.mem_block_symbols = 2 * FASTLED_RMT_MEM_WORDS_PER_CHANNEL;  // Double buffer
    tx_config.trans_queue_depth = 1;
    tx_config.flags.invert_out = false;
    tx_config.flags.with_dma = false;  // Phase 1: No DMA

    esp_err_t ret = rmt_new_tx_channel(&tx_config, &mChannel);
    if (ret != ESP_OK) {
        ESP_LOGE(RMT5_WORKER_TAG, "RmtWorker[%d]: Failed to create RMT TX channel: %d", worker_id, ret);
        return false;
    }

    // Extract channel ID from handle (relies on internal IDF structure)
    mChannelId = getChannelIdFromHandle(mChannel);

    // Get direct pointer to RMT memory
    mRMT_mem_start = reinterpret_cast<volatile rmt_item32_t*>(&RMTMEM.chan[mChannelId].data32[0]);
    mRMT_mem_ptr = mRMT_mem_start;

    // Enable the channel
    ret = rmt_enable(mChannel);
    if (ret != ESP_OK) {
        ESP_LOGE(RMT5_WORKER_TAG, "RmtWorker[%d]: Failed to enable RMT channel: %d", worker_id, ret);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
        return false;
    }

    // Configure threshold interrupt for double-buffer ping-pong
    // Research from iteration 4 confirmed ESP-IDF v5 DOES support threshold interrupts!
    // Threshold interrupt bits: channel 0-3 use bits 8-11 in int_ena/int_st registers
    // Threshold value register: chn_tx_lim[channel].tx_lim_chn

    // Set threshold to 48 items (3/4 of 64-word buffer)
    // This triggers interrupt when 48 items have been transmitted, leaving 16 items
    // in hardware buffer while we refill the next half
#if CONFIG_IDF_TARGET_ESP32
    RMT.tx_lim_ch[mChannelId].limit = 48;
#elif CONFIG_IDF_TARGET_ESP32S3
    RMT.chn_tx_lim[mChannelId].tx_lim_chn = 48;
#elif CONFIG_IDF_TARGET_ESP32C3
    RMT.tx_lim[mChannelId].limit = 48;
#elif CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
    RMT.chn_tx_lim[mChannelId].tx_lim_chn = 48;
#else
#error "RMT5 worker threshold setup not yet implemented for this ESP32 variant"
#endif

    // Enable threshold interrupt using direct register access
    uint32_t thresh_int_bit = 8 + mChannelId;  // Bits 8-11 for channels 0-3
    RMT.int_ena.val |= (1 << thresh_int_bit);

    ESP_LOGI(RMT5_WORKER_TAG, "RmtWorker[%d]: Threshold interrupt enabled on bit %d", worker_id, thresh_int_bit);

    // Allocate custom ISR at Level 3 (compatible with Xtensa and RISC-V)
    // This gives us control over both threshold and done interrupts
#ifdef ETS_RMT_INTR_SOURCE
    ret = esp_intr_alloc(
        ETS_RMT_INTR_SOURCE,
        ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
        &RmtWorker::globalISR,
        this,
        &mIntrHandle
    );

    if (ret != ESP_OK) {
        ESP_LOGE(RMT5_WORKER_TAG, "RmtWorker[%d]: Failed to allocate ISR: %d", worker_id, ret);
        rmt_disable(mChannel);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
        return false;
    }

    ESP_LOGI(RMT5_WORKER_TAG, "RmtWorker[%d]: Custom ISR allocated successfully", worker_id);
#else
    // Fallback: Use RMT5 high-level callback API if ETS_RMT_INTR_SOURCE not available
    ESP_LOGW(RMT5_WORKER_TAG, "RmtWorker[%d]: ETS_RMT_INTR_SOURCE not available, using callbacks", worker_id);

    rmt_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_done = &RmtWorker::onTransDoneCallback;
    ret = rmt_tx_register_event_callbacks(mChannel, &callbacks, this);
    if (ret != ESP_OK) {
        ESP_LOGE(RMT5_WORKER_TAG, "RmtWorker[%d]: Failed to register callbacks: %d", worker_id, ret);
        rmt_disable(mChannel);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
        return false;
    }

    ESP_LOGW(RMT5_WORKER_TAG, "RmtWorker[%d]: Threshold interrupts require custom ISR (not available in callback mode)", worker_id);
#endif

    mAvailable = true;
    return true;
}

bool RmtWorker::configure(gpio_num_t pin, int t1, int t2, int t3, uint32_t reset_ns) {
    // Check if reconfiguration needed
    if (mCurrentPin == pin && mT1 == t1 && mT2 == t2 && mT3 == t3 && mResetNs == reset_ns) {
        return true;  // Already configured
    }

    // Wait for active transmission
    if (mTransmitting) {
        waitForCompletion();
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

    // Update GPIO pin assignment
    // Note: ESP-IDF v5 requires channel to be disabled before changing GPIO
    esp_err_t ret = rmt_disable(mChannel);
    if (ret != ESP_OK) {
        ESP_LOGW(RMT5_WORKER_TAG, "RmtWorker[%d]: Failed to disable channel for GPIO change: %d", mWorkerId, ret);
        return false;
    }

    // Use low-level API to change GPIO
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_matrix_out(pin, RMT_SIG_OUT0_IDX + mChannelId, false, false);

    ret = rmt_enable(mChannel);
    if (ret != ESP_OK) {
        ESP_LOGW(RMT5_WORKER_TAG, "RmtWorker[%d]: Failed to re-enable channel: %d", mWorkerId, ret);
        return false;
    }

    return true;
}

void RmtWorker::transmit(const uint8_t* pixel_data, int num_bytes) {
    FL_ASSERT(!mTransmitting, "RmtWorker::transmit called while already transmitting");
    FL_ASSERT(pixel_data != nullptr, "RmtWorker::transmit called with null pixel data");

    // Store pixel data pointer (not owned by worker)
    mPixelData = pixel_data;
    mNumBytes = num_bytes;

    // Debug: Log transmission start
    ESP_LOGI(RMT5_WORKER_TAG, "Worker[%d]: TX START - %d bytes (%d LEDs)",
             mWorkerId, num_bytes, num_bytes / 3);

    // Reset state
    mCur = 0;
    mWhichHalf = 0;
    mRMT_mem_ptr = mRMT_mem_start;
    mTransmitting = true;
    mAvailable = false;

    // Fill both halves initially (like RMT4)
    fillNextHalf();  // Fill half 0
    fillNextHalf();  // Fill half 1

    // Reset memory pointer and start transmission
    mWhichHalf = 0;
    mRMT_mem_ptr = mRMT_mem_start;

    ESP_LOGI(RMT5_WORKER_TAG, "Worker[%d]: Both halves filled, starting HW transmission", mWorkerId);

    tx_start();
}

void RmtWorker::waitForCompletion() {
    // Spin-wait for transmission to complete
    while (mTransmitting) {
        // Yield to FreeRTOS scheduler to prevent watchdog
        taskYIELD();
    }
}

// Convert byte to 8 RMT items (one per bit)
FASTLED_FORCE_INLINE void IRAM_ATTR RmtWorker::convertByteToRmt(
    uint8_t byte_val,
    volatile rmt_item32_t* out
) {
    // Use local registers for speed (pattern from RMT4)
    uint32_t zero_val = *reinterpret_cast<uint32_t*>(&mZero);
    uint32_t one_val = *reinterpret_cast<uint32_t*>(&mOne);

    uint32_t pixel_u32 = byte_val;
    pixel_u32 <<= 24;  // Shift to MSB position

    // Use temporary buffer to avoid volatile writes in loop
    uint32_t tmp[8];
    for (uint32_t j = 0; j < 8; j++) {
        tmp[j] = (pixel_u32 & 0x80000000UL) ? one_val : zero_val;
        pixel_u32 <<= 1;
    }

    // Write to volatile RMT memory
    out[0].val = tmp[0];
    out[1].val = tmp[1];
    out[2].val = tmp[2];
    out[3].val = tmp[3];
    out[4].val = tmp[4];
    out[5].val = tmp[5];
    out[6].val = tmp[6];
    out[7].val = tmp[7];
}

// Fill next half of RMT buffer (interrupt context)
void IRAM_ATTR RmtWorker::fillNextHalf() {
    volatile rmt_item32_t* pItem = mRMT_mem_ptr;
    uint8_t currentHalf = mWhichHalf;

    // Fill PULSES_PER_FILL / 8 bytes (since each byte = 8 pulses)
    for (fl::size i = 0; i < RmtWorker::PULSES_PER_FILL / 8; i++) {
        if (mCur < mNumBytes) {
            convertByteToRmt(mPixelData[mCur], pItem);
            pItem += 8;
            mCur++;
        } else {
            // End marker - zero duration signals end of transmission
            pItem->val = 0;
            pItem++;
        }
    }

    // Flip to other half
    uint8_t nextHalf = mWhichHalf + 1;
    if (nextHalf == 2) {
        pItem = mRMT_mem_start;
        nextHalf = 0;
    }
    mWhichHalf = nextHalf;

    mRMT_mem_ptr = pItem;

    // Debug logging - track buffer refills
    // Note: Use ets_printf for ISR-safe logging (ESP_LOG* may not be ISR-safe)
    // Only log if significantly more data remaining (avoid spam at end)
    if (mCur < mNumBytes - 16) {
        ets_printf("W%d: fillHalf=%d, byte=%d/%d\n", mWorkerId, currentHalf, mCur, mNumBytes);
    }
}

// Start RMT transmission
void IRAM_ATTR RmtWorker::tx_start() {
    // Use direct register access like RMT4
    // This is platform-specific and based on RMT4's approach
#if CONFIG_IDF_TARGET_ESP32
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
#elif CONFIG_IDF_TARGET_ESP32S3
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
#elif CONFIG_IDF_TARGET_ESP32C3
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
#elif CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
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

// RMT5 TX done callback (called from ISR context)
bool IRAM_ATTR RmtWorker::onTransDoneCallback(rmt_channel_handle_t channel, const rmt_tx_done_event_data_t *edata, void *user_data) {
    RmtWorker* worker = static_cast<RmtWorker*>(user_data);
    worker->handleDoneInterrupt();
    return false;  // Don't yield from ISR
}

// Global ISR handler (dispatches to instance handlers)
// NOTE: This is currently unused - using RMT5 high-level callbacks instead
void IRAM_ATTR RmtWorker::globalISR(void* arg) {
    RmtWorker* worker = static_cast<RmtWorker*>(arg);
    uint32_t intr_st = RMT.int_st.val;

    // Platform-specific bit positions (from RMT4)
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S3 || CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32C6 || CONFIG_IDF_TARGET_ESP32H2
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
        worker->handleDoneInterrupt();
        RMT.int_clr.val = (1 << tx_done_bit);
    }
}

// Handle threshold interrupt (refill next buffer half)
void IRAM_ATTR RmtWorker::handleThresholdInterrupt() {
    portENTER_CRITICAL_ISR(&sRmtSpinlock);

    // Debug: Track threshold interrupts
    static uint32_t thresholdCount = 0;
    thresholdCount++;
    if (thresholdCount % 10 == 0) {  // Log every 10th to reduce spam
        ets_printf("W%d: THRESHOLD_ISR #%d\n", mWorkerId, thresholdCount);
    }

    fillNextHalf();
    portEXIT_CRITICAL_ISR(&sRmtSpinlock);
}

// Handle done interrupt (transmission complete)
void IRAM_ATTR RmtWorker::handleDoneInterrupt() {
    portENTER_CRITICAL_ISR(&sRmtSpinlock);

    // Debug: Track transmission completion
    ets_printf("W%d: TX DONE - sent %d/%d bytes\n", mWorkerId, mCur, mNumBytes);

    mTransmitting = false;
    mAvailable = true;
    portEXIT_CRITICAL_ISR(&sRmtSpinlock);
}

// Extract channel ID from opaque handle
uint32_t RmtWorker::getChannelIdFromHandle(rmt_channel_handle_t handle) {
    // This relies on internal IDF structure
    // WARNING: May break on future IDF versions
    struct rmt_tx_channel_t {
        void* base;  // rmt_channel_t base
        uint32_t channel_id;
        // ... other fields we don't care about
    };

    rmt_tx_channel_t* tx_chan = reinterpret_cast<rmt_tx_channel_t*>(handle);
    return tx_chan->channel_id;
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32

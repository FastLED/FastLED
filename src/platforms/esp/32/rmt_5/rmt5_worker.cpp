#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "hal/rmt_ll.h"
#include "esp_log.h"

#ifdef __cplusplus
}
#endif

#include "fl/force_inline.h"
#include "fl/assert.h"
#include "fl/warn.h"

#define RMT5_WORKER_TAG "rmt5_worker"

namespace fl {

// Static spinlock for ISR synchronization
portMUX_TYPE RmtWorker::sRmtSpinlock = portMUX_INITIALIZER_UNLOCKED;

// Global reference to RMTMEM for direct memory access
extern rmt_block_mem_t RMTMEM;

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
        FL_WARN("RmtWorker[%d]: Failed to create RMT TX channel: %d", worker_id, ret);
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
        FL_WARN("RmtWorker[%d]: Failed to enable RMT channel: %d", worker_id, ret);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
        return false;
    }

    // Register custom ISR for threshold and done interrupts
    ret = esp_intr_alloc(
        ETS_RMT_INTR_SOURCE,
        ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_LEVEL3,
        &RmtWorker::globalISR,
        this,
        &mIntrHandle
    );

    if (ret != ESP_OK) {
        FL_WARN("RmtWorker[%d]: Failed to allocate ISR: %d", worker_id, ret);
        rmt_disable(mChannel);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
        return false;
    }

    // Enable threshold interrupt at 50% mark
    rmt_ll_enable_tx_thres_interrupt(&RMT, mChannelId, true);
    rmt_ll_set_tx_thres(&RMT, mChannelId, static_cast<uint32_t>(PULSES_PER_FILL));

    // Enable done interrupt
    rmt_ll_enable_tx_end_interrupt(&RMT, mChannelId, true);

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
        FL_WARN("RmtWorker[%d]: Failed to disable channel for GPIO change: %d", mWorkerId, ret);
        return false;
    }

    // Use low-level API to change GPIO
    rmt_ll_tx_reset_pointer(&RMT, mChannelId);
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    gpio_matrix_out(pin, RMT_SIG_OUT0_IDX + mChannelId, false, false);

    ret = rmt_enable(mChannel);
    if (ret != ESP_OK) {
        FL_WARN("RmtWorker[%d]: Failed to re-enable channel: %d", mWorkerId, ret);
        return false;
    }

    return true;
}

void RmtWorker::transmit(const uint8_t* pixel_data, fl::size num_bytes) {
    FL_ASSERT(!mTransmitting, "RmtWorker::transmit called while already transmitting");
    FL_ASSERT(pixel_data != nullptr, "RmtWorker::transmit called with null pixel data");

    // Store pixel data pointer (not owned by worker)
    mPixelData = pixel_data;
    mNumBytes = num_bytes;

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

    // Fill PULSES_PER_FILL / 8 bytes (since each byte = 8 pulses)
    for (fl::size i = 0; i < PULSES_PER_FILL / 8; i++) {
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
    mWhichHalf++;
    if (mWhichHalf == 2) {
        pItem = mRMT_mem_start;
        mWhichHalf = 0;
    }

    mRMT_mem_ptr = pItem;
}

// Start RMT transmission
void IRAM_ATTR RmtWorker::tx_start() {
    // Reset RMT memory read pointer
    rmt_ll_tx_reset_pointer(&RMT, mChannelId);

    // Set continuous mode (loop within allocated memory)
    rmt_ll_tx_enable_loop(&RMT, mChannelId, false);

    // Start transmission
    rmt_ll_tx_start(&RMT, mChannelId);
}

// Global ISR handler (dispatches to instance handlers)
void IRAM_ATTR RmtWorker::globalISR(void* arg) {
    RmtWorker* worker = static_cast<RmtWorker*>(arg);
    uint32_t intr_st = RMT.int_st.val;
    uint32_t channel_mask = (1 << worker->mChannelId);

    // Check threshold interrupt (buffer half empty)
    uint32_t thresh_bit = worker->mChannelId + 24;  // TX threshold bits start at bit 24
    if (intr_st & (1 << thresh_bit)) {
        worker->handleThresholdInterrupt();
        RMT.int_clr.val = (1 << thresh_bit);
    }

    // Check done interrupt (transmission complete)
    if (intr_st & channel_mask) {
        worker->handleDoneInterrupt();
        RMT.int_clr.val = channel_mask;
    }
}

// Handle threshold interrupt (refill next buffer half)
void IRAM_ATTR RmtWorker::handleThresholdInterrupt() {
    portENTER_CRITICAL_ISR(&sRmtSpinlock);
    fillNextHalf();
    portEXIT_CRITICAL_ISR(&sRmtSpinlock);
}

// Handle done interrupt (transmission complete)
void IRAM_ATTR RmtWorker::handleDoneInterrupt() {
    portENTER_CRITICAL_ISR(&sRmtSpinlock);
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

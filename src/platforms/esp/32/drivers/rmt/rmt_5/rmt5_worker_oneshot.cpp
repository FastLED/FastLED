#ifdef ESP32

#include "fl/compiler_control.h"

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker_oneshot.h"

FL_EXTERN_C_BEGIN

#include "esp32-hal.h"
#include "driver/gpio.h"
#include "soc/soc.h"
#include "soc/rmt_struct.h"
#include "rom/gpio.h"
#include "rom/ets_sys.h"  // For ets_printf (ISR-safe logging)
#include "esp_log.h"

FL_EXTERN_C_END

#include "fl/force_inline.h"
#include "fl/assert.h"
#include "fl/log.h"

#define RMT5_ONESHOT_TAG "rmt5_oneshot"

namespace fl {

RmtWorkerOneShot::RmtWorkerOneShot(portMUX_TYPE* pool_spinlock)
    : mChannel(nullptr)
    , mEncoder(nullptr)
    , mChannelId(0)
    , mWorkerId(0)
    , mCurrentPin(GPIO_NUM_NC)
    , mT1(0)
    , mT2(0)
    , mT3(0)
    , mResetNs(0)
    , mEncodedSymbols(nullptr)
    , mEncodedCapacity(0)
    , mEncodedSize(0)
    , mAvailable(true)
    , mTransmitting(false)
    , mCompletionSemaphore(nullptr)
    , mPoolSpinlock(pool_spinlock)
{
    // Create binary semaphore for completion signaling
    mCompletionSemaphore = xSemaphoreCreateBinary();
    FL_ASSERT(mCompletionSemaphore != nullptr, "Failed to create completion semaphore");
    // Initialize symbols to safe defaults
    mZero.duration0 = 0;
    mZero.level0 = 0;
    mZero.duration1 = 0;
    mZero.level1 = 0;

    mOne.duration0 = 0;
    mOne.level0 = 0;
    mOne.duration1 = 0;
    mOne.level1 = 0;

    mReset.duration0 = 0;
    mReset.level0 = 0;
    mReset.duration1 = 0;
    mReset.level1 = 0;
}

RmtWorkerOneShot::~RmtWorkerOneShot() {
    // Delete semaphore
    if (mCompletionSemaphore) {
        vSemaphoreDelete(mCompletionSemaphore);
        mCompletionSemaphore = nullptr;
    }

    // Free pre-encoded symbol buffer
    if (mEncodedSymbols) {
        free(mEncodedSymbols);
        mEncodedSymbols = nullptr;
        mEncodedCapacity = 0;
    }

    // Clean up encoder
    if (mEncoder) {
        rmt_del_encoder(mEncoder);
        mEncoder = nullptr;
    }

    // Clean up channel
    if (mChannel) {
        rmt_disable(mChannel);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
    }
}

bool RmtWorkerOneShot::initialize(uint8_t worker_id) {
    mWorkerId = worker_id;
    mAvailable = true;

    // Channel creation is deferred to configure() where we know the actual GPIO pin.
    // This avoids needing placeholder GPIOs and is safe for static initialization.
    FL_LOG_RMT("OneShot[" << (int)worker_id << "]: Initialized (channel creation deferred to first configure)");

    return true;
}

bool RmtWorkerOneShot::createChannel(gpio_num_t pin) {
    FL_LOG_RMT("OneShot[" << (int)mWorkerId << "]: Creating RMT TX channel for GPIO " << (int)pin);

    // Create RMT TX channel (no double-buffer needed for one-shot)
    rmt_tx_channel_config_t tx_config = {};
    tx_config.gpio_num = pin;
    tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_config.resolution_hz = 10000000;  // 10MHz (100ns resolution)
    tx_config.mem_block_symbols = SOC_RMT_MEM_WORDS_PER_CHANNEL;  // Single block (no double-buffer)
    tx_config.trans_queue_depth = 1;
    tx_config.flags.invert_out = false;
    tx_config.flags.with_dma = false;

    esp_err_t ret = rmt_new_tx_channel(&tx_config, &mChannel);
    if (ret != ESP_OK) {
        FL_WARN("OneShot[" << (int)mWorkerId << "]: Failed to create RMT TX channel: " << ret);
        return false;
    }

    // Extract channel ID
    mChannelId = getChannelIdFromHandle(mChannel);

    // Create bytes encoder for converting pixel data to RMT symbols
    // Note: Encoder symbols are placeholders - will be updated in configure()
    rmt_bytes_encoder_config_t encoder_config = {};
    encoder_config.bit0.duration0 = 4;
    encoder_config.bit0.level0 = 1;
    encoder_config.bit0.duration1 = 8;
    encoder_config.bit0.level1 = 0;
    encoder_config.bit1.duration0 = 8;
    encoder_config.bit1.level0 = 1;
    encoder_config.bit1.duration1 = 4;
    encoder_config.bit1.level1 = 0;
    encoder_config.flags.msb_first = 1;  // WS2812B uses MSB first

    ret = rmt_new_bytes_encoder(&encoder_config, &mEncoder);
    if (ret != ESP_OK) {
        FL_WARN("OneShot[" << (int)mWorkerId << "]: Failed to create bytes encoder: " << ret);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
        return false;
    }

    // Register completion callback
    rmt_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_done = &RmtWorkerOneShot::onTransDoneCallback;
    ret = rmt_tx_register_event_callbacks(mChannel, &callbacks, this);
    if (ret != ESP_OK) {
        FL_WARN("OneShot[" << (int)mWorkerId << "]: Failed to register callbacks: " << ret);
        rmt_del_encoder(mEncoder);
        rmt_del_channel(mChannel);
        mEncoder = nullptr;
        mChannel = nullptr;
        return false;
    }

    FL_LOG_RMT("OneShot[" << (int)mWorkerId << "]: Channel created successfully");
    return true;
}

bool RmtWorkerOneShot::configure(gpio_num_t pin, const ChipsetTiming& TIMING, uint32_t reset_ns) {
    // Extract timing values from struct
    uint32_t t1 = TIMING.T1;
    uint32_t t2 = TIMING.T2;
    uint32_t t3 = TIMING.T3;

    // Create channel on first configure
    if (mChannel == nullptr) {
        if (!createChannel(pin)) {
            return false;
        }
    }

    // Check if reconfiguration needed
    if (mCurrentPin == pin && mT1 == t1 && mT2 == t2 && mT3 == t3 && mResetNs == reset_ns) {
        return true;  // Already configured
    }

    // Wait for active transmission
    if (mTransmitting) {
        waitForCompletion();
    }

    // Save old pin before updating (needed to check if channel needs disable)
    gpio_num_t old_pin = mCurrentPin;

    // Update configuration
    mCurrentPin = pin;
    mT1 = t1;
    mT2 = t2;
    mT3 = t3;
    mResetNs = reset_ns;

    // Convert from ns to ticks (10MHz = 1 tick = 100ns)
    auto ns_to_ticks = [](uint32_t ns) -> uint16_t {
        return static_cast<uint16_t>((ns + 50) / 100);  // Round to nearest tick
    };

    // Calculate RMT symbols for 0 and 1 bits
    mZero.level0 = 1;
    mZero.duration0 = ns_to_ticks(t1);
    mZero.level1 = 0;
    mZero.duration1 = ns_to_ticks(t2 + t3);

    mOne.level0 = 1;
    mOne.duration0 = ns_to_ticks(t1 + t2);
    mOne.level1 = 0;
    mOne.duration1 = ns_to_ticks(t3);

    // Reset symbol (zero duration = end marker)
    mReset.duration0 = 0;
    mReset.level0 = 0;
    mReset.duration1 = 0;
    mReset.level1 = 0;

    // Update GPIO pin assignment
    // Disable channel if it's currently enabled (not on first configure)
    if (old_pin != GPIO_NUM_NC) {
        esp_err_t ret = rmt_disable(mChannel);
        if (ret != ESP_OK) {
            FL_WARN("OneShot[" << (int)mWorkerId << "]: Failed to disable channel for GPIO change: " << ret);
            return false;
        }
    }

    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
    // ESP32-P4 uses different signal index naming (RMT_SIG_PAD_OUT0_IDX vs RMT_SIG_OUT0_IDX)
    #if defined(RMT_SIG_PAD_OUT0_IDX)
        gpio_matrix_out(pin, RMT_SIG_PAD_OUT0_IDX + mChannelId, false, false);
    #elif defined(RMT_SIG_OUT0_IDX)
        gpio_matrix_out(pin, RMT_SIG_OUT0_IDX + mChannelId, false, false);
    #else
        #error "Neither RMT_SIG_OUT0_IDX nor RMT_SIG_PAD_OUT0_IDX is defined"
    #endif

    esp_err_t ret = rmt_enable(mChannel);
    if (ret != ESP_OK) {
        FL_WARN("OneShot[" << (int)mWorkerId << "]: Failed to enable channel: " << ret);
        return false;
    }

    return true;
}

void RmtWorkerOneShot::preEncode(const uint8_t* pixel_data, int num_bytes) {
    // Safety: Check for integer overflow before multiplication
    const size_t max_bytes = (SIZE_MAX - 1) / 8;  // Maximum bytes before overflow
    if (static_cast<size_t>(num_bytes) > max_bytes) {
        FL_WARN("OneShot[" << (int)mWorkerId << "]: num_bytes (" << num_bytes << ") too large, would overflow");
        mEncodedSize = 0;
        return;
    }

    // Calculate required symbols (8 per byte + 1 reset)
    const size_t num_symbols = static_cast<size_t>(num_bytes) * 8 + 1;

    // Allocate exact size needed (LED strips don't grow, size is fixed at init)
    if (mEncodedCapacity < num_symbols) {
        FL_LOG_RMT("OneShot[" << (int)mWorkerId << "]: Resizing buffer " << mEncodedCapacity << " -> " << num_symbols
                   << " symbols (" << ((num_symbols * sizeof(rmt_item32_t)) / 1024.0f) << "KB)");

        // Try realloc first (more efficient if possible)
        rmt_item32_t* new_buffer = static_cast<rmt_item32_t*>(
            realloc(mEncodedSymbols, num_symbols * sizeof(rmt_item32_t))
        );

        if (!new_buffer) {
            // Realloc failed - free old buffer and try malloc
            FL_WARN("OneShot[" << (int)mWorkerId << "]: realloc failed, trying malloc");
            free(mEncodedSymbols);
            mEncodedSymbols = static_cast<rmt_item32_t*>(malloc(num_symbols * sizeof(rmt_item32_t)));

            if (!mEncodedSymbols) {
                FL_WARN("OneShot[" << (int)mWorkerId << "]: Failed to allocate " << num_symbols << " symbols ("
                        << ((num_symbols * sizeof(rmt_item32_t)) / 1024.0f) << "KB)");
                mEncodedCapacity = 0;
                mEncodedSize = 0;
                return;
            }
        } else {
            mEncodedSymbols = new_buffer;
        }

        mEncodedCapacity = num_symbols;
    }

    // Pre-encode all bytes to RMT symbols
    rmt_item32_t* out = mEncodedSymbols;
    for (int i = 0; i < num_bytes; i++) {
        convertByteToRmt(pixel_data[i], out);
        out += 8;
    }

    // Add reset/end marker
    out->val = 0;

    mEncodedSize = num_symbols;

    FL_LOG_RMT("OneShot[" << (int)mWorkerId << "]: Pre-encoded " << num_bytes << " bytes -> " << num_symbols
               << " symbols (" << ((num_symbols * sizeof(rmt_item32_t)) / 1024.0f) << "KB)");
}

void RmtWorkerOneShot::transmit(const uint8_t* pixel_data, int num_bytes) {
    FL_ASSERT(!mTransmitting, "RmtWorkerOneShot::transmit called while already transmitting");
    FL_ASSERT(pixel_data != nullptr, "RmtWorkerOneShot::transmit called with null pixel data");

    // Safety check in case FL_ASSERT is compiled out
    if (pixel_data == nullptr || mTransmitting) {
        FL_WARN("OneShot[" << (int)mWorkerId << "]: Invalid transmit state - pixel_data=" << (void*)pixel_data
                << ", transmitting=" << mTransmitting.load());
        return;
    }

    FL_LOG_RMT("OneShot[" << (int)mWorkerId << "]: TX START - " << num_bytes << " bytes (" << (num_bytes / 3) << " LEDs)");

    // Pre-encode entire strip to RMT symbols
    preEncode(pixel_data, num_bytes);

    if (!mEncodedSymbols || mEncodedSize == 0) {
        FL_WARN("OneShot[" << (int)mWorkerId << "]: Pre-encoding failed, aborting transmission");
        return;
    }

    // Set transmission flag (atomic for ISR visibility)
    mTransmitting.store(true, fl::memory_order_release);

    // mAvailable is set false by pool under spinlock, not here

    // One-shot transmission configuration
    rmt_transmit_config_t tx_config = {};
    tx_config.loop_count = 0;  // No loop
    tx_config.flags.eot_level = 0;

    // Transmit pre-encoded buffer (fire-and-forget)
    esp_err_t ret = rmt_transmit(
        mChannel,
        mEncoder,
        mEncodedSymbols,
        mEncodedSize * sizeof(rmt_item32_t),
        &tx_config
    );

    if (ret != ESP_OK) {
        FL_WARN("OneShot[" << (int)mWorkerId << "]: rmt_transmit failed: " << ret);
        mTransmitting.store(false, fl::memory_order_release);
        // Don't modify mAvailable here - pool owns that state
        return;
    }

    FL_LOG_RMT("OneShot[" << (int)mWorkerId << "]: Transmission started (" << mEncodedSize << " symbols)");
}

void RmtWorkerOneShot::waitForCompletion() {
    // Block on semaphore until ISR signals completion
    if (mTransmitting.load(fl::memory_order_acquire)) {
        xSemaphoreTake(mCompletionSemaphore, portMAX_DELAY);
    }
}

void RmtWorkerOneShot::markAsAvailable() {
    // Called by pool under spinlock to mark worker as available
    mAvailable = true;
}

void RmtWorkerOneShot::markAsUnavailable() {
    // Called by pool under spinlock to mark worker as unavailable
    mAvailable = false;
}

// Convert byte to 8 RMT items (one per bit)
FASTLED_FORCE_INLINE void RmtWorkerOneShot::convertByteToRmt(
    uint8_t byte_val,
    rmt_item32_t* out
) {
    // Use local registers for speed
    uint32_t zero_val = *reinterpret_cast<uint32_t*>(&mZero);
    uint32_t one_val = *reinterpret_cast<uint32_t*>(&mOne);

    uint32_t pixel_u32 = byte_val;
    pixel_u32 <<= 24;  // Shift to MSB position

    // Unrolled loop for performance
    out[0].val = (pixel_u32 & 0x80000000UL) ? one_val : zero_val;
    pixel_u32 <<= 1;
    out[1].val = (pixel_u32 & 0x80000000UL) ? one_val : zero_val;
    pixel_u32 <<= 1;
    out[2].val = (pixel_u32 & 0x80000000UL) ? one_val : zero_val;
    pixel_u32 <<= 1;
    out[3].val = (pixel_u32 & 0x80000000UL) ? one_val : zero_val;
    pixel_u32 <<= 1;
    out[4].val = (pixel_u32 & 0x80000000UL) ? one_val : zero_val;
    pixel_u32 <<= 1;
    out[5].val = (pixel_u32 & 0x80000000UL) ? one_val : zero_val;
    pixel_u32 <<= 1;
    out[6].val = (pixel_u32 & 0x80000000UL) ? one_val : zero_val;
    pixel_u32 <<= 1;
    out[7].val = (pixel_u32 & 0x80000000UL) ? one_val : zero_val;
}

// Transmission completion callback (ISR context)
bool IRAM_ATTR RmtWorkerOneShot::onTransDoneCallback(
    rmt_channel_handle_t channel,
    const rmt_tx_done_event_data_t *edata,
    void *user_data
) {
    RmtWorkerOneShot* worker = static_cast<RmtWorkerOneShot*>(user_data);

    // Use ISR-safe logging (ESP_LOGI uses mutexes, not safe in ISR)
    ets_printf("OneShot[%d]: TX DONE\n", worker->mWorkerId);

    // Clear transmission flag (atomic for cross-core visibility)
    worker->mTransmitting.store(false, fl::memory_order_release);

    // Signal completion to waiting thread
    portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(worker->mCompletionSemaphore, &xHigherPriorityTaskWoken);

    // DO NOT set mAvailable = true here!
    // Pool will do this under spinlock when releaseWorker() is called

    return (xHigherPriorityTaskWoken == pdTRUE);  // Yield if needed
}

// Extract channel ID from opaque handle (same as double-buffer worker)
uint32_t RmtWorkerOneShot::getChannelIdFromHandle(rmt_channel_handle_t handle) {
    // SAFETY WARNING: This relies on internal ESP-IDF structure layout
    // See detailed comments in RmtWorker::getChannelIdFromHandle()

    if (handle == nullptr) {
        FL_WARN("getChannelIdFromHandle: null handle");
        return 0;
    }

    struct rmt_tx_channel_t {
        void* base;  // rmt_channel_t base (offset 0)
        uint32_t channel_id;  // offset sizeof(void*)
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

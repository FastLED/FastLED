#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "rmt5_worker_oneshot.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "esp32-hal.h"
#include "driver/gpio.h"
#include "soc/soc.h"
#include "soc/rmt_struct.h"
#include "rom/gpio.h"
#include "esp_log.h"

#ifdef __cplusplus
}
#endif

#include "fl/force_inline.h"
#include "fl/assert.h"

#define RMT5_ONESHOT_TAG "rmt5_oneshot"

namespace fl {

RmtWorkerOneShot::RmtWorkerOneShot()
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
{
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
    ESP_LOGI(RMT5_ONESHOT_TAG, "OneShot[%d]: Initialized (channel creation deferred to first configure)", worker_id);

    return true;
}

bool RmtWorkerOneShot::createChannel(gpio_num_t pin) {
    ESP_LOGI(RMT5_ONESHOT_TAG, "OneShot[%d]: Creating RMT TX channel for GPIO %d", mWorkerId, (int)pin);

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
        ESP_LOGE(RMT5_ONESHOT_TAG, "OneShot[%d]: Failed to create RMT TX channel: %d", mWorkerId, ret);
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
        ESP_LOGE(RMT5_ONESHOT_TAG, "OneShot[%d]: Failed to create bytes encoder: %d", mWorkerId, ret);
        rmt_del_channel(mChannel);
        mChannel = nullptr;
        return false;
    }

    // Register completion callback
    rmt_tx_event_callbacks_t callbacks = {};
    callbacks.on_trans_done = &RmtWorkerOneShot::onTransDoneCallback;
    ret = rmt_tx_register_event_callbacks(mChannel, &callbacks, this);
    if (ret != ESP_OK) {
        ESP_LOGE(RMT5_ONESHOT_TAG, "OneShot[%d]: Failed to register callbacks: %d", mWorkerId, ret);
        rmt_del_encoder(mEncoder);
        rmt_del_channel(mChannel);
        mEncoder = nullptr;
        mChannel = nullptr;
        return false;
    }

    ESP_LOGI(RMT5_ONESHOT_TAG, "OneShot[%d]: Channel created successfully", mWorkerId);
    return true;
}

bool RmtWorkerOneShot::configure(gpio_num_t pin, int t1, int t2, int t3, uint32_t reset_ns) {
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
            ESP_LOGW(RMT5_ONESHOT_TAG, "OneShot[%d]: Failed to disable channel for GPIO change: %d", mWorkerId, ret);
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
        ESP_LOGW(RMT5_ONESHOT_TAG, "OneShot[%d]: Failed to enable channel: %d", mWorkerId, ret);
        return false;
    }

    return true;
}

void RmtWorkerOneShot::preEncode(const uint8_t* pixel_data, int num_bytes) {
    // Calculate required symbols (8 per byte + 1 reset)
    const size_t num_symbols = static_cast<size_t>(num_bytes) * 8 + 1;

    // Allocate exact size needed (LED strips don't grow, size is fixed at init)
    if (mEncodedCapacity < num_symbols) {
        ESP_LOGI(RMT5_ONESHOT_TAG, "OneShot[%d]: Resizing buffer %zu -> %zu symbols (%.1fKB)",
                 mWorkerId, mEncodedCapacity, num_symbols, (num_symbols * sizeof(rmt_item32_t)) / 1024.0f);

        // Try realloc first (more efficient if possible)
        rmt_item32_t* new_buffer = static_cast<rmt_item32_t*>(
            realloc(mEncodedSymbols, num_symbols * sizeof(rmt_item32_t))
        );

        if (!new_buffer) {
            // Realloc failed - free old buffer and try malloc
            ESP_LOGW(RMT5_ONESHOT_TAG, "OneShot[%d]: realloc failed, trying malloc", mWorkerId);
            free(mEncodedSymbols);
            mEncodedSymbols = static_cast<rmt_item32_t*>(malloc(num_symbols * sizeof(rmt_item32_t)));

            if (!mEncodedSymbols) {
                ESP_LOGE(RMT5_ONESHOT_TAG, "OneShot[%d]: Failed to allocate %zu symbols (%.1fKB)",
                         mWorkerId, num_symbols, (num_symbols * sizeof(rmt_item32_t)) / 1024.0f);
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

    ESP_LOGD(RMT5_ONESHOT_TAG, "OneShot[%d]: Pre-encoded %d bytes -> %zu symbols (%.1fKB)",
             mWorkerId, num_bytes, num_symbols, (num_symbols * sizeof(rmt_item32_t)) / 1024.0f);
}

void RmtWorkerOneShot::transmit(const uint8_t* pixel_data, int num_bytes) {
    FL_ASSERT(!mTransmitting, "RmtWorkerOneShot::transmit called while already transmitting");
    FL_ASSERT(pixel_data != nullptr, "RmtWorkerOneShot::transmit called with null pixel data");

    ESP_LOGI(RMT5_ONESHOT_TAG, "OneShot[%d]: TX START - %d bytes (%d LEDs)",
             mWorkerId, num_bytes, num_bytes / 3);

    // Pre-encode entire strip to RMT symbols
    preEncode(pixel_data, num_bytes);

    if (!mEncodedSymbols || mEncodedSize == 0) {
        ESP_LOGE(RMT5_ONESHOT_TAG, "OneShot[%d]: Pre-encoding failed, aborting transmission", mWorkerId);
        return;
    }

    mTransmitting = true;
    mAvailable = false;

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
        ESP_LOGE(RMT5_ONESHOT_TAG, "OneShot[%d]: rmt_transmit failed: %d", mWorkerId, ret);
        mTransmitting = false;
        mAvailable = true;
        return;
    }

    ESP_LOGI(RMT5_ONESHOT_TAG, "OneShot[%d]: Transmission started (%zu symbols)",
             mWorkerId, mEncodedSize);
}

void RmtWorkerOneShot::waitForCompletion() {
    // Spin-wait for transmission to complete
    while (mTransmitting) {
        taskYIELD();  // Yield to FreeRTOS scheduler
    }
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

    ESP_LOGI(RMT5_ONESHOT_TAG, "OneShot[%d]: TX DONE", worker->mWorkerId);

    worker->mTransmitting = false;
    worker->mAvailable = true;

    return false;  // Don't yield from ISR
}

// Extract channel ID from opaque handle (same as double-buffer worker)
uint32_t RmtWorkerOneShot::getChannelIdFromHandle(rmt_channel_handle_t handle) {
    struct rmt_tx_channel_t {
        void* base;
        uint32_t channel_id;
    };

    rmt_tx_channel_t* tx_chan = reinterpret_cast<rmt_tx_channel_t*>(handle);
    return tx_chan->channel_id;
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32



#pragma once

#include <driver/gpio.h>
#include <driver/i2s_std.h>

#include "fl/assert.h"
#include "fl/int.h"
#include "fl/sstream.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/audio_input.h"

#define I2S_INTR_ALLOC_FLAGS 0

namespace fl {
namespace esp_i2s {

typedef i16 audio_sample_t;
typedef i16 dma_buffer_t[I2S_AUDIO_BUFFER_LEN];

struct I2SContext {
    i2s_chan_handle_t rx_handle;
    i2s_std_config_t std_config;
};

I2SContext make_context(const AudioConfigI2S &config) {
    auto detect_slot_mode = [](AudioChannel value) -> i2s_slot_mode_t {
        switch (value) {
        case Left:
            return I2S_SLOT_MODE_MONO;
        case Right:
            return I2S_SLOT_MODE_MONO;
        case Both:
            return I2S_SLOT_MODE_STEREO;
        }
        FL_ASSERT(false, "Invalid mic channel");
        return I2S_SLOT_MODE_STEREO;
    };

    auto detect_slot_mask = [](AudioChannel value) -> i2s_std_slot_mask_t {
        switch (value) {
        case Left:
            return I2S_STD_SLOT_LEFT;
        case Right:
            return I2S_STD_SLOT_RIGHT;
        case Both:
            return I2S_STD_SLOT_BOTH;
        }
        FL_ASSERT(false, "Invalid mic channel");
        return I2S_STD_SLOT_BOTH;
    };

    auto convert_bit_width = [](int bit_resolution) -> i2s_data_bit_width_t {
        switch (bit_resolution) {
        case 8:
            return I2S_DATA_BIT_WIDTH_8BIT;
        case 16:
            return I2S_DATA_BIT_WIDTH_16BIT;
        case 24:
            return I2S_DATA_BIT_WIDTH_24BIT;
        case 32:
            return I2S_DATA_BIT_WIDTH_32BIT;
        default:
            return I2S_DATA_BIT_WIDTH_16BIT;
        }
    };

    int pin_clk = config.mPinClk;
    int pin_ws = config.mPinWs;
    int pin_sd = config.mPinSd;
    u16 sample_rate = config.mSampleRate;
    auto slot_mode = detect_slot_mode(config.mAudioChannel);
    auto bit_width = convert_bit_width(config.mBitResolution);
    auto slot_mask = detect_slot_mask(config.mAudioChannel);

    // Create standard I2S configuration
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg =
#if SOC_I2S_HW_VERSION_1
            {
                .data_bit_width = bit_width,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
                .slot_mode = slot_mode,
                .slot_mask = slot_mask,
                .ws_width = 32,
                .ws_pol = false,
                .bit_shift = true,
                .msb_right = false,
            },
#else
            {
                .data_bit_width = bit_width,
                .slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT,
                .slot_mode = slot_mode,
                .slot_mask = slot_mask,
                .ws_width = 32,
                .ws_pol = false,
                .bit_shift = true,
                .left_align = true,
                .big_endian = false,
                .bit_order_lsb = false,
            },
#endif
        .gpio_cfg = {.bclk = static_cast<gpio_num_t>(pin_clk),
                     .ws = static_cast<gpio_num_t>(pin_ws),
                     .dout = GPIO_NUM_NC,
                     .din = static_cast<gpio_num_t>(pin_sd)}};

    I2SContext out = {nullptr, std_cfg};
    return out;
}

I2SContext i2s_audio_init(const AudioConfigI2S &config) {
    I2SContext ctx = make_context(config);

    // Create I2S channel configuration with DMA buffer settings
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        static_cast<i2s_port_t>(config.mI2sNum), I2S_ROLE_MASTER);
    chan_cfg.dma_desc_num = AUDIO_DMA_BUFFER_COUNT;
    chan_cfg.dma_frame_num = I2S_AUDIO_BUFFER_LEN;

    // Create RX channel
    esp_err_t ret = i2s_new_channel(&chan_cfg, NULL, &ctx.rx_handle);
    FL_ASSERT(ret == ESP_OK, "Failed to create I2S channel");

    // Initialize channel with standard mode configuration
    ret = i2s_channel_init_std_mode(ctx.rx_handle, &ctx.std_config);
    FL_ASSERT(ret == ESP_OK, "Failed to initialize I2S channel");

    // Enable the channel
    ret = i2s_channel_enable(ctx.rx_handle);
    FL_ASSERT(ret == ESP_OK, "Failed to enable I2S channel");

    return ctx;
}

size_t i2s_read_raw_samples(const I2SContext &ctx,
                            audio_sample_t (&buffer)[I2S_AUDIO_BUFFER_LEN]) {
    size_t bytes_read = 0;

    esp_err_t result =
        i2s_channel_read(ctx.rx_handle, buffer, sizeof(buffer), &bytes_read, 0);
    if (result == ESP_OK) {
        if (bytes_read > 0) {
            // cout << "Bytes read: " << bytes_read << endl;
            const size_t count = bytes_read / sizeof(audio_sample_t);
            return count;
        }
    }
    return 0;
}

void i2s_audio_destroy(const I2SContext &ctx) {
    if (ctx.rx_handle != nullptr) {
        // Disable the channel first
        i2s_channel_disable(ctx.rx_handle);
        // Then delete the channel
        i2s_del_channel(ctx.rx_handle);
    }
}

} // namespace esp_i2s

} // namespace fl

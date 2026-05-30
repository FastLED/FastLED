

#pragma once

// IWYU pragma: private

// IWYU pragma: begin_keep
#include <driver/gpio.h>
#include <driver/i2s_pdm.h>
// IWYU pragma: end_keep

#include "fl/audio/audio_input.h"
#include "fl/stl/assert.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/log/log.h"
#include "platforms/esp/esp_version.h"

namespace fl {
namespace esp_pdm {

typedef i16 audio_sample_t;
typedef i16 dma_buffer_t[I2S_AUDIO_BUFFER_LEN];

struct PDMContext {
    i2s_chan_handle_t rx_handle;
    bool valid;
};

PDMContext pdm_audio_init(const audio::ConfigPdm &config) FL_NOEXCEPT {
    int pin_clk = config.mPinClk;
    int pin_din = config.mPinDin;
    u16 sample_rate = config.mSampleRate;

    // Create I2S channel configuration with DMA buffer settings
#if ESP_IDF_VERSION_6_OR_HIGHER
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        config.mI2sNum, I2S_ROLE_MASTER);
#else
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(
        static_cast<i2s_port_t>(config.mI2sNum), I2S_ROLE_MASTER);
#endif
    chan_cfg.dma_desc_num = AUDIO_DMA_BUFFER_COUNT;
    chan_cfg.dma_frame_num = I2S_AUDIO_BUFFER_LEN;

    PDMContext ctx = {nullptr, false};

    // Create RX channel
    esp_err_t ret = i2s_new_channel(&chan_cfg, nullptr, &ctx.rx_handle);
    if (ret != ESP_OK) {
        FL_WARN("Failed to create PDM I2S channel: " << ret);
        return ctx;
    }

    // Configure PDM RX mode
    i2s_pdm_rx_config_t pdm_rx_cfg = {
        .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(sample_rate),
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                   I2S_SLOT_MODE_MONO),
        .gpio_cfg =
            {
                .clk = static_cast<gpio_num_t>(pin_clk),
                .din = static_cast<gpio_num_t>(pin_din),
                .invert_flags =
                    {
                        .clk_inv = false,
                    },
            },
    };

    // Initialize channel with PDM RX mode configuration
    ret = i2s_channel_init_pdm_rx_mode(ctx.rx_handle, &pdm_rx_cfg);
    if (ret != ESP_OK) {
        FL_WARN("Failed to initialize PDM RX mode: " << ret);
        i2s_del_channel(ctx.rx_handle);
        ctx.rx_handle = nullptr;
        return ctx;
    }

    // Enable the channel
    ret = i2s_channel_enable(ctx.rx_handle);
    if (ret != ESP_OK) {
        FL_WARN("Failed to enable PDM channel: " << ret);
        i2s_del_channel(ctx.rx_handle);
        ctx.rx_handle = nullptr;
        return ctx;
    }

    ctx.valid = true;
    return ctx;
}

size_t pdm_read_raw_samples(const PDMContext &ctx,
                            audio_sample_t (&buffer)[I2S_AUDIO_BUFFER_LEN])
    FL_NOEXCEPT {
    size_t bytes_read = 0;

    esp_err_t result =
        i2s_channel_read(ctx.rx_handle, buffer, sizeof(buffer), &bytes_read, 0);
    if (result == ESP_OK) {
        if (bytes_read > 0) {
            const size_t count = bytes_read / sizeof(audio_sample_t);
            return count;
        }
    }
    return 0;
}

void pdm_audio_destroy(const PDMContext &ctx) FL_NOEXCEPT {
    if (ctx.valid && ctx.rx_handle != nullptr) {
        i2s_channel_disable(ctx.rx_handle);
        i2s_del_channel(ctx.rx_handle);
    }
}

} // namespace esp_pdm

} // namespace fl

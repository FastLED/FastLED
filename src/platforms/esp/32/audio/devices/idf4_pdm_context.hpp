

#pragma once

// IWYU pragma: private

// IWYU pragma: begin_keep
#include <driver/gpio.h>
#include <driver/i2s.h>
// IWYU pragma: end_keep

#include "fl/audio/audio_input.h"
#include "fl/stl/assert.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/log/log.h"

namespace fl {
namespace esp_pdm {

typedef i16 audio_sample_t;
typedef i16 dma_buffer_t[I2S_AUDIO_BUFFER_LEN];

struct PDMContext {
    i2s_port_t i2s_port;
    bool valid;
};

PDMContext pdm_audio_init(const audio::ConfigPdm &config) FL_NOEXCEPT {
    int pin_clk = config.mPinClk;
    int pin_din = config.mPinDin;
    u16 sample_rate = config.mSampleRate;
    i2s_port_t i2s_port = static_cast<i2s_port_t>(config.mI2sNum);

    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
        .sample_rate = sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = AUDIO_DMA_BUFFER_COUNT,
        .dma_buf_len = I2S_AUDIO_BUFFER_LEN,
    };

    // PDM only uses CLK and DIN pins (no WS/BCLK separation)
    const i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_PIN_NO_CHANGE,
        .ws_io_num = pin_clk,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = pin_din,
    };

    PDMContext ctx = {i2s_port, false};

    esp_err_t ret = i2s_driver_install(i2s_port, &i2s_config, 0, nullptr);
    if (ret != ESP_OK) {
        FL_WARN("Failed to install PDM I2S driver: " << ret);
        return ctx;
    }
    ret = i2s_set_pin(i2s_port, &pin_config);
    if (ret != ESP_OK) {
        FL_WARN("Failed to set PDM I2S pins: " << ret);
        i2s_driver_uninstall(i2s_port);
        return ctx;
    }
    i2s_zero_dma_buffer(i2s_port);

    ctx.valid = true;
    return ctx;
}

size_t pdm_read_raw_samples(const PDMContext &ctx,
                            audio_sample_t (&buffer)[I2S_AUDIO_BUFFER_LEN])
    FL_NOEXCEPT {
    size_t bytes_read = 0;

    esp_err_t result =
        i2s_read(ctx.i2s_port, buffer, sizeof(buffer), &bytes_read, 0);
    if (result == ESP_OK) {
        if (bytes_read > 0) {
            const size_t count = bytes_read / sizeof(audio_sample_t);
            return count;
        }
    }
    return 0;
}

void pdm_audio_destroy(const PDMContext &ctx) FL_NOEXCEPT {
    if (ctx.valid) {
        i2s_driver_uninstall(ctx.i2s_port);
    }
}

} // namespace esp_pdm

} // namespace fl

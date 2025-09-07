

#pragma once

#include <driver/i2s.h>
#include <driver/gpio.h>

#include "fl/assert.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/int.h"
#include "fl/sstream.h"
#include "fl/audio_input.h"


#define I2S_INTR_ALLOC_FLAGS 0

namespace fl {
namespace esp_i2s {

typedef i16 audio_sample_t;
typedef i16 dma_buffer_t[I2S_AUDIO_BUFFER_LEN];

struct I2SContext {
    i2s_config_t i2s_config;
    i2s_pin_config_t pin_config;
    i2s_port_t i2s_port;
};

I2SContext make_context(const AudioConfigI2S &config) {
    auto convert_channel = [](AudioChannel value) -> int {
        switch (value) {
        case Left:
            return I2S_CHANNEL_FMT_ONLY_LEFT;
        case Right:
            return I2S_CHANNEL_FMT_ONLY_RIGHT;
        case Both:
            return I2S_CHANNEL_FMT_RIGHT_LEFT;
        }
        FL_ASSERT(false, "Invalid mic channel");
        return 0;
    };

    auto convert_comm_format = [](I2SCommFormat value) -> int {
        switch (value) {
        case Philips:
            return I2S_COMM_FORMAT_STAND_I2S;
        case MSB:
            return I2S_COMM_FORMAT_STAND_MSB;
        case PCMShort:
            return I2S_COMM_FORMAT_STAND_PCM_SHORT;
        case PCMLong:
            return I2S_COMM_FORMAT_STAND_PCM_LONG;
        case Max:
            return I2S_COMM_FORMAT_STAND_MAX;
        }
        FL_ASSERT(false, "Invalid comm format");
        return 0;
    };

    int pin_clk = config.mPinClk;
    int pin_ws = config.mPinWs;
    int pin_sd = config.mPinSd;
    int i2s_port = config.mI2sNum;
    u16 sample_rate = config.mSampleRate;
    auto channel_format = convert_channel(config.mAudioChannel);
    auto comm_format = convert_comm_format(config.mCommFormat);
    auto bit_resolution = config.mBitResolution;
    const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = sample_rate,
        .bits_per_sample = i2s_bits_per_sample_t(bit_resolution),
        .channel_format = i2s_channel_fmt_t(channel_format),
        .communication_format = i2s_comm_format_t(comm_format),
        .intr_alloc_flags = 0,
        .dma_buf_count = AUDIO_DMA_BUFFER_COUNT,
        .dma_buf_len = I2S_AUDIO_BUFFER_LEN,
        //.use_apll = true
        // .tx_desc_auto_clear ?
    };

    const i2s_pin_config_t pin_config = {.bck_io_num = pin_clk,
                                         .ws_io_num = pin_ws,
                                         .data_out_num = I2S_PIN_NO_CHANGE,
                                         .data_in_num = pin_sd};

    I2SContext out = {i2s_config, pin_config, i2s_port_t(i2s_port)};
    return out;
}

I2SContext i2s_audio_init(const AudioConfigI2S &config) {
    I2SContext ctx = make_context(config);
    i2s_driver_install(ctx.i2s_port, &ctx.i2s_config, 0, NULL);
    i2s_set_pin(ctx.i2s_port, &ctx.pin_config);
    i2s_zero_dma_buffer(ctx.i2s_port);
    return ctx;
}

size_t i2s_read_raw_samples(const I2SContext &ctx,
                            audio_sample_t (&buffer)[I2S_AUDIO_BUFFER_LEN]) {
    size_t bytes_read = 0;
    i2s_event_t event;

    esp_err_t result =
        i2s_read(ctx.i2s_port, buffer, sizeof(buffer), &bytes_read, 0);
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
    i2s_driver_uninstall(ctx.i2s_port);
}

} // namespace esp_i2s

} // namespace fl

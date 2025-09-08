

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

I2SContext make_context(const AudioConfigI2S &config);

I2SContext i2s_audio_init(const AudioConfigI2S &config);

size_t i2s_read_raw_samples(const I2SContext &ctx,
                            audio_sample_t (&buffer)[I2S_AUDIO_BUFFER_LEN]);

void i2s_audio_destroy(const I2SContext &ctx);

} // namespace esp_i2s

} // namespace fl

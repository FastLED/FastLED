#pragma once

#include "i2s_audio.h"
#include "fl/unused.h"

namespace fl {

void i2s_audio_init(const I2SAudioConfig& config = I2SAudioConfig{}) {
    FL_UNUSED(config);
}

// Worked in previous life but needs re-validation before becoming part of this api.
// HELP WANTED!
//void i2s_audio_enter_light_sleep();
//void i2s_audio_exit_light_sleep();
//void i2s_audio_shutdown();

size_t i2s_read_raw_samples(fl::vector_inlined<i16, IS2_AUDIO_BUFFER_LEN>* buffer) {
    FL_UNUSED(buffer);
    return 0;
}
double i2s_loudness_to_rms_imp441(double rms_loudness) {
    FL_UNUSED(rms_loudness);
    return 0;
}

}  // namespace fl

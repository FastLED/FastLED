#pragma once

#include "platforms/esp/32/audio/devices/i2s.hpp"
#include "platforms/audio_input_null.hpp"

// Ensure FASTLED_ESP32_I2S_SUPPORTED is defined (included via i2s.hpp)
#ifndef FASTLED_ESP32_I2S_SUPPORTED
#error "FASTLED_ESP32_I2S_SUPPORTED should be defined by including i2s.hpp"
#endif

namespace fl {

// ESP32-specific audio input creation function
fl::shared_ptr<IAudioInput> esp32_create_audio_input(const AudioConfig &config, fl::string *error_message) {
    if (config.is<AudioConfigI2S>()) {
#if FASTLED_ESP32_I2S_SUPPORTED
        FL_WARN("Creating I2S standard mode audio source");
        AudioConfigI2S std_config = config.get<AudioConfigI2S>();
        fl::shared_ptr<IAudioInput> out = fl::make_shared<fl::I2S_Audio>(std_config);
        return out;
#else
        const char* ERROR_MESSAGE = "I2S audio not supported on this ESP32 variant (no I2S hardware)";
        FL_WARN(ERROR_MESSAGE);
        if (error_message) {
            *error_message = ERROR_MESSAGE;
        }
        return fl::make_shared<fl::Null_Audio>();
#endif
    }
    const char* ERROR_MESSAGE = "Unsupported audio configuration";
    FL_WARN(ERROR_MESSAGE);
    if (error_message) {
        *error_message = ERROR_MESSAGE;
    }
    return fl::make_shared<fl::Null_Audio>();
}

} // namespace fl

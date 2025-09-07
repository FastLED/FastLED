#pragma once

#include "platforms/esp/32/audio/devices/i2s.hpp"
#include "platforms/esp/32/audio/devices/null.hpp"

namespace fl {

// Static factory method implementation
fl::shared_ptr<IAudioInput>
IAudioInput::create(const AudioConfig &config, fl::string *error_message) {
    if (config.is<AudioConfigI2S>()) {
        FL_WARN("Creating I2S standard mode audio source");
        AudioConfigI2S std_config = config.get<AudioConfigI2S>();
        fl::shared_ptr<IAudioInput> out = fl::make_shared<fl::I2S_Audio>(std_config);
        return out;
    }
    const char* ERROR_MESSAGE = "Unsupported I2S configuration";
    FL_WARN(ERROR_MESSAGE);
    if (error_message) {
        *error_message = ERROR_MESSAGE;
    }
    return fl::make_shared<fl::Null_Audio>();
}

} // namespace fl

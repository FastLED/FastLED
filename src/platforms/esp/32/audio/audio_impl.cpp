#ifdef ESP32

#include "platforms/esp/esp_version.h"

#if ESP_IDF_VERSION_5_OR_HIGHER

#include "audio_impl.hpp"
#include "fl/audio_input.h"
#include "fl/warn.h"

namespace fl {

// Static factory method implementation
fl::shared_ptr<IAudioInput>
IAudioInput::create(const AudioConfig &config, fl::string *error_message) {
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

#endif // ESP_IDF_VERSION_5_OR_HIGHER

#endif // ESP32
#pragma once

// IWYU pragma: private

#include "fl/stl/noexcept.h"
#include "platforms/audio_input_null.hpp"
#include "platforms/esp/32/audio/devices/i2s.hpp"
#include "platforms/esp/32/audio/devices/pdm.hpp"

// Ensure FASTLED_ESP32_I2S_SUPPORTED is defined (included via i2s.hpp)
#ifndef FASTLED_ESP32_I2S_SUPPORTED
#error "FASTLED_ESP32_I2S_SUPPORTED should be defined by including i2s.hpp"
#endif

// Ensure FASTLED_ESP32_PDM_SUPPORTED is defined (included via pdm.hpp)
#ifndef FASTLED_ESP32_PDM_SUPPORTED
#error "FASTLED_ESP32_PDM_SUPPORTED should be defined by including pdm.hpp"
#endif

namespace fl {

// ESP32-specific audio input creation function
fl::shared_ptr<audio::IInput>
esp32_create_audio_input(const audio::Config &config,
                         fl::string *error_message) FL_NOEXCEPT {
    if (config.is<audio::ConfigI2S>()) {
#if FASTLED_ESP32_I2S_SUPPORTED
        FL_WARN("Creating I2S standard mode audio source");
        audio::ConfigI2S std_config = config.get<audio::ConfigI2S>();
        fl::shared_ptr<audio::IInput> out =
            fl::make_shared<fl::I2S_Audio>(std_config);
        return out;
#else
        const char *ERROR_MESSAGE =
            "I2S audio not supported on this ESP32 variant (no I2S hardware)";
        FL_WARN(ERROR_MESSAGE);
        if (error_message) {
            *error_message = ERROR_MESSAGE;
        }
        return fl::make_shared<fl::Null_Audio>();
#endif
    } else if (config.is<audio::ConfigPdm>()) {
#if FASTLED_ESP32_PDM_SUPPORTED
        FL_WARN("Creating PDM mode audio source");
        audio::ConfigPdm pdm_config = config.get<audio::ConfigPdm>();
        fl::shared_ptr<audio::IInput> out =
            fl::make_shared<fl::PDM_Audio>(pdm_config);
        return out;
#else
        const char *ERROR_MESSAGE = "PDM audio not supported on this ESP32 "
                                    "variant (no PDM RX hardware)";
        FL_WARN(ERROR_MESSAGE);
        if (error_message) {
            *error_message = ERROR_MESSAGE;
        }
        return fl::make_shared<fl::Null_Audio>();
#endif
    }
    const char *ERROR_MESSAGE = "Unsupported audio configuration";
    FL_WARN(ERROR_MESSAGE);
    if (error_message) {
        *error_message = ERROR_MESSAGE;
    }
    return fl::make_shared<fl::Null_Audio>();
}

} // namespace fl

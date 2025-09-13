



#include "fl/sketch_macros.h"
#include "fl/shared_ptr.h"
#include "fl/memory.h"
#include "fl/string.h"
#include "fl/compiler_control.h"
#include "fl/has_include.h"
#include "platforms/audio_input_null.hpp"

// Include ESP32 audio input implementation if on ESP32
#ifdef ESP32
#include "platforms/esp/32/audio/audio_impl.hpp"
#endif

// Auto-determine Arduino usage if not explicitly set
#ifndef FASTLED_USES_ARDUINO_AUDIO_INPUT
  #if defined(ESP32)
    #define FASTLED_USES_ARDUINO_AUDIO_INPUT 0
  #elif FL_HAS_INCLUDE(<Arduino.h>)
    #define FASTLED_USES_ARDUINO_AUDIO_INPUT 1
  #else
    #define FASTLED_USES_ARDUINO_AUDIO_INPUT 0
  #endif
#endif

// Include the proper file.
#if FASTLED_USES_ARDUINO_AUDIO_INPUT
#include "platforms/arduino/audio_input.hpp"
#elif defined(ESP32)
#include "platforms/esp/32/audio/audio_impl.hpp"
#endif

namespace fl {

#if FASTLED_USES_ARDUINO_AUDIO_INPUT
// Use Arduino audio implementation
fl::shared_ptr<IAudioInput> platform_create_audio_input(const AudioConfig &config, fl::string *error_message) {
    return arduino_create_audio_input(config, error_message);
}

#elif defined(ESP32)
// ESP32 native implementation
fl::shared_ptr<IAudioInput> platform_create_audio_input(const AudioConfig &config, fl::string *error_message) {
    return esp32_create_audio_input(config, error_message);
}

#else
// Weak default implementation - no audio support
FL_LINK_WEAK
fl::shared_ptr<IAudioInput> platform_create_audio_input(const AudioConfig &config, fl::string *error_message) {
    if (error_message) {
        *error_message = "AudioInput not supported on this platform.";
    }
    return fl::make_shared<fl::Null_Audio>();
}
#endif

// Static method delegates to free function
fl::shared_ptr<IAudioInput>
IAudioInput::create(const AudioConfig &config, fl::string *error_message) {
    return platform_create_audio_input(config, error_message);
}

}  // namespace fl

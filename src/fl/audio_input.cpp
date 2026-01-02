



#include "fl/sketch_macros.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/shared_ptr.h"  // For shared_ptr
#include "fl/stl/string.h"
#include "fl/compiler_control.h"
#include "fl/has_include.h"
#include "platforms/audio_input_null.hpp"


// Auto-determine platform audio input support

// First check for Teensy (before Arduino, since Teensy is Arduino-compatible)
#include "platforms/arm/teensy/is_teensy.h"
#ifndef FASTLED_USES_TEENSY_AUDIO_INPUT
  #if defined(FL_IS_TEENSY)
    #define FASTLED_USES_TEENSY_AUDIO_INPUT 1
  #else
    #define FASTLED_USES_TEENSY_AUDIO_INPUT 0
  #endif
#endif

#ifndef FASTLED_USES_ARDUINO_AUDIO_INPUT
  #if defined(ESP32) && !defined(ESP8266)
    #define FASTLED_USES_ARDUINO_AUDIO_INPUT 0
  #elif defined(__EMSCRIPTEN__)
    #define FASTLED_USES_ARDUINO_AUDIO_INPUT 0
  #elif FASTLED_USES_TEENSY_AUDIO_INPUT
    // Teensy uses its own audio implementation, not generic Arduino
    #define FASTLED_USES_ARDUINO_AUDIO_INPUT 0
  #elif FL_HAS_INCLUDE(<Arduino.h>)
    #define FASTLED_USES_ARDUINO_AUDIO_INPUT 1
  #else
    #define FASTLED_USES_ARDUINO_AUDIO_INPUT 0
  #endif
#endif

#if !FASTLED_USES_ARDUINO_AUDIO_INPUT
#if defined(ESP32) && !defined(ESP8266)
#define FASTLED_USES_ESP32_AUDIO_INPUT 1
#else
#define FASTLED_USES_ESP32_AUDIO_INPUT 0
#endif
#else
#define FASTLED_USES_ESP32_AUDIO_INPUT 0
#endif

// Determine WASM audio input support
#if !FASTLED_USES_ARDUINO_AUDIO_INPUT && !FASTLED_USES_ESP32_AUDIO_INPUT
#if defined(__EMSCRIPTEN__)
#define FASTLED_USES_WASM_AUDIO_INPUT 1
#else
#define FASTLED_USES_WASM_AUDIO_INPUT 0
#endif
#else
#define FASTLED_USES_WASM_AUDIO_INPUT 0
#endif

// Include platform-specific audio input implementation
#if FASTLED_USES_TEENSY_AUDIO_INPUT
#include "platforms/arm/teensy/audio_input_teensy.h"
#elif FASTLED_USES_ARDUINO_AUDIO_INPUT
#include "platforms/arduino/audio_input.hpp"
#elif FASTLED_USES_ESP32_AUDIO_INPUT
#include "platforms/esp/32/audio/audio_impl.hpp"
#elif FASTLED_USES_WASM_AUDIO_INPUT
#include "platforms/wasm/audio_input_wasm.hpp"
#endif

namespace fl {

#if FASTLED_USES_TEENSY_AUDIO_INPUT
// Use Teensy audio implementation
fl::shared_ptr<IAudioInput> platform_create_audio_input(const AudioConfig &config, fl::string *error_message) {
    return teensy_create_audio_input(config, error_message);
}
#elif FASTLED_USES_ARDUINO_AUDIO_INPUT
// Use Arduino audio implementation
fl::shared_ptr<IAudioInput> platform_create_audio_input(const AudioConfig &config, fl::string *error_message) {
    return arduino_create_audio_input(config, error_message);
}
#elif FASTLED_USES_ESP32_AUDIO_INPUT
// ESP32 native implementation
fl::shared_ptr<IAudioInput> platform_create_audio_input(const AudioConfig &config, fl::string *error_message) {
    return esp32_create_audio_input(config, error_message);
}
#elif FASTLED_USES_WASM_AUDIO_INPUT
// WASM implementation - audio comes from JavaScript
fl::shared_ptr<IAudioInput> platform_create_audio_input(const AudioConfig &config, fl::string *error_message) {
    return wasm_create_audio_input(config, error_message);
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

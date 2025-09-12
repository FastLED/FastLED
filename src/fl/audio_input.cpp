



#include "fl/sketch_macros.h"
#include "fl/shared_ptr.h"
#include "fl/memory.h"
#include "fl/string.h"
#include "platforms/esp/32/audio/devices/null.hpp"

namespace fl {

// Weak default implementation - returns null audio
__attribute__((weak))
fl::shared_ptr<IAudioInput> platform_create_audio_input(const AudioConfig &config, fl::string *error_message) {
    if (error_message) {
        *error_message = "AudioInput not supported on this platform.";
    }
    return fl::make_shared<fl::Null_Audio>();
}

// Static method delegates to free function
fl::shared_ptr<IAudioInput>
IAudioInput::create(const AudioConfig &config, fl::string *error_message) {
    return platform_create_audio_input(config, error_message);
}

}  // namespace fl

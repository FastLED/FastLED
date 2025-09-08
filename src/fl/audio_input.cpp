



#ifdef ESP32
#include "platforms/esp/esp_version.h"
#if ESP_IDF_VERSION_5_OR_HIGHER
// Implementation is in platforms/esp/32/audio/audio_impl.cpp
#else
#include "fl/sketch_macros.h"
#include "fl/shared_ptr.h"
#include "fl/memory.h"
#include "fl/string.h"
#include "platforms/esp/32/audio/devices/null.hpp"
namespace fl {
fl::shared_ptr<IAudioInput>
IAudioInput::create(const AudioConfig &config, fl::string *error_message) {
    if (error_message) {
        *error_message = "AudioInput not supported on this platform.";
    }
    return fl::make_shared<fl::Null_Audio>();
}
}  // namespace fl
#endif

#endif  // 

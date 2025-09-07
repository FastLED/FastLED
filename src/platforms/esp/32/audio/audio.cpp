#ifdef ESP32

#include "platforms/esp/esp_version.h"
#include "fl/sketch_macros.h"
#include "fl/shared_ptr.h"
#include "fl/memory.h"
#include "fl/string.h"
#include "platforms/esp/32/audio/devices/null.hpp"


#if ESP_IDF_VERSION_5_OR_HIGHER
#include "platforms/esp/32/audio/audio_impl.hpp"
#else
namespace fl {
fl::shared_ptr<IAudioInput>
IAudioInput::create(const AudioConfig &config, fl::string *error_message) {
    if (error_message) {
        *error_message = "ESP32 platform not supported";
    }
    return fl::make_shared<fl::Null_Audio>();
}
}  // namespace fl
#endif

#endif  // 

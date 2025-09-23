#include "gif.h"
#include "../../third_party/libnsgif/software_decoder.h"

namespace fl {

IDecoderPtr Gif::createDecoder(const GifConfig& config, fl::string* error_message) {
    // Create the software GIF decoder
    auto decoder = fl::make_shared<fl::third_party::SoftwareGifDecoder>(config.format);

    if (!decoder) {
        if (error_message) {
            *error_message = "Failed to allocate GIF decoder";
        }
        return nullptr;
    }

    return decoder;
}

bool Gif::isSupported() {
    // libnsgif is always available since it's included directly
    return true;
}

} // namespace fl
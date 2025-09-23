#include "fl/codec/mpeg1.h"
#include "fl/utility.h"

namespace fl {

// MPEG1 factory implementation
IDecoderPtr Mpeg1::createDecoder(const Mpeg1Config& config, fl::string* error_message) {
    FL_UNUSED(error_message);
    // MPEG1 is currently supported via software decoder on all platforms
    return fl::make_shared<third_party::SoftwareMpeg1Decoder>(config);
}

bool Mpeg1::isSupported() {
    // Software MPEG1 decoder is available on all platforms
    return true;
}

} // namespace fl

#include "fl/codec/webp.h"
#include "fl/utility.h"
#include "fl/str.h"
#include "fl/compiler_control.h"

// Note: simplewebp has compilation issues on embedded platforms
// Keeping stub implementation for now until the issues are resolved

namespace fl {

bool Webp::decode(const WebpDecoderConfig& config, fl::span<const fl::u8> data, Frame* frame, fl::string* error_message) {
    // TODO: Implement actual WebP decoding with simplewebp after fixing compilation issues
    FL_UNUSED(config);
    FL_UNUSED(data);
    FL_UNUSED(frame);

    if (error_message) {
        *error_message = "WebP decoding not yet implemented - simplewebp has compilation issues on embedded platforms";
    }
    return false;
}

FramePtr Webp::decode(const WebpDecoderConfig& config, fl::span<const fl::u8> data, fl::string* error_message) {
    FL_UNUSED(config);
    FL_UNUSED(data);

    if (error_message) {
        *error_message = "WebP decoding not yet implemented - simplewebp has compilation issues on embedded platforms";
    }
    return nullptr;
}

bool Webp::isSupported() {
    // WebP will be supported once simplewebp compilation issues are resolved
    return false;
}

bool Webp::getDimensions(fl::span<const fl::u8> data, fl::u16* width, fl::u16* height, fl::string* error_message) {
    FL_UNUSED(data);
    FL_UNUSED(width);
    FL_UNUSED(height);

    if (error_message) {
        *error_message = "WebP dimension detection not yet implemented";
    }
    return false;
}

bool Webp::isLossless(fl::span<const fl::u8> data, bool* lossless, fl::string* error_message) {
    FL_UNUSED(data);
    FL_UNUSED(lossless);

    if (error_message) {
        *error_message = "WebP lossless detection not yet implemented";
    }
    return false;
}

} // namespace fl
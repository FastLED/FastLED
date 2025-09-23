#pragma once

#include "fl/codec/common.h"

namespace fl {

// WebP-specific configuration
struct WebpDecoderConfig {
    PixelFormat format = PixelFormat::RGB888;
    bool preferLossless = false;  // Prefer lossless decoding when available
    fl::u16 maxWidth = 1920;
    fl::u16 maxHeight = 1080;

    WebpDecoderConfig() = default;
    WebpDecoderConfig(PixelFormat fmt) : format(fmt) {}
    WebpDecoderConfig(PixelFormat fmt, bool lossless)
        : format(fmt), preferLossless(lossless) {}
};

// WebP decoder class
class Webp {
public:
    // Simplified decode function that writes directly to a Frame
    static bool decode(const WebpDecoderConfig& config, fl::span<const fl::u8> data, Frame* frame, fl::string* error_message = nullptr);

    // Decode function that returns a new Frame. If the Frame could not be generated then check error_message.
    static FramePtr decode(const WebpDecoderConfig& config, fl::span<const fl::u8> data, fl::string* error_message = nullptr);

    // Check if WebP decoding is supported on this platform. This should always return true since
    // WebP decoding is done with simplewebp, which is always available.
    static bool isSupported();

    // Get WebP image dimensions without full decoding
    static bool getDimensions(fl::span<const fl::u8> data, fl::u16* width, fl::u16* height, fl::string* error_message = nullptr);

    // Check if the WebP image is lossless
    static bool isLossless(fl::span<const fl::u8> data, bool* lossless, fl::string* error_message = nullptr);
};

} // namespace fl
#pragma once

#include "fl/codec/common.h"

namespace fl {

// JPEG-specific configuration
struct JpegDecoderConfig {
    enum Quality { Low, Medium, High };

    Quality quality = Medium;
    PixelFormat format = PixelFormat::RGB888;
    bool useHardwareAcceleration = true;
    fl::u16 maxWidth = 1920;
    fl::u16 maxHeight = 1080;

    JpegDecoderConfig() = default;
    JpegDecoderConfig(Quality q, PixelFormat fmt = PixelFormat::RGB888)
        : quality(q), format(fmt) {}
};

// JPEG decoder class
class Jpeg {
public:
    // Create a JPEG decoder for the current platform
    static fl::shared_ptr<IDecoder> createDecoder(const JpegDecoderConfig& config, fl::string* error_message = nullptr);

    // Check if JPEG decoding is supported on this platform. This should always return true since
    // JPEG decoding is done with TJpg_Decoder, which is always available.
    static bool isSupported();
};

} // namespace fl

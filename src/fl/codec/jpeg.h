#pragma once

#include "fl/codec/common.h"

namespace fl {

// JPEG-specific configuration
struct JpegDecoderConfig {
    enum Quality { Low, Medium, High };

    Quality quality = High;
    PixelFormat format = PixelFormat::RGB888;
    bool useHardwareAcceleration = true;  // TODO: Is this even used?
    fl::u16 maxWidth = 1920;
    fl::u16 maxHeight = 1080;

    JpegDecoderConfig() = default;
    JpegDecoderConfig(Quality q, PixelFormat fmt = PixelFormat::RGB888)
        : quality(q), format(fmt) {}
};

// JPEG decoder class
class Jpeg {
public:
    // Simplified decode function that writes directly to a Frame
    static bool decode(const JpegDecoderConfig& config, fl::span<const fl::u8> data, Frame* frame, fl::string* error_message = nullptr);

    // Decode function that returns a new Frame. If the Frame could not be generated then check error_message.
    static FramePtr decode(const JpegDecoderConfig& config, fl::span<const fl::u8> data, fl::string* error_message = nullptr);

    // Check if JPEG decoding is supported on this platform. This should always return true since
    // JPEG decoding is done with TJpg_Decoder, which is always available.
    static bool isSupported();
};

} // namespace fl

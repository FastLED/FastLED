#pragma once

#include "fl/codec/common.h"

namespace fl {

// JPEG-specific configuration
struct JpegConfig {
    enum Quality { Low, Medium, High };

    Quality quality = Medium;
    PixelFormat format = PixelFormat::RGB888;
    bool useHardwareAcceleration = true;
    fl::u16 maxWidth = 1920;
    fl::u16 maxHeight = 1080;

    JpegConfig() = default;
    JpegConfig(Quality q, PixelFormat fmt = PixelFormat::RGB888)
        : quality(q), format(fmt) {}
};

// JPEG decoder factory
namespace jpeg {

// Create a JPEG decoder for the current platform
fl::shared_ptr<IDecoder> createDecoder(const JpegConfig& config, fl::string* error_message = nullptr);

// Check if JPEG decoding is supported on this platform
bool isSupported();

} // namespace jpeg


} // namespace fl
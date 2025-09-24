#pragma once

#include "fl/codec/common.h"

namespace fl {

// JPEG metadata information structure
struct JpegInfo {
    fl::u16 width = 0;          // Image width in pixels
    fl::u16 height = 0;         // Image height in pixels
    fl::u8 components = 0;      // Number of color components (1=grayscale, 3=color)
    fl::u8 bitsPerComponent = 8; // Always 8 for JPEG baseline
    bool isGrayscale = false;   // True if grayscale image
    bool isValid = false;       // True if metadata was successfully parsed

    JpegInfo() = default;

    // Constructor for easy initialization
    JpegInfo(fl::u16 w, fl::u16 h, fl::u8 comp)
        : width(w), height(h), components(comp)
        , isGrayscale(comp == 1), isValid(true) {}
};

// JPEG-specific configuration
struct JpegDecoderConfig {
    enum Quality { Low, Medium, High };

    Quality quality = High;
    PixelFormat format = PixelFormat::RGB888;

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

    // Simplified decode with default config (High quality, RGB888)
    static FramePtr decode(fl::span<const fl::u8> data, fl::string* error_message = nullptr) {
        JpegDecoderConfig config; // Uses defaults
        return decode(config, data, error_message);
    }

    // Check if JPEG decoding is supported on this platform. This should always return true since
    // JPEG decoding is done with TJpg_Decoder, which is always available.
    static bool isSupported();

    // Parse JPEG metadata from byte data without full decoding
    // This is a fast, lightweight operation that only reads the JPEG header
    static JpegInfo parseJpegInfo(fl::span<const fl::u8> data, fl::string* error_message = nullptr);
};

} // namespace fl

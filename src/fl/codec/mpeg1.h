#pragma once

#include "fl/codec/common.h"
#include "third_party/mpeg1_decoder/software_decoder.h"

namespace fl {

// MPEG1 metadata information structure
struct Mpeg1Info {
    fl::u16 width = 0;           // Video width in pixels
    fl::u16 height = 0;          // Video height in pixels
    fl::u16 frameRate = 0;       // Frame rate (fps)
    fl::u32 frameCount = 0;      // Total number of frames (may be 0 if unknown)
    fl::u32 duration = 0;        // Duration in milliseconds (may be 0 if unknown)
    bool hasAudio = false;       // True if MPEG1 contains audio track
    bool isValid = false;        // True if metadata was successfully parsed

    Mpeg1Info() = default;

    // Constructor for easy initialization
    Mpeg1Info(fl::u16 w, fl::u16 h, fl::u16 fps)
        : width(w), height(h), frameRate(fps), isValid(true) {}
};

// Re-export MPEG1 configuration from third_party
using Mpeg1Config = third_party::Mpeg1Config;

// MPEG1 decoder factory
class Mpeg1 {
public:
    // Create an MPEG1 decoder for the current platform
    static IDecoderPtr createDecoder(const Mpeg1Config& config, fl::string* error_message = nullptr);

    // Create an MPEG1 decoder with default config (Streaming, 30fps, no audio)
    static IDecoderPtr createDecoder(fl::string* error_message = nullptr) {
        Mpeg1Config config; // Uses defaults
        return createDecoder(config, error_message);
    }

    // Check if MPEG1 decoding is supported on this platform
    static bool isSupported();

    // Parse MPEG1 metadata from byte data without creating a decoder
    // This is a fast, lightweight operation that only reads the MPEG1 header
    static Mpeg1Info parseMpeg1Info(fl::span<const fl::u8> data, fl::string* error_message = nullptr);
};

} // namespace fl

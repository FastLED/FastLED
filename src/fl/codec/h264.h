#pragma once

#include "fl/codec/common.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/noexcept.h"

namespace fl {

// H.264 metadata parsed from SPS NAL unit via Exp-Golomb decoding.
struct H264Info {
    fl::u16 width = 0;
    fl::u16 height = 0;
    fl::u8 profile = 0;       // Baseline=66, Main=77, High=100, etc.
    fl::u8 level = 0;         // e.g. 30 = level 3.0
    fl::u8 numRefFrames = 0;
    bool isValid = false;

    H264Info() FL_NOEXCEPT = default;
    H264Info(fl::u16 w, fl::u16 h, fl::u8 prof, fl::u8 lvl)
        : width(w), height(h), profile(prof), level(lvl), isValid(true) {}
};

// H.264 decoder configuration
struct H264Config {
    fl::u16 maxWidth = 1920;
    fl::u16 maxHeight = 1080;
};

// H.264 decoder factory — follows the same pattern as Mpeg1.
// On host platforms, createDecoder() returns a NullDecoder (structural validation only).
// On ESP32-P4, createDecoder() returns a hardware-accelerated decoder.
class H264 {
public:
    // Create an H.264 decoder for the current platform.
    // Returns NullDecoder on platforms without H.264 HW support.
    static IDecoderPtr createDecoder(const H264Config& config,
                                     fl::string* error_message = nullptr);

    static IDecoderPtr createDecoder(fl::string* error_message = nullptr) {
        H264Config config;
        return createDecoder(config, error_message);
    }

    // Check if H.264 decoding is supported on this platform.
    // Currently only true on ESP32-P4.
    static bool isSupported();

    // Parse H.264 metadata from an MP4 container without creating a decoder.
    // Extracts SPS from avcC box and parses width/height/profile/level.
    static H264Info parseH264Info(fl::span<const fl::u8> mp4Data,
                                  fl::string* error_message = nullptr);

    // Parse H.264 SPS NAL unit directly (without MP4 container).
    // The spsData should be the raw SPS NAL unit bytes (without start code).
    static H264Info parseSPS(fl::span<const fl::u8> spsData,
                             fl::string* error_message = nullptr);
};

} // namespace fl

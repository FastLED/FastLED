#pragma once

#include "fl/codec/common.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

// GIF metadata information structure
struct GifInfo {
    fl::u16 width = 0;           // Image width in pixels
    fl::u16 height = 0;          // Image height in pixels
    fl::u32 frameCount = 0;      // Number of frames (1 for static, >1 for animated)
    fl::u32 loopCount = 0;       // Animation loop count (0 = infinite)
    fl::u8 bitsPerPixel = 0;     // Color depth (typically 8)
    bool isAnimated = false;     // True if GIF has multiple frames
    bool isValid = false;        // True if metadata was successfully parsed

    GifInfo() = default;

    // Constructor for easy initialization
    GifInfo(fl::u16 w, fl::u16 h, fl::u32 frames, fl::u32 loops = 0)
        : width(w), height(h), frameCount(frames), loopCount(loops)
        , isAnimated(frames > 1), isValid(true) {}
};

// GIF-specific configuration
struct GifConfig {
    enum FrameMode { SingleFrame, Streaming };

    FrameMode mode = Streaming;
    PixelFormat format = PixelFormat::RGB888;
    fl::u8 bufferFrames = 3;  // For smooth animation

    GifConfig() = default;
    GifConfig(FrameMode m, PixelFormat fmt = PixelFormat::RGB888)
        : mode(m), format(fmt) {}
};

// GIF decoder factory
class Gif {
public:
    // Create a GIF decoder for the current platform
    static IDecoderPtr createDecoder(const GifConfig& config, fl::string* error_message = nullptr);

    // Create a GIF decoder with default config (Streaming, RGB888)
    static IDecoderPtr createDecoder(fl::string* error_message = nullptr) {
        GifConfig config; // Uses defaults
        return createDecoder(config, error_message);
    }

    // Check if GIF decoding is supported on this platform
    static bool isSupported();

    // Parse GIF metadata from byte data without creating a decoder
    // This is a fast, lightweight operation that only reads the GIF header
    static GifInfo parseGifInfo(fl::span<const fl::u8> data, fl::string* error_message = nullptr);
};

} // namespace fl
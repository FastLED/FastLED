#pragma once

#include "fl/codec/common.h"
#include "fl/vector.h"
#include "fl/shared_ptr.h"

namespace fl {

// GIF-specific configuration
struct GifConfig {
    enum FrameMode { SingleFrame, Streaming };

    FrameMode mode = Streaming;
    PixelFormat format = PixelFormat::RGB888;
    bool looping = true;
    fl::u16 maxWidth = 1920;
    fl::u16 maxHeight = 1080;
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

    // Check if GIF decoding is supported on this platform
    static bool isSupported();
};

} // namespace fl
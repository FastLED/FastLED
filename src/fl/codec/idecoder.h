#pragma once

#include "fl/stl/shared_ptr.h"
#include "fl/str.h"
#include "fl/stl/stdint.h"
#include "fl/bytestream.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/function.h"
#include "fl/audio.h"
#include "fl/fx/frame.h"

namespace fl {

// Decoder result types
enum class DecodeResult {
    Success,
    NeedsMoreData,
    EndOfStream,
    Error,
    UnsupportedFormat
};

// Audio frame callback - called when audio frames are decoded
// Not all decoders will support audio
using AudioFrameCallback = fl::function<void(const AudioSample&)>;

// Base decoder interface for multimedia codecs
// This interface provides a unified API for decoding various formats including:
// - Animated GIFs (multi-frame)
// - MPEG1 video (streaming)
// - Future codec implementations
class IDecoder {
public:
    virtual ~IDecoder() = default;

    // Lifecycle methods
    virtual bool begin(fl::ByteStreamPtr stream) = 0;
    virtual void end() = 0;
    virtual bool isReady() const = 0;
    virtual bool hasError(fl::string* msg = nullptr) const = 0;

    // Decoding methods
    virtual DecodeResult decode() = 0;
    virtual Frame getCurrentFrame() = 0;
    virtual bool hasMoreFrames() const = 0;

    // Optional methods for advanced usage
    virtual fl::u32 getFrameCount() const { return 0; }
    virtual fl::u32 getCurrentFrameIndex() const { return 0; }
    virtual bool seek(fl::u32 frameIndex) { (void)frameIndex; return false; }

    // Audio support (optional - default implementations for decoders without audio)
    virtual bool hasAudio() const { return false; }
    virtual void setAudioCallback(AudioFrameCallback callback) { (void)callback; }
    virtual int getAudioSampleRate() const { return 0; }
};

// Null decoder implementation for unsupported platforms
class NullDecoder : public IDecoder {
public:
    bool begin(fl::ByteStreamPtr) override { return false; }
    void end() override {}
    bool isReady() const override { return false; }
    bool hasError(fl::string* msg = nullptr) const override {
        if (msg) {
            *msg = "Codec not supported on this platform";
        }
        return true;
    }

    DecodeResult decode() override { return DecodeResult::UnsupportedFormat; }
    Frame getCurrentFrame() override { return Frame(0); }
    bool hasMoreFrames() const override { return false; }
};

// Smart pointer typedef - must come after class definition
FASTLED_SHARED_PTR(IDecoder);


} // namespace fl

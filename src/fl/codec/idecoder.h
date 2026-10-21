#pragma once

#include "fl/stl/shared_ptr.h"  // IWYU pragma: keep
#include "fl/stl/string.h"
#include "fl/stl/stdint.h"
#include "fl/fs/file_handle.h"  // For fl::filebuf
#include "fl/stl/shared_ptr.h"  // IWYU pragma: keep
#include "fl/stl/function.h"
#include "fl/audio/audio.h"
#include "fl/fx/frame.h"
#include "fl/stl/noexcept.h"

namespace fl {

using filebuf_ptr = fl::shared_ptr<filebuf>; // filebuf is defined in file_handle.h

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
using AudioFrameCallback = fl::function<void(const audio::Sample&)>;

// Base decoder interface for multimedia codecs
// This interface provides a unified API for decoding various formats including:
// - Animated GIFs (multi-frame)
// - MPEG1 video (streaming)
// - Future codec implementations
class IDecoder {
public:
    virtual ~IDecoder() FL_NO_EXCEPT = default;

    // Lifecycle methods
    virtual bool begin(fl::filebuf_ptr stream) FL_NO_EXCEPT = 0;
    virtual void end() FL_NO_EXCEPT = 0;
    virtual bool isReady() const FL_NO_EXCEPT = 0;
    virtual bool hasError(fl::string* msg = nullptr) const FL_NO_EXCEPT = 0;

    // Decoding methods
    virtual DecodeResult decode() FL_NO_EXCEPT = 0;
    virtual Frame getCurrentFrame() FL_NO_EXCEPT = 0;
    virtual bool hasMoreFrames() const FL_NO_EXCEPT = 0;

    // Optional methods for advanced usage
    virtual fl::u32 getFrameCount() const FL_NO_EXCEPT { return 0; }
    virtual fl::u32 getCurrentFrameIndex() const FL_NO_EXCEPT { return 0; }
    virtual bool seek(fl::u32 frameIndex) FL_NO_EXCEPT { (void)frameIndex; return false; }

    // Audio support (optional - default implementations for decoders without audio)
    virtual bool hasAudio() const FL_NO_EXCEPT { return false; }
    virtual void setAudioCallback(AudioFrameCallback callback) FL_NO_EXCEPT { (void)callback; }
    virtual int getAudioSampleRate() const FL_NO_EXCEPT { return 0; }
};

// Null decoder implementation for unsupported platforms
class NullDecoder : public IDecoder {
public:
    bool begin(fl::filebuf_ptr) FL_NO_EXCEPT override { return false; }
    void end() FL_NO_EXCEPT override {}
    bool isReady() const FL_NO_EXCEPT override { return false; }
    bool hasError(fl::string* msg = nullptr) const FL_NO_EXCEPT override {
        if (msg) {
            *msg = "Codec not supported on this platform";
        }
        return true;
    }

    DecodeResult decode() FL_NO_EXCEPT override { return DecodeResult::UnsupportedFormat; }
    Frame getCurrentFrame() FL_NO_EXCEPT override { return Frame(0); }
    bool hasMoreFrames() const FL_NO_EXCEPT override { return false; }
};

// Smart pointer typedef - must come after class definition
FASTLED_SHARED_PTR(IDecoder);


} // namespace fl

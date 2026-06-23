#pragma once

#include "fl/codec/common.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "third_party/pl_mpeg/src/pl_mpeg.h"

namespace fl {
namespace third_party {

// MPEG1-specific configuration
struct Mpeg1Config {
    enum FrameMode { SingleFrame, Streaming };  // TODO: Why is this needed?

    FrameMode mode = Streaming;
    fl::u16 targetFps = 30;
    bool looping = false;
    bool skipAudio = false;  // Enable audio by default
    bool immediateMode = true;  // For real-time LED applications - bypass frame buffering
    fl::u8 bufferFrames = 2;  // Only used when immediateMode = false
    AudioFrameCallback audioCallback;  // Optional callback for audio frames (default-constructed is empty)

    Mpeg1Config() = default;
    Mpeg1Config(FrameMode m, fl::u16 fps = 30) FL_NOEXCEPT
        : mode(m), targetFps(fps) {}
};

// Software MPEG1 decoder implementation
// Based on pl_mpeg library concepts but simplified for microcontrollers
class SoftwareMpeg1Decoder : public IDecoder {
private:
    struct Mpeg1DecoderData;
    Mpeg1Config config_;
    Mpeg1DecoderData* decoderData_;
    fl::filebuf_ptr stream_;
    fl::shared_ptr<Frame> currentFrame_;
    fl::string errorMessage_;
    bool ready_ = false;
    bool hasError_ = false;

    // Frame buffering for streaming mode
    fl::vector<fl::shared_ptr<Frame>> frameBuffer_;
    fl::u8 currentFrameIndex_ = 0;
    fl::u8 lastDecodedIndex_ = 0;
    bool endOfStream_ = false;

    // Internal methods
    bool initializeDecoder() FL_NOEXCEPT;
    bool decodeNextFrame() FL_NOEXCEPT;
    void cleanupDecoder() FL_NOEXCEPT;
    void setError(const fl::string& message) FL_NOEXCEPT;
    bool parseSequenceHeader() FL_NOEXCEPT;
    bool decodePictureHeader() FL_NOEXCEPT;
    bool decodeFrame() FL_NOEXCEPT;
    void allocateFrameBuffers() FL_NOEXCEPT;

public:
    explicit SoftwareMpeg1Decoder(const Mpeg1Config& config) FL_NOEXCEPT;
    ~SoftwareMpeg1Decoder();

    // IDecoder interface
    bool begin(fl::filebuf_ptr stream) FL_NOEXCEPT override;
    void end() FL_NOEXCEPT override;
    bool isReady() const FL_NOEXCEPT override { return ready_; }
    bool hasError(fl::string* msg = nullptr) const FL_NOEXCEPT override;

    DecodeResult decode() FL_NOEXCEPT override;
    Frame getCurrentFrame() FL_NOEXCEPT override;
    bool hasMoreFrames() const FL_NOEXCEPT override;

    // MPEG1-specific methods
    fl::u32 getFrameCount() const FL_NOEXCEPT override;
    fl::u32 getCurrentFrameIndex() const FL_NOEXCEPT override { return currentFrameIndex_; }
    bool seek(fl::u32 frameIndex) FL_NOEXCEPT override;

    // Get video properties
    fl::u16 getWidth() const FL_NOEXCEPT;
    fl::u16 getHeight() const FL_NOEXCEPT;
    fl::u16 getFrameRate() const FL_NOEXCEPT;

    // Static callback for pl_mpeg video decoding
    static void videoDecodeCallback(fl::third_party::plm_t* plm, fl::third_party::plm_frame_t* frame, void* user) FL_NOEXCEPT;

    // Static callback for pl_mpeg audio decoding
    static void audioDecodeCallback(fl::third_party::plm_t* plm, fl::third_party::plm_samples_t* samples, void* user) FL_NOEXCEPT;

    // IDecoder audio interface overrides
    bool hasAudio() const FL_NOEXCEPT override;
    void setAudioCallback(AudioFrameCallback callback) FL_NOEXCEPT override;
    int getAudioSampleRate() const FL_NOEXCEPT override;
};

} // namespace third_party
} // namespace fl

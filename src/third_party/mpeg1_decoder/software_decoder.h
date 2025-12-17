#pragma once

#include "fl/codec/common.h"
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
    Mpeg1Config(FrameMode m, fl::u16 fps = 30)
        : mode(m), targetFps(fps) {}
};

// Software MPEG1 decoder implementation
// Based on pl_mpeg library concepts but simplified for microcontrollers
class SoftwareMpeg1Decoder : public IDecoder {
private:
    struct Mpeg1DecoderData;
    Mpeg1Config config_;
    Mpeg1DecoderData* decoderData_;
    fl::ByteStreamPtr stream_;
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
    bool initializeDecoder();
    bool decodeNextFrame();
    void cleanupDecoder();
    void setError(const fl::string& message);
    bool parseSequenceHeader();
    bool decodePictureHeader();
    bool decodeFrame();
    void allocateFrameBuffers();

public:
    explicit SoftwareMpeg1Decoder(const Mpeg1Config& config);
    ~SoftwareMpeg1Decoder();

    // IDecoder interface
    bool begin(fl::ByteStreamPtr stream) override;
    void end() override;
    bool isReady() const override { return ready_; }
    bool hasError(fl::string* msg = nullptr) const override;

    DecodeResult decode() override;
    Frame getCurrentFrame() override;
    bool hasMoreFrames() const override;

    // MPEG1-specific methods
    fl::u32 getFrameCount() const override;
    fl::u32 getCurrentFrameIndex() const override { return currentFrameIndex_; }
    bool seek(fl::u32 frameIndex) override;

    // Get video properties
    fl::u16 getWidth() const;
    fl::u16 getHeight() const;
    fl::u16 getFrameRate() const;

    // Static callback for pl_mpeg video decoding
    static void videoDecodeCallback(fl::third_party::plm_t* plm, fl::third_party::plm_frame_t* frame, void* user);

    // Static callback for pl_mpeg audio decoding
    static void audioDecodeCallback(fl::third_party::plm_t* plm, fl::third_party::plm_samples_t* samples, void* user);

    // IDecoder audio interface overrides
    bool hasAudio() const override;
    void setAudioCallback(AudioFrameCallback callback) override;
    int getAudioSampleRate() const override;
};

} // namespace third_party
} // namespace fl

#pragma once

#include "fl/codec/common.h"
#include "fl/vector.h"
#include "fl/shared_ptr.h"

namespace fl {

// MPEG1-specific configuration
struct Mpeg1Config {
    enum FrameMode { SingleFrame, Streaming };  // TODO: Why is this needed?

    FrameMode mode = Streaming;
    fl::u16 targetFps = 30;
    bool looping = false;
    bool skipAudio = true;
    fl::u8 bufferFrames = 2;  // TODO: Is this necessary?

    Mpeg1Config() = default;
    Mpeg1Config(FrameMode m, fl::u16 fps = 30)
        : mode(m), targetFps(fps) {}
};

// MPEG1 decoder factory
class Mpeg1 {
public:
    // Create an MPEG1 decoder for the current platform
    static fl::shared_ptr<IDecoder> createDecoder(const Mpeg1Config& config, fl::string* error_message = nullptr);

    // Check if MPEG1 decoding is supported on this platform
    static bool isSupported();
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
    fl::u8 totalFrames_ = 0;
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
    static void videoDecodeCallback(void* plm, void* frame, void* user);
};

} // namespace fl

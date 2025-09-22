#pragma once

#include "fl/codec/common.h"

namespace fl {

// JPEG-specific configuration
struct JpegConfig {
    enum Quality { Low, Medium, High };

    Quality quality = Medium;
    PixelFormat format = PixelFormat::RGB888;
    bool useHardwareAcceleration = true;
    fl::u16 maxWidth = 1920;
    fl::u16 maxHeight = 1080;

    JpegConfig() = default;
    JpegConfig(Quality q, PixelFormat fmt = PixelFormat::RGB888)
        : quality(q), format(fmt) {}
};

// JPEG decoder factory
namespace jpeg {

// Create a JPEG decoder for the current platform
fl::shared_ptr<IDecoder> createDecoder(const JpegConfig& config, fl::string* error_message = nullptr);

// Check if JPEG decoding is supported on this platform
bool isSupported();

} // namespace jpeg

// Platform-specific base class for JPEG decoders
class JpegDecoderBase : public IDecoder {
protected:
    JpegConfig config_;
    fl::ByteStreamPtr stream_;
    Frame currentFrame_;
    fl::string errorMessage_;
    bool ready_ = false;
    bool hasError_ = false;

    // Internal buffer management
    fl::scoped_array<fl::u8> frameBuffer_;
    fl::size bufferSize_ = 0;

    virtual bool initializeDecoder() = 0;
    virtual bool decodeInternal() = 0;
    virtual void cleanupDecoder() = 0;

public:
    explicit JpegDecoderBase(const JpegConfig& config);
    virtual ~JpegDecoderBase();

    // IDecoder interface
    bool begin(fl::ByteStreamPtr stream) override;
    void end() override;
    bool isReady() const override { return ready_; }
    bool hasError(fl::string* msg = nullptr) const override;

    DecodeResult decode() override;
    Frame getCurrentFrame() override { return currentFrame_; }
    bool hasMoreFrames() const override { return false; } // JPEG is single frame

protected:
    void allocateFrameBuffer();
    void setError(const fl::string& message);
    fl::size getExpectedFrameSize() const;
};

} // namespace fl
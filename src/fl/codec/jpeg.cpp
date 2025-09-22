#include "fl/codec/jpeg.h"
#include "fl/codec/jpeg_tjpg_decoder.h"
#include "fl/utility.h"

namespace fl {

// JPEG factory implementation
namespace jpeg {

fl::shared_ptr<IDecoder> createDecoder(const JpegConfig& config, fl::string* error_message) {
    (void)error_message; // Suppress unused parameter warning for now
    return fl::make_shared<TJpgDecoder>(config);
}

bool isSupported() {
    // TJpg_Decoder is now integrated
    return true;
}

} // namespace jpeg

// JpegDecoderBase implementation
JpegDecoderBase::JpegDecoderBase(const JpegConfig& config)
    : config_(config), currentFrame_(0) {
    // Frame will be properly initialized when decode() is called
}

JpegDecoderBase::~JpegDecoderBase() {
    end();
}

bool JpegDecoderBase::begin(fl::ByteStreamPtr stream) {
    if (!stream) {
        setError("Invalid ByteStream provided");
        return false;
    }

    stream_ = stream;
    hasError_ = false;
    errorMessage_.clear();

    if (!initializeDecoder()) {
        return false;
    }

    ready_ = true;
    return true;
}

void JpegDecoderBase::end() {
    if (ready_) {
        cleanupDecoder();
        ready_ = false;
    }
    stream_.reset();
    frameBuffer_.reset();
}

bool JpegDecoderBase::hasError(fl::string* msg) const {
    if (msg && hasError_) {
        *msg = errorMessage_;
    }
    return hasError_;
}

DecodeResult JpegDecoderBase::decode() {
    if (!ready_) {
        return DecodeResult::Error;
    }

    if (hasError_) {
        return DecodeResult::Error;
    }

    if (!decodeInternal()) {
        return DecodeResult::Error;
    }

    return DecodeResult::Success;
}

void JpegDecoderBase::allocateFrameBuffer() {
    bufferSize_ = getExpectedFrameSize();
    if (bufferSize_ > 0) {
        frameBuffer_.reset(new fl::u8[bufferSize_]);
    }
}

void JpegDecoderBase::setError(const fl::string& message) {
    hasError_ = true;
    errorMessage_ = message;
    ready_ = false;
}

fl::size JpegDecoderBase::getExpectedFrameSize() const {
    if (currentFrame_.getWidth() == 0 || currentFrame_.getHeight() == 0) {
        return 0;
    }
    return static_cast<fl::size>(currentFrame_.getWidth()) * currentFrame_.getHeight() * getBytesPerPixel(config_.format);
}

} // namespace fl
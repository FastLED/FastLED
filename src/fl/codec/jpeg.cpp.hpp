#include "fl/codec/jpeg.h"
#include "fl/stl/unique_ptr.h"  // For make_unique
#include "third_party/TJpg_Decoder/driver.h"
#include "fl/stl/utility.h"
#include "fl/stl/vector.h"
#include "fl/fx/frame.h"
#include "fl/stl/stdio.h"
#include "fl/warn.h"
#include "fl/bytestreammemory.h"
#include "fl/codec/pixel.h"
#include "fl/stl/time.h"

namespace fl {

//////////////////////////////////////////////////////////////////////////
// JpegInfo Implementation
//////////////////////////////////////////////////////////////////////////

JpegInfo::JpegInfo(fl::u16 w, fl::u16 h, fl::u8 comp)
    : width(w), height(h), components(comp)
    , is_grayscale(comp == 1), is_valid(true) {}

//////////////////////////////////////////////////////////////////////////
// JpegConfig Implementation
//////////////////////////////////////////////////////////////////////////

JpegConfig::JpegConfig(Quality q, PixelFormat fmt)
    : quality(q), format(fmt) {}

//////////////////////////////////////////////////////////////////////////
// JpegDecoder::Impl - PIMPL Implementation
//////////////////////////////////////////////////////////////////////////

class JpegDecoder::Impl {
private:
    fl::third_party::TJpgInstanceDecoderPtr mDriver;
    JpegConfig mConfig;
    ProgressiveConfig progressive_mConfig;
    JpegDecoder::State mState;
    float mProgress;
    fl::string mErrorMessage;
    bool mHasError;

    void setError(const fl::string& message) {
        mHasError = true;
        mErrorMessage = message;
        mState = JpegDecoder::State::Error;
    }

    fl::u8 getScale() const {
        switch (mConfig.quality) {
            case JpegConfig::Quality::Low: return 3;     // 1/8 scale
            case JpegConfig::Quality::Medium: return 2;  // 1/4 scale
            case JpegConfig::Quality::High: return 0;    // Full scale (1:1)
        }
        return 0;
    }

public:
    explicit Impl(const JpegConfig& config)
        : mConfig(config), mState(JpegDecoder::State::NotStarted)
        , mProgress(0.0f), mHasError(false) {
        mDriver = fl::third_party::createTJpgInstanceDecoder();
    }

    ~Impl() = default;

    bool begin(fl::ByteStreamPtr stream) {
        if (!mDriver) {
            setError("Driver not initialized");
            return false;
        }

        mState = JpegDecoder::State::NotStarted;
        mHasError = false;
        mErrorMessage.clear();
        mProgress = 0.0f;

        // Configure progressive settings
        fl::third_party::TJpgProgressiveConfig mDriverconfig;
        mDriverconfig.max_mcus_per_tick = progressive_mConfig.max_mcus_per_tick;
        mDriverconfig.max_time_per_tick_ms = progressive_mConfig.max_time_per_tick_ms;
        mDriver->setProgressiveConfig(mDriverconfig);

        // Set scale based on quality setting
        mDriver->setScale(getScale());

        if (!mDriver->beginDecodingStream(stream, mConfig.format)) {
            fl::string err;
            if (mDriver->hasError(&err)) {
                setError(err);
            } else {
                setError("Failed to begin JPEG decoding");
            }
            return false;
        }

        mState = JpegDecoder::State::HeaderParsed;
        return true;
    }

    void end() {
        if (mDriver) {
            mDriver->endDecoding();
        }
        mState = JpegDecoder::State::NotStarted;
    }

    bool isReady() const {
        return mState == JpegDecoder::State::HeaderParsed ||
               mState == JpegDecoder::State::Decoding;
    }

    bool hasError(fl::string* msg = nullptr) const {
        if (msg && mHasError) {
            *msg = mErrorMessage;
        }
        return mHasError;
    }

    DecodeResult decode(fl::optional<fl::function<bool()>> should_yield) {
        if (mState == JpegDecoder::State::Error) {
            return DecodeResult::Error;
        }

        if (mState == JpegDecoder::State::Complete) {
            return DecodeResult::Success;
        }

        while (processChunk()) {
            if (should_yield && (*should_yield)()) {
                // Caller requested yield - return current state
                return (mState == JpegDecoder::State::Complete) ? DecodeResult::Success : DecodeResult::NeedsMoreData;
            }
        }

        return (mState == JpegDecoder::State::Complete) ? DecodeResult::Success : DecodeResult::Error;
    }

    Frame getCurrentFrame() {
        return mDriver ? mDriver->getCurrentFrame() : Frame(0);
    }

    bool hasMoreFrames() const { return false; } // JPEG is single frame

    void setProgressiveConfig(const ProgressiveConfig& config) {
        progressive_mConfig = config;
        if (mDriver) {
            fl::third_party::TJpgProgressiveConfig mDriverconfig;
            mDriverconfig.max_mcus_per_tick = config.max_mcus_per_tick;
            mDriverconfig.max_time_per_tick_ms = config.max_time_per_tick_ms;
            mDriver->setProgressiveConfig(mDriverconfig);
        }
    }

    bool processChunk() {
        if (mState == JpegDecoder::State::Error || mState == JpegDecoder::State::Complete) {
            return false;
        }

        if (mState == JpegDecoder::State::NotStarted || mState == JpegDecoder::State::HeaderParsed) {
            mState = JpegDecoder::State::Decoding;
        }

        if (!mDriver) {
            setError("Driver not available");
            return false;
        }

        bool more_work = mDriver->processChunk();

        auto mDriverstate = mDriver->getState();
        switch (mDriverstate) {
            case fl::third_party::TJpgInstanceDecoder::State::NotStarted:
            case fl::third_party::TJpgInstanceDecoder::State::HeaderParsed:
            case fl::third_party::TJpgInstanceDecoder::State::Decoding:
                mProgress = mDriver->getProgress();
                break;
            case fl::third_party::TJpgInstanceDecoder::State::Complete:
                mState = JpegDecoder::State::Complete;
                mProgress = 1.0f;
                return false;
            case fl::third_party::TJpgInstanceDecoder::State::Error: {
                fl::string err;
                if (mDriver->hasError(&err)) {
                    setError(err);
                } else {
                    setError("JPEG decoding failed");
                }
                return false;
            }
        }

        return more_work;
    }

    float getProgress() const { return mProgress; }
    bool hasPartialImage() const { return mDriver ? mDriver->hasPartialImage() : false; }
    Frame getPartialFrame() { return mDriver ? mDriver->getPartialFrame() : Frame(0); }
    fl::u16 getDecodedRows() const { return mDriver ? mDriver->getDecodedRows() : 0; }
    bool feedData(fl::span<const fl::u8> data) { (void)data; return false; } // Not implemented
    bool needsMoreData() const { return false; } // Not implemented
    fl::size getBytesProcessed() const { return mDriver ? mDriver->getBytesProcessed() : 0; }
    JpegDecoder::State getState() const { return mState; }
    ProgressiveConfig getProgressiveConfig() const { return progressive_mConfig; }
};

//////////////////////////////////////////////////////////////////////////
// JpegDecoder Implementation
//////////////////////////////////////////////////////////////////////////

JpegDecoder::JpegDecoder(const JpegConfig& config)
    : mImpl(fl::make_unique<Impl>(config)) {}

JpegDecoder::~JpegDecoder() = default;

bool JpegDecoder::begin(fl::ByteStreamPtr stream) {
    return mImpl->begin(stream);
}

void JpegDecoder::end() {
    mImpl->end();
}

bool JpegDecoder::isReady() const {
    return mImpl->isReady();
}

bool JpegDecoder::hasError(fl::string* msg) const {
    return mImpl->hasError(msg);
}

DecodeResult JpegDecoder::decode() {
    return mImpl->decode(fl::nullopt); // No callback - process to completion
}

DecodeResult JpegDecoder::decode(fl::optional<fl::function<bool()>> should_yield) {
    return mImpl->decode(should_yield);
}

Frame JpegDecoder::getCurrentFrame() {
    return mImpl->getCurrentFrame();
}

bool JpegDecoder::hasMoreFrames() const {
    return mImpl->hasMoreFrames();
}

void JpegDecoder::setProgressiveConfig(const ProgressiveConfig& config) {
    mImpl->setProgressiveConfig(config);
}

ProgressiveConfig JpegDecoder::getProgressiveConfig() const {
    return mImpl->getProgressiveConfig();
}

float JpegDecoder::getProgress() const {
    return mImpl->getProgress();
}

bool JpegDecoder::hasPartialImage() const {
    return mImpl->hasPartialImage();
}

Frame JpegDecoder::getPartialFrame() {
    return mImpl->getPartialFrame();
}

fl::u16 JpegDecoder::getDecodedRows() const {
    return mImpl->getDecodedRows();
}

bool JpegDecoder::feedData(fl::span<const fl::u8> data) {
    return mImpl->feedData(data);
}

bool JpegDecoder::needsMoreData() const {
    return mImpl->needsMoreData();
}

fl::size JpegDecoder::getBytesProcessed() const {
    return mImpl->getBytesProcessed();
}

JpegDecoder::State JpegDecoder::getState() const {
    return mImpl->getState();
}

//////////////////////////////////////////////////////////////////////////
// Jpeg Static Methods Implementation
//////////////////////////////////////////////////////////////////////////

bool Jpeg::decode(const JpegConfig& config, fl::span<const fl::u8> data, Frame* frame, fl::string* error_message) {
    if (!frame) {
        if (error_message) {
            *error_message = "Frame pointer is null";
        }
        return false;
    }

    if (!frame->isFromCodec() || frame->getWidth() == 0 || frame->getHeight() == 0) {
        if (error_message) {
            *error_message = "Target frame must be created with proper dimensions for in-place decoding";
        }
        return false;
    }

    auto decoder = createDecoder(config);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(data.size());
    stream->write(data.data(), data.size());

    if (!decoder->begin(stream)) {
        if (error_message) {
            decoder->hasError(error_message);
        }
        return false;
    }

    DecodeResult result = decoder->decode();
    if (result != DecodeResult::Success) {
        if (error_message) {
            decoder->hasError(error_message);
        }
        return false;
    }

    Frame decoded = decoder->getCurrentFrame();

    if (frame->getWidth() != decoded.getWidth() || frame->getHeight() != decoded.getHeight()) {
        if (error_message) {
            *error_message = "Target frame dimensions do not match decoded image dimensions";
        }
        return false;
    }

    frame->copy(decoded);
    return true;
}

FramePtr Jpeg::decode(const JpegConfig& config, fl::span<const fl::u8> data, fl::string* error_message) {
    auto decoder = createDecoder(config);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(data.size());
    stream->write(data.data(), data.size());

    if (!decoder->begin(stream)) {
        if (error_message) {
            decoder->hasError(error_message);
        }
        return nullptr;
    }

    DecodeResult result = decoder->decode();
    if (result != DecodeResult::Success) {
        if (error_message) {
            decoder->hasError(error_message);
        }
        return nullptr;
    }

    Frame frame = decoder->getCurrentFrame();
    return frame.isValid() ? fl::make_shared<Frame>(frame) : nullptr;
}

FramePtr Jpeg::decode(fl::span<const fl::u8> data, fl::string* error_message) {
    JpegConfig config; // Uses defaults
    return decode(config, data, error_message);
}

JpegDecoderPtr Jpeg::createDecoder(const JpegConfig& config) {
    return fl::make_shared<JpegDecoder>(config);
}

bool Jpeg::isSupported() {
    return true; // TJpg decoder is always supported
}

bool Jpeg::decodeWithTimeout(
    const JpegConfig& config,
    fl::span<const fl::u8> data,
    Frame* frame,
    fl::u32 timeout_ms,
    float* mProgressout,
    fl::string* error_message) {

    if (!frame) {
        if (error_message) {
            *error_message = "Frame pointer is null";
        }
        return false;
    }

    auto decoder = createDecoder(config);
    auto stream = fl::make_shared<fl::ByteStreamMemory>(data.size());
    stream->write(data.data(), data.size());

    if (!decoder->begin(stream)) {
        if (error_message) {
            decoder->hasError(error_message);
        }
        return false;
    }

    fl::u32 start_time = fl::millis();
    fl::u32 deadline = start_time + timeout_ms;

    // Use callback-based decode with time budget check
    DecodeResult result = decoder->decode(fl::function<bool()>([&]() {
        if (mProgressout) {
            *mProgressout = decoder->getProgress();
        }
        return fl::millis() >= deadline; // Yield if time budget exceeded
    }));

    if (result == DecodeResult::Success) {
        Frame decoded = decoder->getCurrentFrame();

        if (frame->getWidth() != decoded.getWidth() || frame->getHeight() != decoded.getHeight()) {
            if (error_message) {
                *error_message = "Target frame dimensions do not match decoded image dimensions";
            }
            return false;
        }

        frame->copy(decoded);
        return true;
    } else if (result == DecodeResult::Error || decoder->hasError()) {
        if (error_message) {
            decoder->hasError(error_message);
        }
        return false;
    }

    // Partial completion due to timeout
    if (mProgressout) {
        *mProgressout = decoder->getProgress();
    }
    return false; // Not complete yet
}

bool Jpeg::decodeStream(
    const JpegConfig& config,
    fl::ByteStreamPtr input_stream,
    Frame* frame,
    fl::u32 max_time_per_chunk_ms,
    fl::function<bool(float)> mProgresscallback) {

    if (!frame || !input_stream) {
        return false;
    }

    auto decoder = createDecoder(config);

    ProgressiveConfig prog_config;
    prog_config.max_time_per_tick_ms = max_time_per_chunk_ms;
    decoder->setProgressiveConfig(prog_config);

    if (!decoder->begin(input_stream)) {
        return false;
    }

    // Use callback-based decode with progress notifications
    fl::optional<fl::function<bool()>> yield_func;
    if (mProgresscallback) {
        yield_func = fl::function<bool()>([&]() {
            float progress = decoder->getProgress();
            return !mProgresscallback(progress); // Yield if callback returns false
        });
    }
    DecodeResult result = decoder->decode(yield_func);

    if (result == DecodeResult::Success) {
        Frame decoded = decoder->getCurrentFrame();

        if (frame->getWidth() != decoded.getWidth() || frame->getHeight() != decoded.getHeight()) {
            return false;
        }

        frame->copy(decoded);
        return true;
    }

    return false;
}

JpegInfo Jpeg::parseInfo(fl::span<const fl::u8> data, fl::string* error_message) {
    (void)data;
    (void)error_message;
    // TODO: Implement JPEG header parsing
    return JpegInfo();
}

} // namespace fl
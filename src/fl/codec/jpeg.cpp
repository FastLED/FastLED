#include "fl/codec/jpeg.h"
#include "third_party/TJpg_Decoder/driver.h"
#include "fl/utility.h"
#include "fl/vector.h"
#include "fx/frame.h"
#include "fl/printf.h"
#include "fl/warn.h"
#include "fl/bytestreammemory.h"
#include "fl/codec/pixel.h"
#include "fl/time.h"

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
    fl::third_party::TJpgInstanceDecoderPtr driver_;
    JpegConfig config_;
    ProgressiveConfig progressive_config_;
    JpegDecoder::State state_;
    float progress_;
    fl::string error_message_;
    bool has_error_;

    void setError(const fl::string& message) {
        has_error_ = true;
        error_message_ = message;
        state_ = JpegDecoder::State::Error;
    }

    fl::u8 getScale() const {
        switch (config_.quality) {
            case JpegConfig::Quality::Low: return 3;     // 1/8 scale
            case JpegConfig::Quality::Medium: return 2;  // 1/4 scale
            case JpegConfig::Quality::High: return 0;    // Full scale (1:1)
        }
        return 0;
    }

public:
    explicit Impl(const JpegConfig& config)
        : config_(config), state_(JpegDecoder::State::NotStarted)
        , progress_(0.0f), has_error_(false) {
        driver_ = fl::third_party::createTJpgInstanceDecoder();
    }

    ~Impl() = default;

    bool begin(fl::ByteStreamPtr stream) {
        if (!driver_) {
            setError("Driver not initialized");
            return false;
        }

        state_ = JpegDecoder::State::NotStarted;
        has_error_ = false;
        error_message_.clear();
        progress_ = 0.0f;

        // Configure progressive settings
        fl::third_party::TJpgProgressiveConfig driver_config;
        driver_config.max_mcus_per_tick = progressive_config_.max_mcus_per_tick;
        driver_config.max_time_per_tick_ms = progressive_config_.max_time_per_tick_ms;
        driver_->setProgressiveConfig(driver_config);

        // Set scale based on quality setting
        driver_->setScale(getScale());

        if (!driver_->beginDecodingStream(stream, config_.format)) {
            fl::string err;
            if (driver_->hasError(&err)) {
                setError(err);
            } else {
                setError("Failed to begin JPEG decoding");
            }
            return false;
        }

        state_ = JpegDecoder::State::HeaderParsed;
        return true;
    }

    void end() {
        if (driver_) {
            driver_->endDecoding();
        }
        state_ = JpegDecoder::State::NotStarted;
    }

    bool isReady() const {
        return state_ == JpegDecoder::State::HeaderParsed ||
               state_ == JpegDecoder::State::Decoding;
    }

    bool hasError(fl::string* msg = nullptr) const {
        if (msg && has_error_) {
            *msg = error_message_;
        }
        return has_error_;
    }

    DecodeResult decode(fl::optional<fl::function<bool()>> should_yield) {
        if (state_ == JpegDecoder::State::Error) {
            return DecodeResult::Error;
        }

        if (state_ == JpegDecoder::State::Complete) {
            return DecodeResult::Success;
        }

        while (processChunk()) {
            if (should_yield && (*should_yield)()) {
                // Caller requested yield - return current state
                return (state_ == JpegDecoder::State::Complete) ? DecodeResult::Success : DecodeResult::NeedsMoreData;
            }
        }

        return (state_ == JpegDecoder::State::Complete) ? DecodeResult::Success : DecodeResult::Error;
    }

    Frame getCurrentFrame() {
        return driver_ ? driver_->getCurrentFrame() : Frame(0);
    }

    bool hasMoreFrames() const { return false; } // JPEG is single frame

    void setProgressiveConfig(const ProgressiveConfig& config) {
        progressive_config_ = config;
        if (driver_) {
            fl::third_party::TJpgProgressiveConfig driver_config;
            driver_config.max_mcus_per_tick = config.max_mcus_per_tick;
            driver_config.max_time_per_tick_ms = config.max_time_per_tick_ms;
            driver_->setProgressiveConfig(driver_config);
        }
    }

    bool processChunk() {
        if (state_ == JpegDecoder::State::Error || state_ == JpegDecoder::State::Complete) {
            return false;
        }

        if (state_ == JpegDecoder::State::NotStarted || state_ == JpegDecoder::State::HeaderParsed) {
            state_ = JpegDecoder::State::Decoding;
        }

        if (!driver_) {
            setError("Driver not available");
            return false;
        }

        bool more_work = driver_->processChunk();

        auto driver_state = driver_->getState();
        switch (driver_state) {
            case fl::third_party::TJpgInstanceDecoder::State::NotStarted:
            case fl::third_party::TJpgInstanceDecoder::State::HeaderParsed:
            case fl::third_party::TJpgInstanceDecoder::State::Decoding:
                progress_ = driver_->getProgress();
                break;
            case fl::third_party::TJpgInstanceDecoder::State::Complete:
                state_ = JpegDecoder::State::Complete;
                progress_ = 1.0f;
                return false;
            case fl::third_party::TJpgInstanceDecoder::State::Error: {
                fl::string err;
                if (driver_->hasError(&err)) {
                    setError(err);
                } else {
                    setError("JPEG decoding failed");
                }
                return false;
            }
        }

        return more_work;
    }

    float getProgress() const { return progress_; }
    bool hasPartialImage() const { return driver_ ? driver_->hasPartialImage() : false; }
    Frame getPartialFrame() { return driver_ ? driver_->getPartialFrame() : Frame(0); }
    fl::u16 getDecodedRows() const { return driver_ ? driver_->getDecodedRows() : 0; }
    bool feedData(fl::span<const fl::u8> data) { (void)data; return false; } // Not implemented
    bool needsMoreData() const { return false; } // Not implemented
    fl::size getBytesProcessed() const { return driver_ ? driver_->getBytesProcessed() : 0; }
    JpegDecoder::State getState() const { return state_; }
    ProgressiveConfig getProgressiveConfig() const { return progressive_config_; }
};

//////////////////////////////////////////////////////////////////////////
// JpegDecoder Implementation
//////////////////////////////////////////////////////////////////////////

JpegDecoder::JpegDecoder(const JpegConfig& config)
    : impl_(fl::make_unique<Impl>(config)) {}

JpegDecoder::~JpegDecoder() = default;

bool JpegDecoder::begin(fl::ByteStreamPtr stream) {
    return impl_->begin(stream);
}

void JpegDecoder::end() {
    impl_->end();
}

bool JpegDecoder::isReady() const {
    return impl_->isReady();
}

bool JpegDecoder::hasError(fl::string* msg) const {
    return impl_->hasError(msg);
}

DecodeResult JpegDecoder::decode() {
    return impl_->decode(fl::nullopt); // No callback - process to completion
}

DecodeResult JpegDecoder::decode(fl::optional<fl::function<bool()>> should_yield) {
    return impl_->decode(should_yield);
}

Frame JpegDecoder::getCurrentFrame() {
    return impl_->getCurrentFrame();
}

bool JpegDecoder::hasMoreFrames() const {
    return impl_->hasMoreFrames();
}

void JpegDecoder::setProgressiveConfig(const ProgressiveConfig& config) {
    impl_->setProgressiveConfig(config);
}

ProgressiveConfig JpegDecoder::getProgressiveConfig() const {
    return impl_->getProgressiveConfig();
}

float JpegDecoder::getProgress() const {
    return impl_->getProgress();
}

bool JpegDecoder::hasPartialImage() const {
    return impl_->hasPartialImage();
}

Frame JpegDecoder::getPartialFrame() {
    return impl_->getPartialFrame();
}

fl::u16 JpegDecoder::getDecodedRows() const {
    return impl_->getDecodedRows();
}

bool JpegDecoder::feedData(fl::span<const fl::u8> data) {
    return impl_->feedData(data);
}

bool JpegDecoder::needsMoreData() const {
    return impl_->needsMoreData();
}

fl::size JpegDecoder::getBytesProcessed() const {
    return impl_->getBytesProcessed();
}

JpegDecoder::State JpegDecoder::getState() const {
    return impl_->getState();
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
    float* progress_out,
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

    fl::u32 start_time = fl::time();
    fl::u32 deadline = start_time + timeout_ms;

    // Use callback-based decode with time budget check
    DecodeResult result = decoder->decode(fl::function<bool()>([&]() {
        if (progress_out) {
            *progress_out = decoder->getProgress();
        }
        return fl::time() >= deadline; // Yield if time budget exceeded
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
    if (progress_out) {
        *progress_out = decoder->getProgress();
    }
    return false; // Not complete yet
}

bool Jpeg::decodeStream(
    const JpegConfig& config,
    fl::ByteStreamPtr input_stream,
    Frame* frame,
    fl::u32 max_time_per_chunk_ms,
    fl::function<bool(float)> progress_callback) {

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
    if (progress_callback) {
        yield_func = fl::function<bool()>([&]() {
            float progress = decoder->getProgress();
            return !progress_callback(progress); // Yield if callback returns false
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
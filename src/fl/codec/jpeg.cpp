#include "fl/codec/jpeg.h"
#include "third_party/TJpg_Decoder/src/TJpg_Decoder.h"
#include "fl/utility.h"
#include "fl/vector.h"
#include "fx/frame.h"
#include "fl/thread_local.h"
#include "fl/printf.h"
#include "fl/warn.h"
#include "fl/bytestreammemory.h"
#include "fl/codec/pixel.h"

namespace fl {

// TJpg decoder implementation
class TJpgDecoder : public IDecoder {
private:
    // Configuration and state
    JpegDecoderConfig config_;
    fl::ByteStreamPtr stream_;
    fl::string errorMessage_;
    bool ready_ = false;
    bool hasError_ = false;

    // TJpg decoder specific - removed member instance, will use global TJpgDec
    fl::scoped_array<fl::u8> inputBuffer_;
    size_t inputSize_;
    bool frameDecoded_;
    fl::shared_ptr<Frame> decodedFrame_;

    // Internal buffer management
    fl::scoped_array<fl::u8> frameBuffer_;
    fl::size bufferSize_ = 0;

    // Static callback for TJpg_Decoder output
    static bool outputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* data);

    // Internal helper methods
    bool initializeDecoder();
    bool decodeInternal();
    void cleanupDecoder();
    void mapQualityToScale();
    void convertPixelFormat(uint16_t* srcData, fl::u8* dstData, uint16_t w, uint16_t h);
    fl::size getBytesPerPixel() const;
    void allocateFrameBuffer(uint16_t width, uint16_t height);
    void setError(const fl::string& message);
    fl::size getExpectedFrameSize() const;

public:
    explicit TJpgDecoder(const JpegDecoderConfig& config);
    ~TJpgDecoder() override;

    // IDecoder interface
    bool begin(fl::ByteStreamPtr stream) override;
    void end() override;
    bool isReady() const override { return ready_; }
    bool hasError(fl::string* msg = nullptr) const override;
    DecodeResult decode() override;
    Frame getCurrentFrame() override;
    bool hasMoreFrames() const override { return false; } // JPEG is single frame

    // Additional method to get frame pointer directly
    fl::shared_ptr<Frame> getFramePtr() const { return decodedFrame_; }
};

// JPEG class implementation

bool Jpeg::decode(const JpegDecoderConfig& config, fl::span<const fl::u8> data, Frame* frame, fl::string* error_message) {
    if (!frame) {
        if (error_message) {
            *error_message = "Frame pointer is null";
        }
        return false;
    }

    // Check if frame has codec dimensions (width/height > 0)
    if (!frame->isFromCodec() || frame->getWidth() == 0 || frame->getHeight() == 0) {
        if (error_message) {
            *error_message = "Target frame must be created with proper dimensions for in-place decoding";
        }
        return false;
    }

    auto decoder = fl::make_shared<TJpgDecoder>(config);
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

    // Check if dimensions match
    if (frame->getWidth() != decoded.getWidth() || frame->getHeight() != decoded.getHeight()) {
        if (error_message) {
            *error_message = "Target frame dimensions do not match decoded image dimensions";
        }
        return false;
    }

    frame->copy(decoded);
    return true;
}

FramePtr Jpeg::decode(const JpegDecoderConfig& config, fl::span<const fl::u8> data, fl::string* error_message) {
    auto decoder = fl::make_shared<TJpgDecoder>(config);
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

    return decoder->getFramePtr();
}

bool Jpeg::isSupported() {
    // TJpg_Decoder is now integrated
    return true;
}


// Thread-local variable to hold current decoder instance for callback
static fl::ThreadLocal<TJpgDecoder*> currentDecoder(nullptr);

// Debug flag to track if callback was invoked
static bool g_callbackInvoked = false;

TJpgDecoder::TJpgDecoder(const JpegDecoderConfig& config)
    : config_(config), inputSize_(0), frameDecoded_(false) {
}

TJpgDecoder::~TJpgDecoder() {
    end();
}

bool TJpgDecoder::begin(fl::ByteStreamPtr stream) {
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

void TJpgDecoder::end() {
    if (ready_) {
        cleanupDecoder();
        ready_ = false;
    }
    stream_.reset();
    frameBuffer_.reset();
}

bool TJpgDecoder::hasError(fl::string* msg) const {
    if (msg && hasError_) {
        *msg = errorMessage_;
    }
    return hasError_;
}

DecodeResult TJpgDecoder::decode() {
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

bool TJpgDecoder::initializeDecoder() {
    if (!stream_) {
        setError("No input stream provided");
        return false;
    }

    // Read all data from stream into buffer
    fl::vector<fl::u8> tempBuffer;
    tempBuffer.reserve(4096); // Initial capacity

    fl::u8 chunk[256];
    size_t totalBytesRead = 0;
    while (true) {
        fl::size bytesRead = stream_->read(chunk, sizeof(chunk));
        if (bytesRead == 0) break;

        // Append read data to buffer
        size_t oldSize = tempBuffer.size();
        tempBuffer.resize(oldSize + bytesRead);
        memcpy(tempBuffer.data() + oldSize, chunk, bytesRead);
        totalBytesRead += bytesRead;
    }

    inputSize_ = tempBuffer.size();
    if (inputSize_ == 0) {
        setError("Empty input stream - read 0 bytes total");
        return false;
    }
    if (inputSize_ != totalBytesRead) {
        setError("Size mismatch in reading");
        return false;
    }

    inputBuffer_.reset(new fl::u8[inputSize_]);
    if (!inputBuffer_) {
        setError("Failed to allocate input buffer");
        return false;
    }

    // Copy data from temp buffer to allocated buffer
    memcpy(inputBuffer_.get(), tempBuffer.data(), inputSize_);

    // Get image dimensions first
    uint16_t width, height;
    fl::third_party::JRESULT result = fl::third_party::TJpgDec.getJpgSize(&width, &height, inputBuffer_.get(), inputSize_);
    if (result != fl::third_party::JDR_OK) {
        char error_code_str[16];
        fl::snprintf(error_code_str, sizeof(error_code_str), "%d", static_cast<int>(result));
        fl::string error_msg = "Failed to parse JPEG header, error code: ";
        error_msg += error_code_str;
        setError(error_msg);
        return false;
    }

    // Validate dimensions
    if (width > config_.maxWidth || height > config_.maxHeight) {
        setError("Image dimensions exceed maximum allowed size");
        return false;
    }

    // Initialize frame buffer and create Frame immediately
    allocateFrameBuffer(width, height);

    // Create frame with our allocated buffer
    decodedFrame_ = fl::make_shared<Frame>(frameBuffer_.get(), width, height, config_.format);

    // Set up decoder configuration
    mapQualityToScale();
    fl::third_party::TJpgDec.setSwapBytes(false); // We'll handle byte order ourselves
    fl::third_party::TJpgDec.setCallback(outputCallback);

    frameDecoded_ = false;
    return true;
}

bool TJpgDecoder::decodeInternal() {
    if (frameDecoded_) {
        return true; // Already decoded
    }

    // Set current decoder for static callback
    currentDecoder.set(this);
    g_callbackInvoked = false;  // Reset debug flag

    // Re-set the callback right before decoding (in case it was lost)
    fl::third_party::TJpgDec.setCallback(outputCallback);


    // Decode the JPEG
    fl::third_party::JRESULT result = fl::third_party::TJpgDec.drawJpg(0, 0, inputBuffer_.get(), inputSize_);


    currentDecoder.set(nullptr);

    if (result != fl::third_party::JDR_OK) {
        char error_code_str[16];
        fl::snprintf(error_code_str, sizeof(error_code_str), "%d", static_cast<int>(result));
        fl::string error_msg = "JPEG decoding failed, error code: ";
        error_msg += error_code_str;
        setError(error_msg);
        return false;
    }

    // Check if callback was invoked
    if (!g_callbackInvoked) {
        setError("JPEG decoder callback was never invoked - no pixel data transferred");
        return false;
    }


    frameDecoded_ = true;
    return true;
}

void TJpgDecoder::cleanupDecoder() {
    inputBuffer_.reset();
    inputSize_ = 0;
    frameDecoded_ = false;
    decodedFrame_.reset();
    currentDecoder.set(nullptr);
}

bool TJpgDecoder::outputCallback(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* data) {
    g_callbackInvoked = true;  // Mark that callback was called

    TJpgDecoder* decoder = currentDecoder.access();
    if (!decoder) {
        return false;
    }


    if (!decoder->frameBuffer_ || !decoder->decodedFrame_) {
        // Set error for debugging
        decoder->setError("Output callback called but frame buffer or decoded frame is null");
        return false;
    }

    if (!data) {
        decoder->setError("Output callback called with null data pointer");
        return false;
    }

    uint16_t frameWidth = decoder->decodedFrame_->getWidth();
    uint16_t frameHeight = decoder->decodedFrame_->getHeight();

    // Bounds checking
    if (x < 0 || y < 0 || x + w > frameWidth || y + h > frameHeight) {
        decoder->setError("Output callback bounds check failed");
        return false;
    }

    // Write directly to the Frame's RGB buffer
    CRGB* framePixels = decoder->decodedFrame_->rgb();

    for (uint16_t row = 0; row < h; ++row) {
        for (uint16_t col = 0; col < w; ++col) {
            uint16_t srcIdx = row * w + col;

            // Calculate destination position in Frame buffer
            int pixelX = x + col;
            int pixelY = y + row;
            int frameIdx = pixelY * frameWidth + pixelX;

            // Convert RGB565 pixel to CRGB using rgb565ToRgb888 function with lookup tables
            uint16_t rgb565 = data[srcIdx];
            fl::u8 r, g, b;
            rgb565ToRgb888(rgb565, r, g, b);

            // Set pixel directly in the Frame's buffer
            framePixels[frameIdx] = CRGB(r, g, b);
        }
    }

    return true;
}

void TJpgDecoder::mapQualityToScale() {
    fl::u8 scale = 1;
    switch (config_.quality) {
        case JpegDecoderConfig::Quality::Low:
            scale = 8;
            break;
        case JpegDecoderConfig::Quality::Medium:
            scale = 4;
            break;
        case JpegDecoderConfig::Quality::High:
            scale = 1;
            break;
    }
    fl::third_party::TJpgDec.setJpgScale(scale);
}

void TJpgDecoder::convertPixelFormat(uint16_t* srcData, fl::u8* dstData, uint16_t w, uint16_t h) {
    (void)w; (void)h; // Suppress unused parameter warnings

    uint16_t rgb565 = *srcData;

    // Convert RGB565 to RGB888 using lookup tables
    fl::u8 r, g, b;
    rgb565ToRgb888(rgb565, r, g, b);

    switch (config_.format) {
        case PixelFormat::RGB888:
            dstData[0] = r;
            dstData[1] = g;
            dstData[2] = b;
            break;
        case PixelFormat::RGBA8888:
            dstData[0] = r;
            dstData[1] = g;
            dstData[2] = b;
            dstData[3] = 255; // Full alpha
            break;
        case PixelFormat::RGB565:
            *reinterpret_cast<fl::u16*>(dstData) = rgb565;
            break;
        case PixelFormat::YUV420:
            // Simple conversion to grayscale for YUV420
            dstData[0] = static_cast<fl::u8>(0.299f * r + 0.587f * g + 0.114f * b);
            break;
        default:
            // Default to RGB888
            dstData[0] = r;
            dstData[1] = g;
            dstData[2] = b;
            break;
    }
}

fl::size TJpgDecoder::getBytesPerPixel() const {
    return fl::getBytesPerPixel(config_.format);
}

void TJpgDecoder::setError(const fl::string& message) {
    hasError_ = true;
    errorMessage_ = message;
    ready_ = false;
}

fl::size TJpgDecoder::getExpectedFrameSize() const {
    if (!decodedFrame_ || decodedFrame_->getWidth() == 0 || decodedFrame_->getHeight() == 0) {
        return 0;
    }
    return static_cast<fl::size>(decodedFrame_->getWidth()) * decodedFrame_->getHeight() * getBytesPerPixel();
}

void TJpgDecoder::allocateFrameBuffer(uint16_t width, uint16_t height) {
    bufferSize_ = static_cast<fl::size>(width) * height * getBytesPerPixel();
    if (bufferSize_ > 0) {
        frameBuffer_.reset(new fl::u8[bufferSize_]);
        // Initialize buffer to zero to prevent garbage values
        if (frameBuffer_) {
            memset(frameBuffer_.get(), 0, bufferSize_);
        }
    }
}

Frame TJpgDecoder::getCurrentFrame() {
    if (decodedFrame_) {
        return *decodedFrame_;
    }
    // Return empty frame if not decoded yet
    return Frame(0);
}

} // namespace fl
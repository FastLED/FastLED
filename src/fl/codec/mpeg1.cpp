#include "fl/codec/mpeg1.h"
#include "fl/utility.h"
#include "fl/str.h"
#include "fl/compiler_control.h"

// Include stdio for FILE type needed by pl_mpeg
#include <stdio.h>  // ok include
#include <string.h>  // for memcpy (C header for Arduino compatibility)

// Suppress warnings from pl_mpeg for embedded systems
FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING(overflow)
FL_DISABLE_WARNING(narrowing)
FL_DISABLE_WARNING(shift-count-overflow)

#include "third_party/pl_mpeg/driver.h"

FL_DISABLE_WARNING_POP

namespace fl {

// MPEG1 factory implementation
namespace mpeg1 {

fl::shared_ptr<IDecoder> createDecoder(const Mpeg1Config& config, fl::string* error_message) {
    // MPEG1 is currently supported via software decoder on all platforms
    return fl::make_shared<SoftwareMpeg1Decoder>(config);
}

bool isSupported() {
    // Software MPEG1 decoder is available on all platforms
    return true;
}

} // namespace mpeg1

// Helper function to convert YUV to RGB
static void yuv_to_rgb(const fl::third_party::plm_frame_t* frame, fl::u8* rgb_buffer) {
    fl::u32 width = frame->width;
    fl::u32 height = frame->height;

    // YUV to RGB conversion coefficients
    const fl::i32 YUV_TO_RGB_MATRIX[9] = {
        1164,    0, 1596,  // Y, U, V coefficients for R
        1164, -391, -813,  // Y, U, V coefficients for G
        1164, 2018,    0   // Y, U, V coefficients for B
    };

    for (fl::u32 y = 0; y < height; y++) {
        for (fl::u32 x = 0; x < width; x++) {
            // Get Y, U, V values
            fl::u32 y_index = y * frame->y.width + x;
            fl::u32 uv_x = x / 2;
            fl::u32 uv_y = y / 2;
            fl::u32 uv_index = uv_y * frame->cr.width + uv_x;

            fl::i32 Y = frame->y.data[y_index] - 16;
            fl::i32 U = frame->cb.data[uv_index] - 128;
            fl::i32 V = frame->cr.data[uv_index] - 128;

            // Convert to RGB
            fl::i32 R = (YUV_TO_RGB_MATRIX[0] * Y + YUV_TO_RGB_MATRIX[1] * U + YUV_TO_RGB_MATRIX[2] * V) / 1000;
            fl::i32 G = (YUV_TO_RGB_MATRIX[3] * Y + YUV_TO_RGB_MATRIX[4] * U + YUV_TO_RGB_MATRIX[5] * V) / 1000;
            fl::i32 B = (YUV_TO_RGB_MATRIX[6] * Y + YUV_TO_RGB_MATRIX[7] * U + YUV_TO_RGB_MATRIX[8] * V) / 1000;

            // Clamp values
            R = R < 0 ? 0 : (R > 255 ? 255 : R);
            G = G < 0 ? 0 : (G > 255 ? 255 : G);
            B = B < 0 ? 0 : (B > 255 ? 255 : B);

            // Store RGB values
            fl::u32 rgb_index = (y * width + x) * 3;
            rgb_buffer[rgb_index + 0] = static_cast<fl::u8>(R);
            rgb_buffer[rgb_index + 1] = static_cast<fl::u8>(G);
            rgb_buffer[rgb_index + 2] = static_cast<fl::u8>(B);
        }
    }
}

// MPEG1 decoder internal data structure
struct SoftwareMpeg1Decoder::Mpeg1DecoderData {
    // pl_mpeg decoder instance
    fl::third_party::plm_t* plmpeg = nullptr;

    // Video properties
    fl::u16 width = 0;
    fl::u16 height = 0;
    fl::u16 frameRate = 0;

    // Input buffer for pl_mpeg
    fl::scoped_array<fl::u8> inputBuffer;
    fl::size inputSize = 0;
    fl::size totalSize = 0;

    // Current decoded frame data (RGB)
    fl::scoped_array<fl::u8> rgbFrameBuffer;
    fl::size rgbFrameSize = 0;

    // Decoder state
    bool headerParsed = false;
    bool initialized = false;
    bool hasNewFrame = false;

    // Frame timing
    double lastFrameTime = 0.0;
    double targetFrameDuration = 1.0/30.0; // Default 30fps
};

// Static callback wrapper for pl_mpeg
void SoftwareMpeg1Decoder::videoDecodeCallback(void* plm_ptr, void* frame_ptr, void* user) {
    auto* decoder = static_cast<SoftwareMpeg1Decoder*>(user);
    auto* frame = static_cast<fl::third_party::plm_frame_t*>(frame_ptr);

    if (decoder && decoder->decoderData_ && frame) {
        decoder->decoderData_->hasNewFrame = true;
        decoder->decoderData_->lastFrameTime = frame->time;

        // Convert YUV to RGB and store in buffer
        if (decoder->decoderData_->rgbFrameBuffer.get()) {
            yuv_to_rgb(frame, decoder->decoderData_->rgbFrameBuffer.get());
        }
    }
}

SoftwareMpeg1Decoder::SoftwareMpeg1Decoder(const Mpeg1Config& config)
    : config_(config)
    , decoderData_(new Mpeg1DecoderData()) {
    // Set target frame duration based on config
    if (config_.targetFps > 0) {
        decoderData_->targetFrameDuration = 1.0 / config_.targetFps;
    }
}

SoftwareMpeg1Decoder::~SoftwareMpeg1Decoder() {
    end();
    delete decoderData_;
}

bool SoftwareMpeg1Decoder::begin(fl::ByteStreamPtr stream) {
    if (!stream) {
        setError("Invalid ByteStream provided");
        return false;
    }

    stream_ = stream;
    hasError_ = false;
    errorMessage_.clear();
    endOfStream_ = false;
    currentFrameIndex_ = 0;

    if (!initializeDecoder()) {
        return false;
    }

    ready_ = true;
    return true;
}

void SoftwareMpeg1Decoder::end() {
    if (ready_) {
        cleanupDecoder();
        ready_ = false;
    }
    stream_.reset();
}

bool SoftwareMpeg1Decoder::hasError(fl::string* msg) const {
    if (msg && hasError_) {
        *msg = errorMessage_;
    }
    return hasError_;
}

DecodeResult SoftwareMpeg1Decoder::decode() {
    if (!ready_) {
        return DecodeResult::Error;
    }

    if (hasError_) {
        return DecodeResult::Error;
    }

    if (endOfStream_) {
        return DecodeResult::EndOfStream;
    }

    if (!decodeNextFrame()) {
        if (!hasError_) {
            endOfStream_ = true;
            return DecodeResult::EndOfStream;
        }
        return DecodeResult::Error;
    }

    return DecodeResult::Success;
}

Frame SoftwareMpeg1Decoder::getCurrentFrame() {
    if (config_.mode == Mpeg1Config::Streaming && !frameBuffer_.empty() && currentFrameIndex_ > 0) {
        Frame result = *frameBuffer_[lastDecodedIndex_];
        return result;
    }
    if (currentFrame_) {
        Frame result = *currentFrame_;
        return result;
    }
    // Return an invalid frame if no frame has been decoded yet
    return Frame(0);
}

bool SoftwareMpeg1Decoder::hasMoreFrames() const {
    return !endOfStream_ && ready_ && !hasError_;
}

fl::u32 SoftwareMpeg1Decoder::getFrameCount() const {
    // For streaming mode, we don't know total frames in advance
    return 0;
}

bool SoftwareMpeg1Decoder::seek(fl::u32 frameIndex) {
    (void)frameIndex; // Suppress unused parameter warning
    // Seeking not supported in this simplified implementation
    return false;
}

fl::u16 SoftwareMpeg1Decoder::getWidth() const {
    return decoderData_->width;
}

fl::u16 SoftwareMpeg1Decoder::getHeight() const {
    return decoderData_->height;
}

fl::u16 SoftwareMpeg1Decoder::getFrameRate() const {
    return decoderData_->frameRate;
}

bool SoftwareMpeg1Decoder::initializeDecoder() {
    if (!stream_) {
        setError("No input stream available");
        return false;
    }

    // Read the entire stream into memory for pl_mpeg
    // First, estimate the size by reading chunks
    const fl::size CHUNK_SIZE = 8192;
    fl::vector<fl::u8> tempBuffer;

    fl::u8 chunk[CHUNK_SIZE];
    fl::size bytesRead;
    do {
        bytesRead = stream_->read(chunk, CHUNK_SIZE);
        if (bytesRead > 0) {
            for (fl::size i = 0; i < bytesRead; ++i) {
                tempBuffer.push_back(chunk[i]);
            }
        }
    } while (bytesRead == CHUNK_SIZE);

    if (tempBuffer.empty()) {
        setError("Empty input stream - no data available");
        return false;
    }

    // Copy to our buffer
    decoderData_->totalSize = tempBuffer.size();
    decoderData_->inputBuffer.reset(new fl::u8[decoderData_->totalSize]);
    memcpy(decoderData_->inputBuffer.get(), tempBuffer.data(), decoderData_->totalSize);

    // Create pl_mpeg instance with memory buffer
    decoderData_->plmpeg = fl::third_party::plm_create_with_memory(
        decoderData_->inputBuffer.get(),
        decoderData_->totalSize,
        0  // Don't free when done - we manage the memory
    );

    if (!decoderData_->plmpeg) {
        setError("Failed to create pl_mpeg decoder instance");
        return false;
    }

    // Disable audio decoding if requested
    if (config_.skipAudio) {
        fl::third_party::plm_set_audio_enabled(decoderData_->plmpeg, 0);
    }

    // Set looping if requested
    fl::third_party::plm_set_loop(decoderData_->plmpeg, config_.looping ? 1 : 0);

    // Wait for headers to be parsed
    if (!fl::third_party::plm_has_headers(decoderData_->plmpeg)) {
        // Try to decode one frame to get headers
        fl::third_party::plm_decode(decoderData_->plmpeg, decoderData_->targetFrameDuration);
    }

    if (!fl::third_party::plm_has_headers(decoderData_->plmpeg)) {
        setError("Failed to parse MPEG1 headers");
        return false;
    }

    // Get video properties
    decoderData_->width = static_cast<fl::u16>(fl::third_party::plm_get_width(decoderData_->plmpeg));
    decoderData_->height = static_cast<fl::u16>(fl::third_party::plm_get_height(decoderData_->plmpeg));
    decoderData_->frameRate = static_cast<fl::u16>(fl::third_party::plm_get_framerate(decoderData_->plmpeg));

    if (decoderData_->width == 0 || decoderData_->height == 0) {
        setError("Invalid video dimensions from MPEG1 stream");
        return false;
    }

    // Set up video decode callback
    fl::third_party::plm_set_video_decode_callback(decoderData_->plmpeg,
        reinterpret_cast<fl::third_party::plm_video_decode_callback>(SoftwareMpeg1Decoder::videoDecodeCallback), this);

    allocateFrameBuffers();
    decoderData_->initialized = true;
    decoderData_->headerParsed = true;
    return true;
}

bool SoftwareMpeg1Decoder::parseSequenceHeader() {
    // This is now handled by pl_mpeg in initializeDecoder()
    return decoderData_->headerParsed;
}

bool SoftwareMpeg1Decoder::decodeNextFrame() {
    if (!decoderData_->headerParsed || !decoderData_->plmpeg) {
        return false;
    }

    // Reset the new frame flag
    decoderData_->hasNewFrame = false;

    // Decode using pl_mpeg
    fl::third_party::plm_decode(decoderData_->plmpeg, decoderData_->targetFrameDuration);

    // Check if we have reached the end
    if (fl::third_party::plm_has_ended(decoderData_->plmpeg)) {
        return false;
    }

    // Return true if we have a new frame
    return decoderData_->hasNewFrame;
}

bool SoftwareMpeg1Decoder::decodePictureHeader() {
    // This is now handled by pl_mpeg internally
    return true;
}

bool SoftwareMpeg1Decoder::decodeFrame() {
    // The actual frame decoding is handled by pl_mpeg via the callback
    // This method just updates our Frame objects with the decoded data

    if (!decoderData_->hasNewFrame || !decoderData_->rgbFrameBuffer.get()) {
        return false;
    }

    // Calculate timestamp in milliseconds
    fl::u32 timestampMs = static_cast<fl::u32>(decoderData_->lastFrameTime * 1000.0);

    // Update current frame
    if (config_.mode == Mpeg1Config::Streaming && !frameBuffer_.empty()) {
        fl::u8 bufferIndex = currentFrameIndex_ % config_.bufferFrames;
        // Create a new Frame using shared_ptr
        frameBuffer_[bufferIndex] = fl::make_shared<Frame>(decoderData_->rgbFrameBuffer.get(),
                                                           decoderData_->width,
                                                           decoderData_->height,
                                                           PixelFormat::RGB888,
                                                           timestampMs);
        lastDecodedIndex_ = bufferIndex;
    } else {
        // Create a new frame as shared_ptr
        currentFrame_ = fl::make_shared<Frame>(decoderData_->rgbFrameBuffer.get(),
                                              decoderData_->width,
                                              decoderData_->height,
                                              PixelFormat::RGB888,
                                              timestampMs);
    }

    currentFrameIndex_++;
    return true;
}

void SoftwareMpeg1Decoder::allocateFrameBuffers() {
    fl::size frameSize = decoderData_->width * decoderData_->height * 3; // RGB888
    decoderData_->rgbFrameSize = frameSize;

    // Allocate RGB frame buffer for converted frames
    decoderData_->rgbFrameBuffer.reset(new fl::u8[frameSize]);

    if (config_.mode == Mpeg1Config::Streaming) {
        frameBuffer_.resize(config_.bufferFrames);
        for (fl::u8 i = 0; i < config_.bufferFrames; ++i) {
            // Create Frame objects with shared_ptr (initially empty)
            frameBuffer_[i] = fl::make_shared<Frame>(0);
        }
    }
}

void SoftwareMpeg1Decoder::cleanupDecoder() {
    if (decoderData_) {
        // Destroy pl_mpeg instance
        if (decoderData_->plmpeg) {
            fl::third_party::plm_destroy(decoderData_->plmpeg);
            decoderData_->plmpeg = nullptr;
        }

        decoderData_->initialized = false;
        decoderData_->headerParsed = false;
        decoderData_->hasNewFrame = false;
        decoderData_->inputBuffer.reset();
        decoderData_->rgbFrameBuffer.reset();
    }

    // Clean up frame buffers
    frameBuffer_.clear();
}

void SoftwareMpeg1Decoder::setError(const fl::string& message) {
    hasError_ = true;
    errorMessage_ = message;
    ready_ = false;
}

} // namespace fl

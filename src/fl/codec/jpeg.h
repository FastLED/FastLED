#pragma once

#include "fl/codec/common.h"
#include "fl/bytestream.h"
#include "fl/function.h"
#include "third_party/TJpg_Decoder/src/tjpgd.h"

namespace fl {

// Forward declarations
class ProgressiveJpegDecoder;
using ProgressiveJpegDecoderPtr = fl::shared_ptr<ProgressiveJpegDecoder>;

// JPEG metadata information structure
struct JpegInfo {
    fl::u16 width = 0;          // Image width in pixels
    fl::u16 height = 0;         // Image height in pixels
    fl::u8 components = 0;      // Number of color components (1=grayscale, 3=color)
    fl::u8 bitsPerComponent = 8; // Always 8 for JPEG baseline
    bool isGrayscale = false;   // True if grayscale image
    bool isValid = false;       // True if metadata was successfully parsed

    JpegInfo() = default;

    // Constructor for easy initialization
    JpegInfo(fl::u16 w, fl::u16 h, fl::u8 comp)
        : width(w), height(h), components(comp)
        , isGrayscale(comp == 1), isValid(true) {}
};

// JPEG-specific configuration
struct JpegDecoderConfig {
    enum Quality { Low, Medium, High };

    Quality quality = High;
    PixelFormat format = PixelFormat::RGB888;

    JpegDecoderConfig() = default;
    JpegDecoderConfig(Quality q, PixelFormat fmt = PixelFormat::RGB888)
        : quality(q), format(fmt) {}
};

// Progressive JPEG processing configuration
struct ProgressiveConfig {
    fl::u16 max_mcus_per_tick = 2;          // MCU processing limit per call
    fl::u32 max_time_per_tick_ms = 4;       // 4ms time budget for 250fps
    fl::size input_buffer_size = 512;       // Input buffer size
    bool yield_on_row_complete = false;     // Don't wait for full rows at 4ms

    ProgressiveConfig() = default;
};

// Progressive JPEG decoder class
class ProgressiveJpegDecoder : public IDecoder {
public:
    enum class State {
        NotStarted,
        HeaderParsed,
        Decoding,
        Suspended,
        Complete,
        Error
    };

private:
    JpegDecoderConfig config_;
    ProgressiveConfig progressive_config_;
    State state_ = State::NotStarted;
    fl::ByteStreamPtr input_stream_;
    fl::shared_ptr<Frame> partial_frame_;
    fl::scoped_array<fl::u8> pixel_buffer_;
    float progress_percentage_ = 0.0f;
    fl::string error_message_;
    bool has_error_ = false;

    // Progressive decoder state
    fl::third_party::JDEC_Progressive progressive_decoder_;
    fl::scoped_array<fl::u8> input_buffer_;
    fl::size input_size_ = 0;

    // Helper methods
    bool readStreamData();
    bool initializeProgressiveDecoder();
    void cleanupProgressiveDecoder();
    fl::u8 getScale() const;
    void updateProgress();
    void allocateFrameBuffer(fl::u16 width, fl::u16 height);
    fl::size getBytesPerPixel() const;
    void setError(const fl::string& message);

public:
    explicit ProgressiveJpegDecoder(const JpegDecoderConfig& config);
    ~ProgressiveJpegDecoder() override = default;

    // Progressive output handling (public so static callback can access)
    bool handleProgressiveOutput(int16_t x, int16_t y, fl::u16 w, fl::u16 h, fl::u16* data);

    // IDecoder interface
    bool begin(fl::ByteStreamPtr stream) override;
    void end() override;
    bool isReady() const override;
    bool hasError(fl::string* msg = nullptr) const override;
    DecodeResult decode() override;
    Frame getCurrentFrame() override;
    bool hasMoreFrames() const override { return false; }

    // Progressive configuration
    void setProgressiveConfig(const ProgressiveConfig& config);
    ProgressiveConfig getProgressiveConfig() const { return progressive_config_; }

    // Progressive processing interface
    bool processChunk();                    // Process one chunk (returns true if more work)
    float getProgress() const;              // 0.0 to 1.0 completion
    bool canSuspend() const;                // Safe to pause processing
    void suspend();                         // Suspend processing
    bool resume();                          // Resume from suspension

    // Incremental output access
    bool hasPartialImage() const;           // Partial results available
    Frame getPartialFrame();                // Get current partial image
    fl::u16 getDecodedRows() const;         // Number of completed rows

    // Stream interface extensions
    bool feedData(fl::span<const fl::u8> data);  // Add more input data
    bool needsMoreData() const;                   // Requires additional input
    fl::size getBytesProcessed() const;           // Input bytes consumed

    // State access
    State getState() const { return state_; }
};

// JPEG decoder class
class Jpeg {
public:
    // Existing synchronous interface (unchanged)
    static bool decode(const JpegDecoderConfig& config, fl::span<const fl::u8> data, Frame* frame, fl::string* error_message = nullptr);
    static FramePtr decode(const JpegDecoderConfig& config, fl::span<const fl::u8> data, fl::string* error_message = nullptr);

    // Simplified decode with default config (High quality, RGB888)
    static FramePtr decode(fl::span<const fl::u8> data, fl::string* error_message = nullptr) {
        JpegDecoderConfig config; // Uses defaults
        return decode(config, data, error_message);
    }
    static bool isSupported();

    // New progressive interface
    static ProgressiveJpegDecoderPtr createProgressiveDecoder(const JpegDecoderConfig& config);

    // Utility for time-bounded decoding
    static bool decodeWithTimeout(
        const JpegDecoderConfig& config,
        fl::span<const fl::u8> data,
        Frame* frame,
        fl::u32 timeout_ms,                 // Maximum decode time
        float* progress_out = nullptr,       // Optional progress output
        fl::string* error_message = nullptr
    );

    // Streaming interface for large images
    static bool decodeStream(
        const JpegDecoderConfig& config,
        fl::ByteStreamPtr input_stream,
        Frame* frame,
        fl::u32 max_time_per_chunk_ms = 4,    // 4ms default
        fl::function<bool(float)> progress_callback = fl::function<bool(float)>()
    );

    // Parse JPEG metadata from byte data without full decoding
    // This is a fast, lightweight operation that only reads the JPEG header
    static JpegInfo parseJpegInfo(fl::span<const fl::u8> data, fl::string* error_message = nullptr);
};

} // namespace fl

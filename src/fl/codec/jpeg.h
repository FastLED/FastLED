#pragma once

#include "fl/codec/common.h"
#include "fl/bytestream.h"
#include "fl/stl/function.h"

namespace fl {

// Forward declarations
class JpegDecoder;
using JpegDecoderPtr = fl::shared_ptr<JpegDecoder>;

// JPEG metadata information structure
struct JpegInfo {
    fl::u16 width = 0;
    fl::u16 height = 0;
    fl::u8 components = 0;
    fl::u8 bits_per_component = 8;
    bool is_grayscale = false;
    bool is_valid = false;

    JpegInfo() = default;
    JpegInfo(fl::u16 w, fl::u16 h, fl::u8 comp);
};

// JPEG decoder configuration
struct JpegConfig {
    enum Quality { Low, Medium, High };

    Quality quality = High;
    PixelFormat format = PixelFormat::RGB888;

    JpegConfig() = default;
    JpegConfig(Quality q, PixelFormat fmt = PixelFormat::RGB888);
};

// Progressive processing configuration
struct ProgressiveConfig {
    fl::u16 max_mcus_per_tick = 2;
    fl::u32 max_time_per_tick_ms = 4;
    fl::size input_buffer_size = 512;
    bool yield_on_row_complete = false;

    ProgressiveConfig() = default;
};

// JPEG decoder with progressive processing support
class JpegDecoder : public IDecoder {
public:
    enum class State {
        NotStarted,
        HeaderParsed,
        Decoding,
        Complete,
        Error
    };

    
    using YieldFunction = fl::function<bool()>;
    static bool NeverYeildUntilDone() {
        return false;  // never signal to yield
    }
    
    explicit JpegDecoder(const JpegConfig& config);
    ~JpegDecoder() override;

    // IDecoder interface
    bool begin(fl::ByteStreamPtr stream) override;
    void end() override;
    bool isReady() const override;
    bool hasError(fl::string* msg = nullptr) const override;
    DecodeResult decode() override;
    // Partial, incremental decoding is supported. This will
    // process until the jpeg is decoded or should_yield returns true.
    DecodeResult decode(fl::optional<fl::function<bool()>> should_yield);  // Decode with optional yield callback
    Frame getCurrentFrame() override;
    bool hasMoreFrames() const override;

    // Progressive configuration
    void setProgressiveConfig(const ProgressiveConfig& config);
    ProgressiveConfig getProgressiveConfig() const;
    float getProgress() const;              // 0.0 to 1.0 completion

    // Incremental output access
    bool hasPartialImage() const;
    Frame getPartialFrame();
    fl::u16 getDecodedRows() const;

    // Stream interface extensions
    bool feedData(fl::span<const fl::u8> data);
    bool needsMoreData() const;
    fl::size getBytesProcessed() const;

    // State access
    State getState() const;

private:
    class Impl;
    fl::unique_ptr<Impl> mImpl;
};

// Main JPEG codec interface
class Jpeg {
public:
    // Synchronous decode interface
    static bool decode(const JpegConfig& config, fl::span<const fl::u8> data,
                      Frame* frame, fl::string* error_message = nullptr);
    static FramePtr decode(const JpegConfig& config, fl::span<const fl::u8> data,
                          fl::string* error_message = nullptr);

    // Simplified decode with defaults
    static FramePtr decode(fl::span<const fl::u8> data, fl::string* error_message = nullptr);

    // Progressive decoder factory
    static JpegDecoderPtr createDecoder(const JpegConfig& config);

    // Utility methods
    static bool decodeWithTimeout(const JpegConfig& config, fl::span<const fl::u8> data,
                                 Frame* frame, fl::u32 timeout_ms,
                                 float* progress_out = nullptr,
                                 fl::string* error_message = nullptr);

    static bool decodeStream(const JpegConfig& config, fl::ByteStreamPtr input_stream,
                           Frame* frame, fl::u32 max_time_per_chunk_ms = 4,
                           fl::function<bool(float)> progress_callback = {});

    // Metadata parsing
    static JpegInfo parseInfo(fl::span<const fl::u8> data, fl::string* error_message = nullptr);

    static bool isSupported();
};

} // namespace fl

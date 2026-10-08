#pragma once

#include "fl/codec/common.h"  // IWYU pragma: keep
#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"

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

    JpegInfo() FL_NO_EXCEPT = default;
    JpegInfo(fl::u16 w, fl::u16 h, fl::u8 comp) FL_NO_EXCEPT;
};

// JPEG decoder configuration
struct JpegConfig {
    enum Quality { Low, Medium, High };

    Quality quality = High;
    PixelFormat format = PixelFormat::RGB888;

    JpegConfig() FL_NO_EXCEPT = default;
    JpegConfig(Quality q, PixelFormat fmt = PixelFormat::RGB888) FL_NO_EXCEPT;
};

// Progressive processing configuration
struct ProgressiveConfig {
    fl::u16 max_mcus_per_tick = 2;
    fl::u32 max_time_per_tick_ms = 4;
    fl::size input_buffer_size = 512;
    bool yield_on_row_complete = false;

    ProgressiveConfig() FL_NO_EXCEPT = default;
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
    static bool NeverYeildUntilDone() FL_NO_EXCEPT {
        return false;  // never signal to yield
    }
    
    explicit JpegDecoder(const JpegConfig& config) FL_NO_EXCEPT;
    ~JpegDecoder() FL_NO_EXCEPT override;

    // IDecoder interface
    bool begin(fl::filebuf_ptr stream) FL_NO_EXCEPT override;
    void end() FL_NO_EXCEPT override;
    bool isReady() const FL_NO_EXCEPT override;
    bool hasError(fl::string* msg = nullptr) const FL_NO_EXCEPT override;
    DecodeResult decode() FL_NO_EXCEPT override;
    // Partial, incremental decoding is supported. This will
    // process until the jpeg is decoded or should_yield returns true.
    DecodeResult decode(fl::optional<fl::function<bool()>> should_yield) FL_NO_EXCEPT;  // Decode with optional yield callback
    Frame getCurrentFrame() FL_NO_EXCEPT override;
    bool hasMoreFrames() const FL_NO_EXCEPT override;

    // Progressive configuration
    void setProgressiveConfig(const ProgressiveConfig& config) FL_NO_EXCEPT;
    ProgressiveConfig getProgressiveConfig() const FL_NO_EXCEPT;
    float getProgress() const FL_NO_EXCEPT;              // 0.0 to 1.0 completion

    // Incremental output access
    bool hasPartialImage() const FL_NO_EXCEPT;
    Frame getPartialFrame() FL_NO_EXCEPT;
    fl::u16 getDecodedRows() const FL_NO_EXCEPT;

    // Stream interface extensions
    bool feedData(fl::span<const fl::u8> data) FL_NO_EXCEPT;
    bool needsMoreData() const FL_NO_EXCEPT;
    fl::size getBytesProcessed() const FL_NO_EXCEPT;

    // State access
    State getState() const FL_NO_EXCEPT;

private:
    class Impl;
    fl::unique_ptr<Impl> mImpl;
};

// Main JPEG codec interface
class Jpeg {
public:
    // Synchronous decode interface
    static bool decode(const JpegConfig& config, fl::span<const fl::u8> data,
                      Frame* frame, fl::string* error_message = nullptr) FL_NO_EXCEPT;
    static FramePtr decode(const JpegConfig& config, fl::span<const fl::u8> data,
                          fl::string* error_message = nullptr) FL_NO_EXCEPT;

    // Simplified decode with defaults
    static FramePtr decode(fl::span<const fl::u8> data, fl::string* error_message = nullptr) FL_NO_EXCEPT;

    // Progressive decoder factory
    static JpegDecoderPtr createDecoder(const JpegConfig& config) FL_NO_EXCEPT;

    // Utility methods
    static bool decodeWithTimeout(const JpegConfig& config, fl::span<const fl::u8> data,
                                 Frame* frame, fl::u32 timeout_ms,
                                 float* progress_out = nullptr,
                                 fl::string* error_message = nullptr) FL_NO_EXCEPT;

    static bool decodeStream(const JpegConfig& config, fl::filebuf_ptr input_stream,
                           Frame* frame, fl::u32 max_time_per_chunk_ms = 4,
                           fl::function<bool(float)> progress_callback = {}) FL_NO_EXCEPT;

    // Metadata parsing
    static JpegInfo parseInfo(fl::span<const fl::u8> data, fl::string* error_message = nullptr) FL_NO_EXCEPT;

    static bool isSupported() FL_NO_EXCEPT;
};

} // namespace fl

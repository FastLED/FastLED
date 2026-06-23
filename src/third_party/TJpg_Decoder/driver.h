// Driver layer for TJpg_Decoder to enable instance-based progressive JPEG decoding
// This file provides an instance-based wrapper around the TJpg decoder to eliminate
// global state dependencies and enable concurrent JPEG decoding

#pragma once

#include "platforms/avr/is_avr.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/unique_ptr.h"
#include "fl/codec/pixel.h"
#include "fl/fx/frame.h"
#include "fl/system/file_system.h"
#include "fl/stl/noexcept.h"
#include "src/tjpgd.h"

namespace fl {
namespace third_party {

// Forward declarations
class TJpgInstanceDecoder;
using TJpgInstanceDecoderPtr = fl::shared_ptr<TJpgInstanceDecoder>;

// Progressive configuration for time-budgeted decoding
struct TJpgProgressiveConfig {
    fl::u16 max_mcus_per_tick = 16;    // Maximum MCUs to process per tick
    fl::u32 max_time_per_tick_ms = 4;  // Maximum time per processing tick (4ms default)
};

// Instance-based TJpg decoder that contains all state internally
class TJpgInstanceDecoder {
public:
    enum class State {
        NotStarted,
        HeaderParsed,
        Decoding,
        Complete,
        Error
    };

private:
    // Embedded state structure to avoid any global dependencies
    struct EmbeddedTJpgState {
        // Workspace for TJpg decoder (must be aligned)
        // On AVR, alignment beyond 1 byte causes issues with new operator
#ifdef FL_IS_AVR
        fl::u8 workspace[4096];
#else
        fl::u8 workspace[4096] __attribute__((aligned(4)));
#endif

        // Input stream management
        const fl::u8* array_data = nullptr;
        fl::size array_index = 0;
        fl::size array_size = 0;

        // Configuration
        bool swap_bytes = false;
        fl::u8 jpg_scale = 1;

        // Back-reference to parent decoder for callbacks
        TJpgInstanceDecoder* decoder_instance = nullptr;
    } embedded_tjpg_;

    // Progressive decoder state (if using progressive mode)
    JDEC_Progressive progressive_state_;
    bool use_progressive_ = false;

    // Configuration
    TJpgProgressiveConfig progressive_config_;
    PixelFormat pixel_format_ = PixelFormat::RGB888;

    // Frame management
    fl::shared_ptr<Frame> current_frame_;
    fl::unique_ptr<fl::u8[]> frame_buffer_;
    fl::size frame_buffer_size_ = 0;

    // State tracking
    State state_ = State::NotStarted;
    fl::string error_message_;
    float progress_ = 0.0f;

    // Input data management
    fl::filebuf_ptr input_stream_;
    fl::unique_ptr<fl::u8[]> input_buffer_;
    fl::size input_size_ = 0;

    // Processing time management
    fl::u32 start_time_ms_ = 0;
    fl::u16 operations_this_tick_ = 0;

    // Internal methods
    bool readStreamData() FL_NOEXCEPT;
    bool initializeDecoder() FL_NOEXCEPT;
    void allocateFrameBuffer(fl::u16 width, fl::u16 height) FL_NOEXCEPT;
    fl::size getBytesPerPixel() const FL_NOEXCEPT;
    void setError(const fl::string& msg) FL_NOEXCEPT;
    bool shouldYield() const FL_NOEXCEPT;
    void startTick() FL_NOEXCEPT;

    // Static callbacks that use instance context
    static fl::size inputCallback(JDEC* jd, fl::u8* buff, fl::size nbyte) FL_NOEXCEPT;
    static int outputCallback(JDEC* jd, void* bitmap, JRECT* rect) FL_NOEXCEPT;

public:
    TJpgInstanceDecoder() FL_NOEXCEPT;
    ~TJpgInstanceDecoder();

    // Main decoding interface
    bool beginDecodingStream(fl::filebuf_ptr stream, PixelFormat format) FL_NOEXCEPT;
    bool processChunk() FL_NOEXCEPT;  // Process one chunk with time budget
    void endDecoding() FL_NOEXCEPT;

    // Progressive configuration
    void setProgressiveConfig(const TJpgProgressiveConfig& config) FL_NOEXCEPT {
        progressive_config_ = config;
    }

    // Scale configuration for quality settings
    void setScale(fl::u8 scale) FL_NOEXCEPT {
        embedded_tjpg_.jpg_scale = scale;
    }

    // State queries
    State getState() const FL_NOEXCEPT { return state_; }
    bool hasError(fl::string* msg = nullptr) const FL_NOEXCEPT;
    float getProgress() const FL_NOEXCEPT { return progress_; }
    Frame getCurrentFrame() const FL_NOEXCEPT;

    // Progressive features
    bool hasPartialImage() const FL_NOEXCEPT;
    Frame getPartialFrame() const FL_NOEXCEPT;
    fl::u16 getDecodedRows() const FL_NOEXCEPT;
    fl::size getBytesProcessed() const FL_NOEXCEPT;
};

// Factory function
TJpgInstanceDecoderPtr createTJpgInstanceDecoder() FL_NOEXCEPT;

} // namespace third_party
} // namespace fl
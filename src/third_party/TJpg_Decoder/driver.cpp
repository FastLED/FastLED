#include "driver.h"
#include "fl/stl/time.h"
#include "fl/warn.h"
#include "fl/stl/stdio.h"
#include "fl/stl/string.h"
#include "fl/str.h"
#include "fl/stl/cstring.h"  // for fl::memset() and fl::memcpy()

namespace fl {
namespace third_party {

TJpgInstanceDecoder::TJpgInstanceDecoder() {
    fl::memset(&embedded_tjpg_, 0, sizeof(embedded_tjpg_));
    fl::memset(&progressive_state_, 0, sizeof(progressive_state_));
    embedded_tjpg_.decoder_instance = this;
}

TJpgInstanceDecoder::~TJpgInstanceDecoder() {
    endDecoding();
}

bool TJpgInstanceDecoder::beginDecodingStream(fl::ByteStreamPtr stream, PixelFormat format) {
    if (!stream) {
        setError("Invalid stream provided");
        return false;
    }

    input_stream_ = stream;
    pixel_format_ = format;
    state_ = State::NotStarted;
    progress_ = 0.0f;
    error_message_.clear();

    // Read stream data
    if (!readStreamData()) {
        return false;
    }

    // Initialize decoder
    if (!initializeDecoder()) {
        return false;
    }

    state_ = State::HeaderParsed;
    return true;
}

bool TJpgInstanceDecoder::readStreamData() {
    if (!input_stream_) {
        setError("No input stream");
        return false;
    }

    // Read all data from stream
    fl::vector<fl::u8> temp_buffer;
    temp_buffer.reserve(4096);

    fl::u8 chunk[256];
    while (true) {
        fl::size bytes_read = input_stream_->read(chunk, sizeof(chunk));
        if (bytes_read == 0) break;

        fl::size old_size = temp_buffer.size();
        temp_buffer.resize(old_size + bytes_read);
        fl::memcpy(temp_buffer.data() + old_size, chunk, bytes_read);
    }

    input_size_ = temp_buffer.size();
    if (input_size_ == 0) {
        setError("Empty input stream");
        return false;
    }

    input_buffer_.reset(new fl::u8[input_size_]);
    fl::memcpy(input_buffer_.get(), temp_buffer.data(), input_size_);

    // Set up embedded state for input
    embedded_tjpg_.array_data = input_buffer_.get();
    embedded_tjpg_.array_index = 0;
    embedded_tjpg_.array_size = input_size_;

    return true;
}

bool TJpgInstanceDecoder::initializeDecoder() {
    // Create JDEC instance in our workspace
    JDEC* jdec = reinterpret_cast<JDEC*>(embedded_tjpg_.workspace);

    // Calculate working memory pool (after JDEC struct)
    fl::size jdec_size = sizeof(JDEC);
    fl::u8* pool = embedded_tjpg_.workspace + jdec_size;
    fl::size pool_size = sizeof(embedded_tjpg_.workspace) - jdec_size;

    // Prepare decoder with our input callback
    JRESULT res = jd_prepare(jdec, inputCallback, pool, pool_size, &embedded_tjpg_);

    if (res != JDR_OK) {
        char err_str[32];
        fl::snprintf(err_str, sizeof(err_str), "jd_prepare failed: %d", (int)res);
        setError(err_str);
        return false;
    }

    // Get image dimensions
    fl::u16 width = jdec->width;
    fl::u16 height = jdec->height;


    // Apply scaling based on jpg_scale setting
    if (embedded_tjpg_.jpg_scale > 1) {
        width /= embedded_tjpg_.jpg_scale;
        height /= embedded_tjpg_.jpg_scale;
    }


    // Allocate frame buffer
    allocateFrameBuffer(width, height);

    // Create frame object
    current_frame_ = fl::make_shared<Frame>(frame_buffer_.get(), width, height, pixel_format_);

    if (!current_frame_->isValid()) {
        setError("Failed to create frame");
        return false;
    }

    // Initialize progressive state if needed
    if (use_progressive_) {
        fl::memcpy(&progressive_state_.base, jdec, sizeof(JDEC));
        progressive_state_.current_mcu_x = 0;
        progressive_state_.current_mcu_y = 0;
        progressive_state_.mcus_processed = 0;
        progressive_state_.total_mcus = (jdec->width / jdec->msx / 8) *
                                        (jdec->height / jdec->msy / 8);
    }

    return true;
}

bool TJpgInstanceDecoder::processChunk() {
    if (state_ == State::Complete || state_ == State::Error) {
        return false;
    }

    if (state_ == State::HeaderParsed) {
        state_ = State::Decoding;
    }

    startTick();

    if (use_progressive_) {
        // Progressive decoding with time budget
        fl::u8 more_data_needed = 0;
        fl::u8 processing_complete = 0;

        JRESULT res = jd_decomp_progressive(
            &progressive_state_,
            outputCallback,
            embedded_tjpg_.jpg_scale,
            progressive_config_.max_mcus_per_tick,
            &more_data_needed,
            &processing_complete
        );

        if (processing_complete) {
            state_ = State::Complete;
            progress_ = 1.0f;
            return false;
        }

        if (res == (JRESULT)JDR_SUSPEND) {
            // Time budget exceeded, more work remains
            // Update progress
            if (progressive_state_.total_mcus > 0) {
                progress_ = (float)progressive_state_.mcus_processed /
                           (float)progressive_state_.total_mcus;
            }
            return true; // More work to do
        }

        if (res != JDR_OK) {
            char err_str[32];
            fl::snprintf(err_str, sizeof(err_str), "Progressive decode error: %d", (int)res);
            setError(err_str);
            return false;
        }

        return true; // Continue processing
    } else {
        // Non-progressive decoding (all at once)
        JDEC* jdec = reinterpret_cast<JDEC*>(embedded_tjpg_.workspace);

        JRESULT res = jd_decomp(jdec, outputCallback, embedded_tjpg_.jpg_scale);

        if (res == JDR_OK) {
            // Check if any pixels were actually set by sampling the first pixel
            CRGB* pixels = current_frame_->rgb();
            fl::u8 first_pixel_sum = 0;
            if (pixels) {
                first_pixel_sum = pixels[0].r + pixels[0].g + pixels[0].b;
            }

            char debug_str[64];
            fl::snprintf(debug_str, sizeof(debug_str), "JPEG decode OK, first_pixel_sum=%d", first_pixel_sum);

            // If no pixels were set, this indicates the output callback wasn't called
            if (first_pixel_sum == 0) {
                setError("JPEG decode succeeded but output callback was not called");
                return false;
            }

            state_ = State::Complete;
            progress_ = 1.0f;
            return false; // Complete
        } else {
            char err_str[32];
            fl::snprintf(err_str, sizeof(err_str), "Decode error: %d", (int)res);
            setError(err_str);
            return false;
        }
    }
}

void TJpgInstanceDecoder::endDecoding() {
    input_stream_.reset();
    input_buffer_.reset();
    frame_buffer_.reset();
    current_frame_.reset();
    state_ = State::NotStarted;
    progress_ = 0.0f;
}

bool TJpgInstanceDecoder::hasError(fl::string* msg) const {
    if (state_ == State::Error) {
        if (msg) {
            *msg = error_message_;
        }
        return true;
    }
    return false;
}

Frame TJpgInstanceDecoder::getCurrentFrame() const {
    if (current_frame_) {
        return *current_frame_;
    }
    return Frame(0);
}


bool TJpgInstanceDecoder::hasPartialImage() const {
    return current_frame_ && current_frame_->isValid();
}

Frame TJpgInstanceDecoder::getPartialFrame() const {
    return getCurrentFrame();
}

fl::u16 TJpgInstanceDecoder::getDecodedRows() const {
    if (use_progressive_ && progressive_state_.total_mcus > 0) {
        fl::u16 mcu_height = progressive_state_.base.msy * 8;
        return progressive_state_.current_mcu_y * mcu_height;
    }
    return 0;
}

fl::size TJpgInstanceDecoder::getBytesProcessed() const {
    return embedded_tjpg_.array_index;
}

void TJpgInstanceDecoder::allocateFrameBuffer(fl::u16 width, fl::u16 height) {
    frame_buffer_size_ = static_cast<fl::size>(width) * height * getBytesPerPixel();
    if (frame_buffer_size_ > 0) {
        frame_buffer_.reset(new fl::u8[frame_buffer_size_]);
        fl::memset(frame_buffer_.get(), 0, frame_buffer_size_);
    }
}

fl::size TJpgInstanceDecoder::getBytesPerPixel() const {
    return fl::getBytesPerPixel(pixel_format_);
}

void TJpgInstanceDecoder::setError(const fl::string& msg) {
    error_message_ = msg;
    state_ = State::Error;
}

bool TJpgInstanceDecoder::shouldYield() const {
    return (fl::millis() - start_time_ms_) >= progressive_config_.max_time_per_tick_ms;
}

void TJpgInstanceDecoder::startTick() {
    start_time_ms_ = fl::millis();
    operations_this_tick_ = 0;
}

// Static input callback - reads from embedded state
fl::size TJpgInstanceDecoder::inputCallback(JDEC* jd, fl::u8* buff, fl::size nbyte) {
    EmbeddedTJpgState* state = reinterpret_cast<EmbeddedTJpgState*>(jd->device);

    if (!state || !state->array_data) {
        return 0;
    }

    fl::size remaining = state->array_size - state->array_index;
    fl::size to_read = (nbyte < remaining) ? nbyte : remaining;

    if (buff) {
        fl::memcpy(buff, state->array_data + state->array_index, to_read);
    }

    state->array_index += to_read;
    return to_read;
}

// Static output callback - writes to frame buffer
int TJpgInstanceDecoder::outputCallback(JDEC* jd, void* bitmap, JRECT* rect) {

    EmbeddedTJpgState* state = reinterpret_cast<EmbeddedTJpgState*>(jd->device);

    if (!state || !state->decoder_instance) {
        return 0;
    }

    TJpgInstanceDecoder* decoder = state->decoder_instance;

    if (!decoder->current_frame_) {
        return 0;
    }

    fl::u16 frame_width = decoder->current_frame_->getWidth();
    fl::u16 frame_height = decoder->current_frame_->getHeight();

    // Calculate rectangle dimensions
    fl::u16 x = rect->left;
    fl::u16 y = rect->top;
    fl::u16 w = rect->right - rect->left + 1;  // JRECT is inclusive
    fl::u16 h = rect->bottom - rect->top + 1;  // JRECT is inclusive


    // Handle zero-size rectangle by treating it as full frame for small images
    if (w == 0 && h == 0 && frame_width <= 8 && frame_height <= 8) {
        x = 0;
        y = 0;
        w = frame_width;
        h = frame_height;
    }

    // Bounds check
    if (x >= frame_width || y >= frame_height ||
        x + w > frame_width || y + h > frame_height) {
        return 0;
    }

    // Get pointer to frame's RGB buffer
    CRGB* frame_pixels = decoder->current_frame_->rgb();
    if (!frame_pixels) {
        return 0;
    }

    // Copy pixels (already in RGB888 format since JD_FORMAT=0)
    fl::u8* rgb_data = reinterpret_cast<fl::u8*>(bitmap);

    for (fl::u16 row = 0; row < h; ++row) {
        for (fl::u16 col = 0; col < w; ++col) {
            fl::u16 src_idx = (row * w + col) * 3;  // RGB888 is 3 bytes per pixel

            // Calculate destination position
            int pixel_x = x + col;
            int pixel_y = y + row;
            int frame_idx = pixel_y * frame_width + pixel_x;

            // Get RGB888 values directly
            fl::u8 r = rgb_data[src_idx + 0];
            fl::u8 g = rgb_data[src_idx + 1];
            fl::u8 b = rgb_data[src_idx + 2];

            // Set pixel in frame buffer
            frame_pixels[frame_idx] = CRGB(r, g, b);
        }
    }

    return 1; // Success
}

// Factory function
TJpgInstanceDecoderPtr createTJpgInstanceDecoder() {
    return fl::make_shared<TJpgInstanceDecoder>();
}

} // namespace third_party
} // namespace fl
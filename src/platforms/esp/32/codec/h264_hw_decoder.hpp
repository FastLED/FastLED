// IWYU pragma: private
#include "platforms/esp/is_esp.h"
#include "platforms/esp/32/codec/h264_hw_decoder.h"

#if defined(FL_IS_ESP_32P4)

#include "fl/codec/h264.h"
#include "fl/stl/memory.h"

// TODO: When ESP-IDF with esp_h264 component is available, implement:
// #include "esp_h264_dec.h"
// #include "esp_h264_types.h"

namespace fl {

struct H264HwDecoder::Impl {
    fl::ByteStreamPtr stream;
    bool ready = false;
    bool error = false;
    fl::string errorMsg;
    Frame currentFrame{0};
    // esp_h264_dec_handle_t decoder = nullptr;
    // DMA-aligned buffers would go here
};

H264HwDecoder::H264HwDecoder() : mImpl(fl::make_unique<Impl>()) {}

H264HwDecoder::~H264HwDecoder() {
    end();
}

bool H264HwDecoder::begin(fl::ByteStreamPtr stream) {
    if (!stream) {
        mImpl->error = true;
        mImpl->errorMsg = "Null stream";
        return false;
    }
    mImpl->stream = stream;

    // TODO: Initialize esp_h264 decoder
    // esp_h264_dec_cfg_t cfg = { .max_width = 1920, .max_height = 1080 };
    // esp_err_t err = esp_h264_dec_open(&cfg, &mImpl->decoder);
    // if (err != ESP_OK) { ... }

    mImpl->ready = true;
    return true;
}

void H264HwDecoder::end() {
    // TODO: esp_h264_dec_close(mImpl->decoder);
    mImpl->ready = false;
    mImpl->stream = nullptr;
}

bool H264HwDecoder::isReady() const {
    return mImpl->ready;
}

bool H264HwDecoder::hasError(fl::string* msg) const {
    if (msg && mImpl->error) *msg = mImpl->errorMsg;
    return mImpl->error;
}

DecodeResult H264HwDecoder::decode() {
    if (!mImpl->ready) return DecodeResult::Error;

    // TODO: Read NAL units from stream, feed to esp_h264_dec_process()
    // Convert decoded YUV420 to RGB888 frame
    return DecodeResult::UnsupportedFormat;
}

Frame H264HwDecoder::getCurrentFrame() {
    return mImpl->currentFrame;
}

bool H264HwDecoder::hasMoreFrames() const {
    return mImpl->ready && mImpl->stream && mImpl->stream->available() > 0;
}

// Factory: on ESP32-P4 we return the hardware decoder.
IDecoderPtr H264::createDecoder(const H264Config& config,
                                fl::string* error_message) {
    FL_UNUSED(config);
    FL_UNUSED(error_message);
    return fl::make_shared<H264HwDecoder>();
}

bool H264::isSupported() { return true; }

} // namespace fl

#endif // FL_IS_ESP_32P4

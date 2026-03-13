// IWYU pragma: private
#include "platforms/esp/is_esp.h"
#include "platforms/esp/32/codec/h264_hw_decoder.h"

#if defined(FL_IS_ESP_32P4)

#include "fl/codec/h264.h"
#include "fl/codec/mp4_parser.h"
#include "fl/stl/memory.h"
#include "fl/stl/vector.h"
#include "fl/fx/frame.h"
#include "fl/stl/has_include.h"

// Check if the esp_h264 component is available.
// Install via: idf.py add-dependency "espressif/esp_h264^1.2.0"
// Or add to idf_component.yml: espressif/esp_h264: "^1.2.0"
#if FL_HAS_INCLUDE("esp_h264_dec.h")
#define FL_H264_HW_AVAILABLE 1
#include "esp_h264_dec.h"
#include "esp_h264_types.h"
#else
#define FL_H264_HW_AVAILABLE 0
#endif

namespace fl {

#if FL_H264_HW_AVAILABLE

namespace {

// Clamp integer to [0, 255] range.
inline fl::u8 clampU8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<fl::u8>(v);
}

// Split Annex B bytestream into individual NAL units.
// Finds 0x00 0x00 0x00 0x01 start codes and returns data between them.
struct NalUnit {
    const fl::u8* data;
    fl::size len;
};

fl::vector<NalUnit> splitAnnexB(const fl::vector<fl::u8>& annexB) {
    fl::vector<NalUnit> units;
    const fl::size sz = annexB.size();
    if (sz < 4) return units;

    fl::size start = 0;
    // Find first start code
    for (fl::size i = 0; i + 3 < sz; i++) {
        if (annexB[i] == 0 && annexB[i+1] == 0 &&
            annexB[i+2] == 0 && annexB[i+3] == 1) {
            start = i + 4;
            break;
        }
    }
    if (start == 0) return units;

    for (fl::size i = start; i + 3 < sz; i++) {
        if (annexB[i] == 0 && annexB[i+1] == 0 &&
            annexB[i+2] == 0 && annexB[i+3] == 1) {
            if (i > start) {
                NalUnit u;
                u.data = annexB.data() + start;
                u.len = i - start;
                units.push_back(u);
            }
            start = i + 4;
        }
    }
    // Last NAL unit
    if (start < sz) {
        NalUnit u;
        u.data = annexB.data() + start;
        u.len = sz - start;
        units.push_back(u);
    }
    return units;
}

} // namespace

struct H264HwDecoder::Impl {
    fl::filebuf_ptr stream;
    bool ready = false;
    bool error = false;
    fl::string errorMsg;
    Frame currentFrame{0};

    fl::vector<fl::u8> mp4Data;
    fl::vector<fl::u8> annexB;
    Mp4TrackInfo track;
    H264Info info;
    bool frameDecoded = false;

    esp_h264_dec_handle_t decoder = nullptr;
};

H264HwDecoder::H264HwDecoder() : mImpl(fl::make_unique<Impl>()) {}

H264HwDecoder::~H264HwDecoder() {
    end();
}

bool H264HwDecoder::begin(fl::filebuf_ptr stream) {
    if (!stream) {
        mImpl->error = true;
        mImpl->errorMsg = "Null stream";
        return false;
    }
    mImpl->stream = stream;

    // Read all data from stream
    fl::size total = stream->size();
    if (total == 0) {
        fl::u8 buf[1024];
        while (true) {
            fl::size n = stream->read(buf, sizeof(buf));
            if (n == 0) break;
            for (fl::size i = 0; i < n; i++) {
                mImpl->mp4Data.push_back(buf[i]);
            }
        }
    } else {
        mImpl->mp4Data.resize(total);
        stream->read(mImpl->mp4Data.data(), total);
    }

    if (mImpl->mp4Data.empty()) {
        mImpl->error = true;
        mImpl->errorMsg = "No data read from stream";
        return false;
    }

    // Parse MP4 container
    fl::string parseError;
    mImpl->track = parseMp4(mImpl->mp4Data, &parseError);
    if (!mImpl->track.isValid) {
        mImpl->error = true;
        mImpl->errorMsg = "MP4 parse failed: ";
        mImpl->errorMsg += parseError;
        return false;
    }

    // Parse H264 info for dimensions
    mImpl->info = H264::parseH264Info(mImpl->mp4Data, &parseError);
    if (!mImpl->info.isValid) {
        mImpl->error = true;
        mImpl->errorMsg = "H264 info parse failed: ";
        mImpl->errorMsg += parseError;
        return false;
    }

    // Extract Annex B NAL units
    mImpl->annexB = extractH264NalUnits(mImpl->mp4Data, mImpl->track, &parseError);
    if (mImpl->annexB.empty()) {
        mImpl->error = true;
        mImpl->errorMsg = "No NAL units extracted: ";
        mImpl->errorMsg += parseError;
        return false;
    }

    // Initialize esp_h264 software decoder
    esp_h264_dec_cfg_sw_t cfg = {};
    cfg.pic_type = ESP_H264_RAW_FMT_I420;

    esp_h264_err_t err = esp_h264_dec_sw_new(&cfg, &mImpl->decoder);
    if (err != ESP_H264_ERR_OK || !mImpl->decoder) {
        mImpl->error = true;
        mImpl->errorMsg = "esp_h264_dec_sw_new failed";
        return false;
    }

    err = esp_h264_dec_open(mImpl->decoder);
    if (err != ESP_H264_ERR_OK) {
        esp_h264_dec_del(mImpl->decoder);
        mImpl->decoder = nullptr;
        mImpl->error = true;
        mImpl->errorMsg = "esp_h264_dec_open failed";
        return false;
    }

    mImpl->ready = true;
    return true;
}

void H264HwDecoder::end() {
    if (mImpl->decoder) {
        esp_h264_dec_close(mImpl->decoder);
        esp_h264_dec_del(mImpl->decoder);
        mImpl->decoder = nullptr;
    }
    mImpl->ready = false;
    mImpl->stream = nullptr;
    mImpl->mp4Data.clear();
    mImpl->annexB.clear();
    mImpl->frameDecoded = false;
}

bool H264HwDecoder::isReady() const {
    return mImpl->ready;
}

bool H264HwDecoder::hasError(fl::string* msg) const {
    if (msg && mImpl->error) *msg = mImpl->errorMsg;
    return mImpl->error;
}

DecodeResult H264HwDecoder::decode() {
    if (!mImpl->ready || !mImpl->decoder) return DecodeResult::Error;
    if (mImpl->frameDecoded) return DecodeResult::EndOfStream;

    const fl::u16 w = mImpl->info.width;
    const fl::u16 h = mImpl->info.height;

    // Split Annex B stream into individual NAL units
    fl::vector<NalUnit> nalUnits = splitAnnexB(mImpl->annexB);
    if (nalUnits.empty()) {
        mImpl->error = true;
        mImpl->errorMsg = "No NAL units found in Annex B stream";
        return DecodeResult::Error;
    }

    // Feed each NAL unit to decoder until we get a frame
    for (fl::size i = 0; i < nalUnits.size(); i++) {
        esp_h264_dec_in_frame_t in_frame = {};
        in_frame.raw_data.buffer = const_cast<fl::u8*>(nalUnits[i].data);
        in_frame.raw_data.len = static_cast<fl::u32>(nalUnits[i].len);

        esp_h264_dec_out_frame_t out_frame = {};

        esp_h264_err_t err = esp_h264_dec_process(mImpl->decoder, &in_frame, &out_frame);
        if (err != ESP_H264_ERR_OK) {
            continue; // SPS/PPS NALUs may not produce output
        }

        if (out_frame.out_size > 0 && out_frame.outbuf != nullptr) {
            // YUV420P (I420) → RGB888 conversion
            const fl::u32 pixelCount = static_cast<fl::u32>(w) * static_cast<fl::u32>(h);
            const fl::u8* yPlane = out_frame.outbuf;
            const fl::u8* uPlane = out_frame.outbuf + pixelCount;
            const fl::u8* vPlane = uPlane + pixelCount / 4;

            fl::vector<fl::u8> rgbBuf(pixelCount * 3);
            fl::u8* dst = rgbBuf.data();

            for (fl::u16 row = 0; row < h; row++) {
                for (fl::u16 col = 0; col < w; col++) {
                    int y = yPlane[row * w + col];
                    int u = uPlane[(row / 2) * (w / 2) + (col / 2)];
                    int v = vPlane[(row / 2) * (w / 2) + (col / 2)];

                    // BT.601 YUV→RGB fixed-point
                    int d = u - 128;
                    int e = v - 128;
                    *dst++ = clampU8(y + ((359 * e + 128) >> 8));           // R
                    *dst++ = clampU8(y - ((88 * d + 183 * e + 128) >> 8)); // G
                    *dst++ = clampU8(y + ((454 * d + 128) >> 8));           // B
                }
            }

            mImpl->currentFrame = Frame(rgbBuf.data(), w, h, PixelFormat::RGB888, 0);
            mImpl->frameDecoded = true;
            return DecodeResult::Success;
        }
    }

    mImpl->error = true;
    mImpl->errorMsg = "No frame decoded from NAL units";
    return DecodeResult::Error;
}

Frame H264HwDecoder::getCurrentFrame() {
    return mImpl->currentFrame;
}

bool H264HwDecoder::hasMoreFrames() const {
    return mImpl->ready && !mImpl->frameDecoded;
}

#else // !FL_H264_HW_AVAILABLE

// Stub implementation when esp_h264 component is not installed.
// Returns a clear error message.

struct H264HwDecoder::Impl {
    fl::filebuf_ptr stream;
    bool ready = false;
    Frame currentFrame{0};
};

H264HwDecoder::H264HwDecoder() : mImpl(fl::make_unique<Impl>()) {}
H264HwDecoder::~H264HwDecoder() { end(); }

bool H264HwDecoder::begin(fl::filebuf_ptr) { return false; }
void H264HwDecoder::end() { mImpl->ready = false; mImpl->stream = nullptr; }
bool H264HwDecoder::isReady() const { return false; }
bool H264HwDecoder::hasError(fl::string* msg) const {
    if (msg) *msg = "esp_h264 component not installed. "
                     "Add espressif/esp_h264 to idf_component.yml";
    return true;
}
DecodeResult H264HwDecoder::decode() { return DecodeResult::UnsupportedFormat; }
Frame H264HwDecoder::getCurrentFrame() { return mImpl->currentFrame; }
bool H264HwDecoder::hasMoreFrames() const { return false; }

#endif // FL_H264_HW_AVAILABLE

// Factory: on ESP32-P4 we return the hardware decoder.
IDecoderPtr H264::createDecoder(const H264Config& config,
                                fl::string* error_message) {
    FL_UNUSED(config);
#if !FL_H264_HW_AVAILABLE
    if (error_message) {
        *error_message = "esp_h264 component not installed";
    }
#else
    FL_UNUSED(error_message);
#endif
    return fl::make_shared<H264HwDecoder>();
}

bool H264::isSupported() {
#if FL_H264_HW_AVAILABLE
    return true;
#else
    return false;
#endif
}

} // namespace fl

#endif // FL_IS_ESP_32P4

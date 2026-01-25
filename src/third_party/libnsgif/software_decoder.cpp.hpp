#include "software_decoder.h"
#include "fl/scoped_array.h"
#include "fl/stl/string.h"
#include "fl/str.h"
#include "fl/stl/cstring.h"  // for fl::memset() and fl::memcpy()

namespace fl {
namespace third_party {

// Initialize static bitmap callbacks
nsgif_bitmap_cb_vt SoftwareGifDecoder::bitmapCallbacks_ = {
    SoftwareGifDecoder::bitmapCreate,
    SoftwareGifDecoder::bitmapDestroy,
    SoftwareGifDecoder::bitmapGetBuffer,
    nullptr, // set_opaque (optional)
    nullptr, // test_opaque (optional)
    nullptr, // modified (optional)
    nullptr  // get_rowspan (optional)
};

SoftwareGifDecoder::SoftwareGifDecoder(fl::PixelFormat format)
    : gif_(nullptr)
    , stream_(nullptr)
    , currentFrame_(nullptr)
    , ready_(false)
    , hasError_(false)
    , dataComplete_(false)
    , currentFrameIndex_(0)
    , endOfStream_(false) {
    (void)format; // Unused - libnsgif always outputs RGBA8888
}

SoftwareGifDecoder::~SoftwareGifDecoder() {
    end();
}

bool SoftwareGifDecoder::begin(fl::ByteStreamPtr stream) {
    if (!stream) {
        setError("Invalid stream provided");
        return false;
    }

    stream_ = stream;

    if (!initializeDecoder()) {
        return false;
    }

    return loadMoreData();
}

void SoftwareGifDecoder::end() {
    cleanupDecoder();
    stream_ = nullptr;
    currentFrame_ = nullptr;
    ready_ = false;
    hasError_ = false;
    dataComplete_ = false;
    currentFrameIndex_ = 0;
    endOfStream_ = false;
    errorMessage_.clear();
    dataBuffer_.clear();
}

bool SoftwareGifDecoder::hasError(fl::string* msg) const {
    if (msg && hasError_) {
        *msg = errorMessage_;
    }
    return hasError_;
}

fl::DecodeResult SoftwareGifDecoder::decode() {
    if (hasError_) {
        return fl::DecodeResult::Error;
    }

    if (!ready_) {
        setError("Decoder not ready");
        return fl::DecodeResult::Error;
    }

    if (endOfStream_) {
        return fl::DecodeResult::EndOfStream;
    }

    // Try to decode current frame
    nsgif_bitmap_t* bitmap = nullptr;
    nsgif_error result = nsgif_frame_decode(gif_, currentFrameIndex_, &bitmap);

    switch (result) {
        case NSGIF_OK:
            currentFrame_ = convertBitmapToFrame(bitmap);
            if (!currentFrame_) {
                setError("Failed to convert bitmap to frame");
                return fl::DecodeResult::Error;
            }
            currentFrameIndex_++;
            return fl::DecodeResult::Success;

        case NSGIF_ERR_END_OF_DATA:
            if (!dataComplete_) {
                // Try to load more data
                if (loadMoreData()) {
                    return decode(); // Retry
                }
            }
            endOfStream_ = true;
            return fl::DecodeResult::EndOfStream;

        case NSGIF_ERR_ANIMATION_END:
            endOfStream_ = true;
            return fl::DecodeResult::EndOfStream;

        case NSGIF_ERR_OOM:
        case NSGIF_ERR_DATA:
        case NSGIF_ERR_BAD_FRAME:
        case NSGIF_ERR_DATA_FRAME:
        case NSGIF_ERR_DATA_COMPLETE:
        case NSGIF_ERR_FRAME_DISPLAY:
        default:
            setError(fl::string("GIF decode error: ") + nsgif_strerror(result));
            return fl::DecodeResult::Error;
    }
}

fl::Frame SoftwareGifDecoder::getCurrentFrame() {
    if (currentFrame_) {
        return *currentFrame_;
    }
    return fl::Frame(0); // Empty frame
}

bool SoftwareGifDecoder::hasMoreFrames() const {
    if (hasError_ || !ready_) {
        return false;
    }

    if (endOfStream_) {
        return false;
    }

    // Check if we have more frames available
    const nsgif_info_t* info = nsgif_get_info(gif_);
    if (!info) {
        return false;
    }

    return currentFrameIndex_ < info->frame_count || !dataComplete_;
}

fl::u32 SoftwareGifDecoder::getFrameCount() const {
    if (!ready_ || hasError_) {
        return 0;
    }

    const nsgif_info_t* info = nsgif_get_info(gif_);
    return info ? info->frame_count : 0;
}

bool SoftwareGifDecoder::seek(fl::u32 frameIndex) {
    if (!ready_ || hasError_) {
        return false;
    }

    const nsgif_info_t* info = nsgif_get_info(gif_);
    if (!info || frameIndex >= info->frame_count) {
        return false;
    }

    currentFrameIndex_ = frameIndex;
    endOfStream_ = false;
    return true;
}

fl::u16 SoftwareGifDecoder::getWidth() const {
    if (!ready_ || hasError_) {
        return 0;
    }

    const nsgif_info_t* info = nsgif_get_info(gif_);
    return info ? static_cast<fl::u16>(info->width) : 0;
}

fl::u16 SoftwareGifDecoder::getHeight() const {
    if (!ready_ || hasError_) {
        return 0;
    }

    const nsgif_info_t* info = nsgif_get_info(gif_);
    return info ? static_cast<fl::u16>(info->height) : 0;
}

bool SoftwareGifDecoder::isAnimated() const {
    return getFrameCount() > 1;
}

fl::u32 SoftwareGifDecoder::getLoopCount() const {
    if (!ready_ || hasError_) {
        return 0;
    }

    const nsgif_info_t* info = nsgif_get_info(gif_);
    return info ? static_cast<fl::u32>(info->loop_max) : 0;
}

bool SoftwareGifDecoder::initializeDecoder() {
    // Determine bitmap format based on output format
    // Use R8G8B8A8 explicitly to ensure byte order: 0xRR, 0xGG, 0xBB, 0xAA
    nsgif_bitmap_fmt_t bitmapFormat = NSGIF_BITMAP_FMT_R8G8B8A8;

    nsgif_error result = nsgif_create(&bitmapCallbacks_, bitmapFormat, &gif_);
    if (result != NSGIF_OK) {
        setError(fl::string("Failed to create GIF decoder: ") + nsgif_strerror(result));
        return false;
    }

    ready_ = true;
    return true;
}

void SoftwareGifDecoder::cleanupDecoder() {
    if (gif_) {
        nsgif_destroy(gif_);
        gif_ = nullptr;
    }
    ready_ = false;
}

void SoftwareGifDecoder::setError(const fl::string& message) {
    hasError_ = true;
    errorMessage_ = message;
    ready_ = false;
}

bool SoftwareGifDecoder::loadMoreData() {
    if (!stream_ || !gif_) {
        return false;
    }

    // Read available data from stream in chunks and accumulate
    const fl::size bufferSize = 4096; // Read in chunks
    fl::u8 buffer[bufferSize];
    fl::size bytesRead = stream_->read(buffer, bufferSize);

    if (bytesRead == 0) {
        // No more data available, mark as complete
        nsgif_data_complete(gif_);
        dataComplete_ = true;
        return false;
    }

    // Append new data to accumulated buffer
    // libnsgif requires ALL data to be provided in each call to nsgif_data_scan
    fl::size oldSize = dataBuffer_.size();
    dataBuffer_.resize(oldSize + bytesRead);
    fl::memcpy(dataBuffer_.data() + oldSize, buffer, bytesRead);

    // Feed ALL accumulated data to libnsgif
    nsgif_error result = nsgif_data_scan(gif_, dataBuffer_.size(), dataBuffer_.data());

    // Check if we've read less than requested (likely end of stream)
    if (bytesRead < bufferSize) {
        nsgif_data_complete(gif_);
        dataComplete_ = true;
    }

    if (result != NSGIF_OK && result != NSGIF_ERR_END_OF_DATA) {
        setError(fl::string("GIF data scan error: ") + nsgif_strerror(result));
        return false;
    }

    // Frame count is available via nsgif_get_info() if needed in the future

    return true;
}

fl::shared_ptr<fl::Frame> SoftwareGifDecoder::convertBitmapToFrame(nsgif_bitmap_t* bitmap) {
    if (!bitmap) {
        setError("convertBitmapToFrame called with null bitmap");
        return nullptr;
    }

    GifBitmap* gifBitmap = static_cast<GifBitmap*>(bitmap);

    // Validate bitmap data
    if (!gifBitmap->pixels || gifBitmap->width == 0 || gifBitmap->height == 0) {
        setError("GIF bitmap has invalid data or dimensions");
        return nullptr;
    }

    fl::u8* rawData = gifBitmap->pixels.get();

    // Since libnsgif outputs RGBA8888, we need to handle the conversion properly
    // The Frame constructor expects the format we specify (outputFormat_)
    // but libnsgif always gives us RGBA8888 data

    // Create Frame with RGBA8888 format since that's what libnsgif provides
    // The Frame constructor will handle conversion to outputFormat_ via convertPixelsToRgb
    auto frame = fl::make_shared<fl::Frame>(
        rawData,
        gifBitmap->width,
        gifBitmap->height,
        fl::PixelFormat::RGBA8888,  // What libnsgif actually provides
        currentFrameIndex_ // Use frame index as timestamp
    );

    if (!frame || !frame->isValid()) {
        setError("Failed to create valid Frame from GIF bitmap");
        return nullptr;
    }

    return frame;
}

// Static callback implementations
nsgif_bitmap_t* SoftwareGifDecoder::bitmapCreate(int width, int height) {
    // Create a GifBitmap wrapper
    // Note: In environments without exceptions, new will return nullptr on failure
    GifBitmap* bitmap = new GifBitmap(static_cast<fl::u16>(width), static_cast<fl::u16>(height), 4); // 4 bytes per pixel for RGBA
    return static_cast<nsgif_bitmap_t*>(bitmap);
}

void SoftwareGifDecoder::bitmapDestroy(nsgif_bitmap_t* bitmap) {
    if (bitmap) {
        delete static_cast<GifBitmap*>(bitmap);
    }
}

fl::u8* SoftwareGifDecoder::bitmapGetBuffer(nsgif_bitmap_t* bitmap) {
    if (!bitmap) {
        return nullptr;
    }

    GifBitmap* gifBitmap = static_cast<GifBitmap*>(bitmap);
    return gifBitmap->pixels.get();
}

} // namespace third_party
} // namespace fl
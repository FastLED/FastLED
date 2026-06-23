#pragma once

#include "fl/codec/idecoder.h"
#include "fl/stl/noexcept.h"
#include "fl/codec/common.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/stdint.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/vector.h"
#include "fl/fx/frame.h"

// Include the actual nsgif header

#include "third_party/libnsgif/include/nsgif.hpp"

namespace fl {
namespace third_party {

    // Simple bitmap wrapper for libnsgif integration with FastLED Frame
    struct GifBitmap {
        fl::unique_ptr<fl::u8[]> pixels;
        fl::u16 width;
        fl::u16 height;
        fl::u8 bytesPerPixel;

        GifBitmap(fl::u16 w, fl::u16 h, fl::u8 bpp) FL_NOEXCEPT
            : pixels(fl::make_unique<fl::u8[]>(w * h * bpp))
            , width(w)
            , height(h)
            , bytesPerPixel(bpp) {
        }
    };

    /**
     * Software GIF decoder implementation using libnsgif.
     *
     * This decoder provides full animated GIF support through the FastLED IDecoder
     * interface, enabling streaming decode of GIF animations with proper frame timing
     * and disposal method handling.
     */
    class SoftwareGifDecoder : public fl::IDecoder {
    private:
        nsgif_t* gif_;
        fl::filebuf_ptr stream_;
        fl::shared_ptr<fl::Frame> currentFrame_;
        fl::string errorMessage_;
        bool ready_;
        bool hasError_;
        bool dataComplete_;

        // Data buffer - libnsgif requires all data to be contiguous
        fl::vector<fl::u8> dataBuffer_;

        // Animation state
        fl::u32 currentFrameIndex_;
        bool endOfStream_;

        // Bitmap callbacks for libnsgif
        static nsgif_bitmap_cb_vt bitmapCallbacks_;

        // Internal helper methods
        bool initializeDecoder() FL_NOEXCEPT;
        void cleanupDecoder() FL_NOEXCEPT;
        void setError(const fl::string& message) FL_NOEXCEPT;
        bool loadMoreData() FL_NOEXCEPT;
        fl::shared_ptr<fl::Frame> convertBitmapToFrame(nsgif_bitmap_t* bitmap) FL_NOEXCEPT;

        // Static callbacks for libnsgif
        static nsgif_bitmap_t* bitmapCreate(int width, int height) FL_NOEXCEPT;
        static void bitmapDestroy(nsgif_bitmap_t* bitmap) FL_NOEXCEPT;
        static fl::u8* bitmapGetBuffer(nsgif_bitmap_t* bitmap) FL_NOEXCEPT;

    public:
        explicit SoftwareGifDecoder(fl::PixelFormat format = fl::PixelFormat::RGB888) FL_NOEXCEPT;
        ~SoftwareGifDecoder();

        // IDecoder interface implementation
        bool begin(fl::filebuf_ptr stream) FL_NOEXCEPT override;
        void end() FL_NOEXCEPT override;
        bool isReady() const FL_NOEXCEPT override { return ready_; }
        bool hasError(fl::string* msg = nullptr) const FL_NOEXCEPT override;

        fl::DecodeResult decode() FL_NOEXCEPT override;
        fl::Frame getCurrentFrame() FL_NOEXCEPT override;
        bool hasMoreFrames() const FL_NOEXCEPT override;

        // Animation support methods
        fl::u32 getFrameCount() const FL_NOEXCEPT override;
        fl::u32 getCurrentFrameIndex() const FL_NOEXCEPT override { return currentFrameIndex_; }
        bool seek(fl::u32 frameIndex) FL_NOEXCEPT override;

        // Get GIF properties
        fl::u16 getWidth() const FL_NOEXCEPT;
        fl::u16 getHeight() const FL_NOEXCEPT;
        bool isAnimated() const FL_NOEXCEPT;
        fl::u32 getLoopCount() const FL_NOEXCEPT;
    };

} // namespace third_party
} // namespace fl

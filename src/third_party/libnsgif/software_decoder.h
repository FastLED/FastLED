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

        GifBitmap(fl::u16 w, fl::u16 h, fl::u8 bpp) FL_NO_EXCEPT
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
        bool initializeDecoder() FL_NO_EXCEPT;
        void cleanupDecoder() FL_NO_EXCEPT;
        void setError(const fl::string& message) FL_NO_EXCEPT;
        bool loadMoreData() FL_NO_EXCEPT;
        fl::shared_ptr<fl::Frame> convertBitmapToFrame(nsgif_bitmap_t* bitmap) FL_NO_EXCEPT;

        // Static callbacks for libnsgif
        static nsgif_bitmap_t* bitmapCreate(int width, int height) FL_NO_EXCEPT;
        static void bitmapDestroy(nsgif_bitmap_t* bitmap) FL_NO_EXCEPT;
        static fl::u8* bitmapGetBuffer(nsgif_bitmap_t* bitmap) FL_NO_EXCEPT;

    public:
        explicit SoftwareGifDecoder(fl::PixelFormat format = fl::PixelFormat::RGB888) FL_NO_EXCEPT;
        ~SoftwareGifDecoder();

        // IDecoder interface implementation
        bool begin(fl::filebuf_ptr stream) FL_NO_EXCEPT override;
        void end() FL_NO_EXCEPT override;
        bool isReady() const FL_NO_EXCEPT override { return ready_; }
        bool hasError(fl::string* msg = nullptr) const FL_NO_EXCEPT override;

        fl::DecodeResult decode() FL_NO_EXCEPT override;
        fl::Frame getCurrentFrame() FL_NO_EXCEPT override;
        bool hasMoreFrames() const FL_NO_EXCEPT override;

        // Animation support methods
        fl::u32 getFrameCount() const FL_NO_EXCEPT override;
        fl::u32 getCurrentFrameIndex() const FL_NO_EXCEPT override { return currentFrameIndex_; }
        bool seek(fl::u32 frameIndex) FL_NO_EXCEPT override;

        // Get GIF properties
        fl::u16 getWidth() const FL_NO_EXCEPT;
        fl::u16 getHeight() const FL_NO_EXCEPT;
        bool isAnimated() const FL_NO_EXCEPT;
        fl::u32 getLoopCount() const FL_NO_EXCEPT;
    };

} // namespace third_party
} // namespace fl

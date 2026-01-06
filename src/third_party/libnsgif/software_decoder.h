#pragma once

#include "fl/codec/idecoder.h"
#include "fl/codec/common.h"
#include "fl/stl/shared_ptr.h"
#include "fl/str.h"
#include "fl/stl/stdint.h"
#include "fl/scoped_array.h"
#include "fl/stl/vector.h"
#include "fl/fx/frame.h"

// Include the actual nsgif header

#include "third_party/libnsgif/include/nsgif.hpp"

namespace fl {
namespace third_party {

    // Simple bitmap wrapper for libnsgif integration with FastLED Frame
    struct GifBitmap {
        fl::scoped_array<fl::u8> pixels;
        fl::u16 width;
        fl::u16 height;
        fl::u8 bytesPerPixel;

        GifBitmap(fl::u16 w, fl::u16 h, fl::u8 bpp)
            : pixels(fl::make_scoped_array<fl::u8>(w * h * bpp))
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
        fl::ByteStreamPtr stream_;
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
        bool initializeDecoder();
        void cleanupDecoder();
        void setError(const fl::string& message);
        bool loadMoreData();
        fl::shared_ptr<fl::Frame> convertBitmapToFrame(nsgif_bitmap_t* bitmap);

        // Static callbacks for libnsgif
        static nsgif_bitmap_t* bitmapCreate(int width, int height);
        static void bitmapDestroy(nsgif_bitmap_t* bitmap);
        static fl::u8* bitmapGetBuffer(nsgif_bitmap_t* bitmap);

    public:
        explicit SoftwareGifDecoder(fl::PixelFormat format = fl::PixelFormat::RGB888);
        ~SoftwareGifDecoder();

        // IDecoder interface implementation
        bool begin(fl::ByteStreamPtr stream) override;
        void end() override;
        bool isReady() const override { return ready_; }
        bool hasError(fl::string* msg = nullptr) const override;

        fl::DecodeResult decode() override;
        fl::Frame getCurrentFrame() override;
        bool hasMoreFrames() const override;

        // Animation support methods
        fl::u32 getFrameCount() const override;
        fl::u32 getCurrentFrameIndex() const override { return currentFrameIndex_; }
        bool seek(fl::u32 frameIndex) override;

        // Get GIF properties
        fl::u16 getWidth() const;
        fl::u16 getHeight() const;
        bool isAnimated() const;
        fl::u32 getLoopCount() const;
    };

} // namespace third_party
} // namespace fl

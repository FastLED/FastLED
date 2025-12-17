#pragma once

#include "fl/stl/cstring.h"
#include "crgb.h"
#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/xymap.h"
#include "fl/stl/vector.h"
#include "fl/stl/stdint.h"

#include "fl/stl/allocator.h"
#include "fl/draw_mode.h"
#include "fl/codec/pixel.h"

namespace fl {

FASTLED_SHARED_PTR(Frame);

// Frames are used to hold led data. This includes an optional alpha channel.
// This object is used by the fx and video engines. Most of the memory used for
// Fx and Video will be located in instances of this class. See
// Frame::SetAllocator() for custom memory allocation.
class Frame {
  public:
    // Frames take up a lot of memory. On some devices like ESP32 there is
    // PSRAM available. You should see allocator.h ->
    // SetPSRamAllocator(...) on setting a custom allocator for these large
    // blocks.
    explicit Frame(int pixels_per_frame);

    // Constructor for codec-sourced frames
    Frame(fl::u8* pixels, fl::u16 width, fl::u16 height, PixelFormat format, fl::u32 timestamp = 0);

    // Copy constructor
    Frame(const Frame& other);

    // Delete copy assignment operator (can't assign due to const member)
    Frame& operator=(const Frame& other) = delete;

    ~Frame();
    CRGB *rgb() { return mRgb.data(); }
    const CRGB *rgb() const { return mRgb.data(); }
    size_t size() const { return mPixelsCount; }
    void copy(const Frame &other);
    void interpolate(const Frame &frame1, const Frame &frame2,
                     uint8_t amountOfFrame2);
    static void interpolate(const Frame &frame1, const Frame &frame2,
                            uint8_t amountofFrame2, CRGB *pixels);
    void draw(CRGB *leds, DrawMode draw_mode = DRAW_MODE_OVERWRITE) const;
    void drawXY(CRGB *leds, const XYMap &xyMap,
                DrawMode draw_mode = DRAW_MODE_OVERWRITE) const;
    void clear();

    // Codec functionality methods
    bool isValid() const;
    fl::u32 getTimestamp() const { return mTimestamp; }
    PixelFormat getFormat() const { return mFormat; }
    fl::u16 getWidth() const { return mWidth; }
    fl::u16 getHeight() const { return mHeight; }
    bool isFromCodec() const { return mIsFromCodec; }

  private:
    const size_t mPixelsCount;
    fl::vector<CRGB, fl::allocator_psram<CRGB>> mRgb;

    // Codec-specific members
    fl::u16 mWidth = 0;
    fl::u16 mHeight = 0;
    PixelFormat mFormat = PixelFormat::RGB888;
    fl::u32 mTimestamp = 0;
    bool mIsFromCodec = false;

    // Helper method for pixel format conversion
    void convertPixelsToRgb(fl::u8* pixels, PixelFormat format);
};

inline void Frame::copy(const Frame &other) {
    fl::memcpy(mRgb.data(), other.mRgb.data(), other.mPixelsCount * sizeof(CRGB));
}

} // namespace fl

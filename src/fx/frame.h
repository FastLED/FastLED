#pragma once

#include <string.h>

#include "crgb.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/xymap.h"
#include "fl/vector.h"
#include "fl/stdint.h"

#include "fl/allocator.h"
#include "fl/draw_mode.h"

namespace fl {

// Color formats for decoded output
enum class PixelFormat {
    RGB565,     // 16-bit RGB: RRRRR GGGGGG BBBBB
    RGB888,     // 24-bit RGB: RRRRRRRR GGGGGGGG BBBBBBBB
    RGBA8888,   // 32-bit RGBA: RRRRRRRR GGGGGGGG BBBBBBBB AAAAAAAA
    YUV420      // YUV 4:2:0 format (mainly for internal use)
};

// Calculate bytes per pixel for given format
inline fl::u8 getBytesPerPixel(PixelFormat format) {
    switch (format) {
        case PixelFormat::RGB565: return 2;
        case PixelFormat::RGB888: return 3;
        case PixelFormat::RGBA8888: return 4;
        case PixelFormat::YUV420: return 1; // Simplified for luminance component
        default: return 3;
    }
}

// Convert RGB888 to RGB565
inline fl::u16 rgb888ToRgb565(fl::u8 r, fl::u8 g, fl::u8 b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Convert RGB565 to RGB888
inline void rgb565ToRgb888(fl::u16 rgb565, fl::u8& r, fl::u8& g, fl::u8& b) {
    r = (rgb565 >> 8) & 0xF8;
    g = (rgb565 >> 3) & 0xFC;
    b = (rgb565 << 3) & 0xF8;
}

FASTLED_SMART_PTR(Frame);

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
    memcpy(mRgb.data(), other.mRgb.data(), other.mPixelsCount * sizeof(CRGB));
}

} // namespace fl

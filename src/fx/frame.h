#pragma once

#include <string.h>

#include "fl/namespace.h"
#include "crgb.h"
#include "fl/ptr.h"

#include "fl/allocator.h"

namespace fl {

FASTLED_SMART_PTR(Frame);

// Frames are used to hold led data. This includes an optional alpha channel. This object
// is used by the fx and video engines. Most of the memory used for Fx and Video will be located
// in instances of this class. See Frame::SetAllocator() for custom memory allocation.
class Frame : public fl::Referent {
public:
    // Frames take up a lot of memory. On some devices like ESP32 there is
    // PSRAM available. You should see allocator.h -> SetLargeBlockAllocator(...)
    // on setting a custom allocator for these large blocks.
    explicit Frame(int pixels_per_frame);
    ~Frame() override;
    CRGB* rgb() { return mRgb.get(); }
    const CRGB* rgb() const { return mRgb.get(); }
    size_t size() const { return mPixelsCount; }
    void copy(const Frame& other);
    void interpolate(const Frame& frame1, const Frame& frame2, uint8_t amountOfFrame2);
    static void interpolate(const Frame& frame1, const Frame& frame2, uint8_t amountofFrame2, CRGB* pixels);
    void draw(CRGB* leds) const;
private:
    const size_t mPixelsCount;
    fl::scoped_array<CRGB> mRgb;
};


inline void Frame::copy(const Frame& other) {
    memcpy(mRgb.get(), other.mRgb.get(), other.mPixelsCount * sizeof(CRGB));
}

}  // namespace fl

#pragma once

#include <string.h>

#include "namespace.h"
#include "crgb.h"
#include "ref.h"

#include "allocator.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(Frame);

// Frames are used to hold led data. This includes an optional alpha channel. This object
// is used by the fx and video engines. Most of the memory used for Fx and Video will be located
// in instances of this class. See Frame::SetAllocator() for custom memory allocation.
class Frame : public Referent {
public:
    // Frames take up a lot of memory. On some devices like ESP32 there is
    // PSRAM available. You should see allocator.h -> SetLargeBlockAllocator(...)
    // on setting a custom allocator for these large blocks.
    explicit Frame(int pixels_per_frame, bool has_alpha = false);
    ~Frame() override;
    CRGB* rgb() { return mRgb.get(); }
    const CRGB* rgb() const { return mRgb.get(); }
    size_t size() const { return mPixelsCount; }
    uint8_t* alpha() { return mAlpha.get(); }
    const uint8_t* alpha() const { return mAlpha.get(); }
    uint32_t getTimestamp() const { return mTimeStamp; }
    uint32_t getFrameNumber() const { return mFrameNumber; }
    void setFrameNumber(uint32_t n) { mFrameNumber = n; }
    void setFrameNumberAndTime(uint32_t frameNumber, uint32_t timestamp) {
        mFrameNumber = frameNumber;
        mTimeStamp = timestamp;
    }
    void copy(const Frame& other);
    void interpolate(const Frame& frame1, const Frame& frame2, uint8_t amountOfFrame2);
    static void interpolate(const Frame& frame1, const Frame& frame2, uint8_t amountofFrame2, CRGB* pixels, uint8_t* alpha);
    void draw(CRGB* leds, uint8_t* alpha) const;
private:
    const size_t mPixelsCount;
    uint32_t mTimeStamp = 0;
    scoped_array<CRGB> mRgb;
    scoped_array<uint8_t> mAlpha;  // Optional alpha channel.
    uint32_t mFrameNumber = 0;  // optional
};


inline void Frame::copy(const Frame& other) {
    memcpy(mRgb.get(), other.mRgb.get(), other.mPixelsCount * sizeof(CRGB));
    mFrameNumber = other.mFrameNumber;
    mTimeStamp = other.mTimeStamp;
    if (other.mAlpha) {
        // mAlpha.reset(new uint8_t[mPixelsCount]);
        mAlpha.reset(LargeBlockAllocator<uint8_t>::Alloc(mPixelsCount));
        memcpy(mAlpha.get(), other.mAlpha.get(), mPixelsCount);
    }
}

FASTLED_NAMESPACE_END

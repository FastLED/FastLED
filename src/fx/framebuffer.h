#pragma once

#include <stdint.h>
#include "namespace.h"
#include "crgb.h"
#include "ptr.h"

FASTLED_NAMESPACE_BEGIN

// Useful for compositing.
class FrameBuffer: public Referent {
public:
    FrameBuffer(uint16_t numLeds) : mNumLeds(numLeds) {
        mLedsPtr.reset(new CRGBA[numLeds]);
        mLeds = mLedsPtr.get();
    }
    void setPixel(uint16_t i, CRGB color, uint8_t alpha = 255) {
        mLeds[i] = CRGBA(color, alpha);
    }

    void setPixel(uint16_t i, CRGBA color) {
        mLeds[i] = color;
    }

private:
    scoped_ptr<CRGBA> mLedsPtr;
    CRGBA* mLeds;
    uint16_t mNumLeds;
};

FASTLED_NAMESPACE_END

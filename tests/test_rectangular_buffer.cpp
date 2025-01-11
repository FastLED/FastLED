
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fl/ptr.h"
#include "fl/vector.h"
#include "fl/scoped_ptr.h"
#include "rgbw.h"
#include "fl/math_macros.h"

#include "fl/namespace.h"

using namespace fl;

struct Info {
    uint8_t pin = 0;
    uint16_t numLeds = 0;
    bool isRgbw = false;
    bool operator!=(const Info &other) const {
        return pin != other.pin || numLeds != other.numLeds || isRgbw != other.isRgbw;
    }
};


// Maps multiple pins and CRGB strips to a single ObjectFLED object.
class RectangularBuffer {
  public:
    typedef fl::HeapVector<Info> DrawList;

    fl::HeapVector<uint8_t> mAllLedsBufferUint8;
    DrawList mDrawList;
    DrawList mPrevDrawList;
    bool mDrawn = false;
    bool mOnPreDrawCalled = false;


    RectangularBuffer() = default;
    ~RectangularBuffer() = default;

    void onNewFrame() {
        if (!mDrawn) {
            return;
        }
        mDrawn = false;
        mOnPreDrawCalled = false;
        mDrawList.swap(mPrevDrawList);
        mDrawList.clear();
        if (!mAllLedsBufferUint8.empty()) {
            memset(&mAllLedsBufferUint8.front(), 0, mAllLedsBufferUint8.size());
            mAllLedsBufferUint8.clear();
        }
    }

    void onPreDraw() {
        if (mOnPreDrawCalled) {
            return;
        }
        // iterator through the current draw objects and calculate the total number of leds
        // that will be drawn this frame.
        uint32_t totalLeds = 0;
        for (auto it = mDrawList.begin(); it != mDrawList.end(); ++it) {
            totalLeds += it->numLeds;
        }
        if (totalLeds == 0) {
            return;
        }
        // Always assume RGB data. RGBW data will be converted to RGB data.
        mAllLedsBufferUint8.reserve(totalLeds * 3);
    }

    void addObject(uint8_t pin, uint16_t numLeds, bool is_rgbw) {
        if (is_rgbw) {
            numLeds = Rgbw::size_as_rgb(numLeds);
        }
        Info newInfo = {pin, numLeds, is_rgbw};
        mDrawList.push_back(newInfo);
    }

    uint32_t getMaxLedInStrip() const {
        uint32_t maxLed = 0;
        for (auto it = mDrawList.begin(); it != mDrawList.end(); ++it) {
            maxLed = MAX(maxLed, it->numLeds);
        }
        return maxLed;
    }

    uint32_t getTotalLeds() const {
        uint32_t numStrips = mDrawList.size();
        uint32_t maxLed = getMaxLedInStrip();
        return numStrips * maxLed;
    }
};



TEST_CASE("Rectangular Buffer") {
 
}



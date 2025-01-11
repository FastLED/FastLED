
// g++ --std=c++11 test.cpp

#include "test.h"

#include "fl/math_macros.h"
#include "fl/ptr.h"
#include "fl/scoped_ptr.h"
#include "fl/vector.h"
#include "rgbw.h"
#include "test.h"

#include "fl/namespace.h"

using namespace fl;

struct DrawItem {
    DrawItem() = default;
    DrawItem(uint8_t pin, uint16_t numLeds, bool is_rgbw): mPin(pin), mIsRgbw(is_rgbw) {
        if (is_rgbw) {
            numLeds = Rgbw::size_as_rgb(numLeds);
        } else {
            numLeds = numLeds;
        }
        mNumBytes = numLeds * 3;
    }
    uint8_t mPin = 0;
    uint32_t mNumBytes = 0;
    bool mIsRgbw = false;
    bool operator!=(const DrawItem &other) const {
        return mPin != other.mPin ||
               mNumBytes != other.mNumBytes ||
               mIsRgbw != other.mIsRgbw;
    }
};

// Maps multiple pins and CRGB strips to a single ObjectFLED object.
class RectangularBuffer {
  public:
    typedef fl::HeapVector<DrawItem> DrawList;

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
        // iterator through the current draw objects and calculate the total
        // number of bytes (representing RGB or RGBW) that will be drawn this frame.
        uint32_t total_bytes = 0;
        for (auto it = mDrawList.begin(); it != mDrawList.end(); ++it) {
            total_bytes += it->mNumBytes;
        }
        if (total_bytes == 0) {
            return;
        }
        mAllLedsBufferUint8.reserve(total_bytes);
    }

    void queue(const DrawItem& item) {
        mDrawList.push_back(item);
    }

    uint32_t getMaxBytesInStrip() const {
        uint32_t max_bytes = 0;
        for (auto it = mDrawList.begin(); it != mDrawList.end(); ++it) {
            max_bytes = MAX(max_bytes, it->mNumBytes);
        }
        return max_bytes;
    }

    uint32_t getTotalBytes() const {
        uint32_t num_strips = mDrawList.size();
        uint32_t max_bytes = getMaxBytesInStrip();
        return num_strips * max_bytes;
    }
};

TEST_CASE("Rectangular Buffer") {
    RectangularBuffer buffer;

    SUBCASE("Empty buffer has no LEDs") {
        CHECK(buffer.getTotalBytes() == 0);
        CHECK(buffer.getMaxBytesInStrip() == 0);
    }

    SUBCASE("Add one strip of 10 RGB LEDs") {
        buffer.queue(DrawItem(1, 10, false));

        CHECK(buffer.getMaxBytesInStrip() == 30);
        CHECK(buffer.getTotalBytes() == 30);
    }

    SUBCASE("Add two strips of 10 RGB LEDs") {
        buffer.queue(DrawItem(1, 10, false));
        buffer.queue(DrawItem(2, 10, false));

        CHECK(buffer.getMaxBytesInStrip() == 30);
        CHECK(buffer.getTotalBytes() == 60);
    }

    SUBCASE("Add one strip of 10 RGBW LEDs") {
        buffer.queue(DrawItem(1, 10, true));

        uint32_t num_bytes = Rgbw::size_as_rgb(10) * 3;

        CHECK(buffer.getMaxBytesInStrip() == num_bytes);
        CHECK(buffer.getTotalBytes() == num_bytes);
    }

    SUBCASE("Add one strip of 10 RGBW LEDs and one strip of 10 RGB LEDs") {
        buffer.queue(DrawItem(1, 10, true));
        buffer.queue(DrawItem(2, 10, false));

        uint32_t num_bytes = Rgbw::size_as_rgb(10) * 3;

        CHECK(buffer.getMaxBytesInStrip() == num_bytes);
        CHECK(buffer.getTotalBytes() == num_bytes * 2);
    }
}

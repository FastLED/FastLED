


#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(ARDUINO_XIAO_ESP32S3)

#define FASTLED_INTERNAL
#include "third_party/yves/I2SClockLessLedDriveresp32s3/driver.h"
#include "FastLED.h"




#include "crgb.h"
#include "eorder.h"
#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "pixel_iterator.h"
#include "fl/allocator.h"
#include "cpixel_ledcontroller.h"

#include "clockless_i2s_esp32s3.h"

namespace { // anonymous namespace

typedef fl::FixedVector<int, 16> PinList50;

typedef uint8_t Pin;


struct Info {
    uint8_t pin = 0;
    uint16_t numLeds = 0;
    bool isRgbw = false;
    bool operator!=(const Info &other) const {
        return pin != other.pin || numLeds != other.numLeds || isRgbw != other.isRgbw;
    }
};


// Maps multiple pins and CRGB strips to a single I2S_Esp32 object.
class I2SEsp32S3_Group {
  public:
    typedef fl::HeapVector<Info> DrawList;

    fl::scoped_ptr<I2SClocklessLedDriveresp32S3> mDriver;
    fl::HeapVector<uint8_t, fl::LargeBlockAllocator<uint8_t>> mAllLedsBufferUint8;
    DrawList mObjects;
    DrawList mPrevObjects;
    bool mDrawn = false;
    bool mOnPreDrawCalled = false;

    static I2SEsp32S3_Group &getInstance() {
        return fl::Singleton<I2SEsp32S3_Group>::instance();
    }

    I2SEsp32S3_Group() = default;
    ~I2SEsp32S3_Group() { mDriver.reset(); }

    void onNewFrame() {
        if (!mDrawn) {
            return;
        }
        mDrawn = false;
        mOnPreDrawCalled = false;
        mObjects.swap(mPrevObjects);
        mObjects.clear();
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
        for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
            totalLeds += it->numLeds;
        }
        if (totalLeds == 0) {
            return;
        }
        // Always assume RGB data. RGBW data will be converted to RGB data.
        mAllLedsBufferUint8.reserve(totalLeds * 3);
    }

    void addObject(Pin pin, uint16_t numLeds, bool is_rgbw) {
        if (is_rgbw) {
            numLeds = Rgbw::size_as_rgb(numLeds);
        }
        Info newInfo = {pin, numLeds, is_rgbw};
        mObjects.push_back(newInfo);
    }

    uint32_t getMaxLedInStrip() const {
        uint32_t maxLed = 0;
        for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
            maxLed = MAX(maxLed, it->numLeds);
        }
        return maxLed;
    }

    uint32_t getTotalLeds() const {
        uint32_t numStrips = mObjects.size();
        uint32_t maxLed = getMaxLedInStrip();
        return numStrips * maxLed;
    }

    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            return;
        }
        uint32_t totalLeds = getTotalLeds();
        if (totalLeds == 0) {
            return;
        }
        
        bool needs_validation = mPrevObjects != mObjects || !mDriver.get();
        if (needs_validation) {
            mDriver.reset();
            PinList50 pinList;
            for (auto it = mObjects.begin(); it != mObjects.end(); ++it) {
                pinList.push_back(it->pin);
            }

            // new Driver(totalLeds, &mAllLedsBufferUint8.front(),
            //                                CORDER_RGB, pinList.size(),
            //                                pinList.data()));
            mDriver.reset(new I2SClocklessLedDriveresp32S3());
            mDriver->initled(
                mAllLedsBufferUint8.data(),
                pinList.data(),
                pinList.size(),
                getMaxLedInStrip()
            );
        }
        mDriver->show();
        mDrawn = true;
    }
};

} // anonymous namespace


namespace fl {

void I2S_Esp32::beginShowLeds(int datapin, int nleds) {
    I2SEsp32S3_Group &group = I2SEsp32S3_Group::getInstance();
    group.onNewFrame();
}

void I2S_Esp32::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    I2SEsp32S3_Group &group = I2SEsp32S3_Group::getInstance();
    group.onPreDraw();
    const Rgbw rgbw = pixel_iterator.get_rgbw();
    int numLeds = pixel_iterator.size();
    auto& all_pixels = group.mAllLedsBufferUint8;
    if (rgbw.active()) {
        uint8_t r, g, b, w;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGBW(&r, &g, &b, &w);
            all_pixels.push_back(r);
            all_pixels.push_back(g);
            all_pixels.push_back(b);
            all_pixels.push_back(w);
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    } else {
        uint8_t r, g, b;
        while (pixel_iterator.has(1)) {
            pixel_iterator.loadAndScaleRGB(&r, &g, &b);
            all_pixels.push_back(r);
            all_pixels.push_back(g);
            all_pixels.push_back(b);
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    }
    group.addObject(data_pin, numLeds, rgbw.active());
}

void I2S_Esp32::endShowLeds() {
    // First one to call this draws everything, every other call this frame
    // is ignored.
    I2SEsp32S3_Group::getInstance().showPixelsOnceThisFrame();
}



class Driver: public InternalI2SDriver {
  public:
    Driver() = default;
    ~Driver() override = default;
    void initled(uint8_t * leds, int * pins, int numstrip,int NUM_LED_PER_STRIP) override {
        mDriver.initled(leds, pins, numstrip, NUM_LED_PER_STRIP);
    }
    void show() override {
        mDriver.show();
    }

    void setBrightness(uint8_t brightness) override {
        mDriver.setBrightness(brightness);
    }

  private:
    I2SClocklessLedDriveresp32S3 mDriver;

};

InternalI2SDriver* InternalI2SDriver::create() {
    return new Driver();
}

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32S3

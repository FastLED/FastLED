
#ifdef ESP32

#if !__has_include("esp_memory_utils.h")
#warning "esp_memory_utils.h is not available, are you on esp-idf 4? The parallel clockless i2s driver will not be available"
#else



#define CONFIG_FREERTOS_ENABLE_BACKWARD_COMPATIBILITY 1

#define FASTLED_INTERNAL
#include "FastLED.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)

#include "third_party/yves/I2SClockLessLedDriveresp32s3/driver.h"

#include "crgb.h"
#include "eorder.h"
#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "pixel_iterator.h"
#include "fl/allocator.h"
#include "fl/scoped_ptr.h"
#include "fl/assert.h"
#include "fl/rectangular_draw_buffer.h"
#include "cpixel_ledcontroller.h"

#include "clockless_i2s_esp32s3.h"

namespace { // anonymous namespace

typedef fl::FixedVector<int, 16> PinList16;

typedef uint8_t Pin;



// Maps multiple pins and CRGB strips to a single I2S_Esp32 object.
class I2SEsp32S3_Group {
  public:

    fl::scoped_ptr<fl::I2SClocklessLedDriveresp32S3> mDriver;
    fl::RectangularDrawBuffer mRectDrawBuffer;
    bool mDrawn = false;

    static I2SEsp32S3_Group &getInstance() {
        return fl::Singleton<I2SEsp32S3_Group>::instance();
    }

    I2SEsp32S3_Group() = default;
    ~I2SEsp32S3_Group() { mDriver.reset(); }

    void onNewFrame() {
        mRectDrawBuffer.onQueuingStart();
        mDrawn = false;
    }

    void onPreDraw() {
        mRectDrawBuffer.onQueuingDone();
    }

    void addObject(Pin pin, uint16_t numLeds, bool is_rgbw) {
        FASTLED_ASSERT(mRectDrawBuffer.mQueueState == fl::RectangularDrawBuffer::QUEUEING, "I2SEsp32S3_Group::addObject: not in queueing state");
        mRectDrawBuffer.queue(fl::DrawItem(pin, numLeds, is_rgbw));
    }


    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            return;
        }
        mDrawn = true;
        if (mRectDrawBuffer.mAllLedsBufferUint8.empty()) {
            return;
        }

        //static uint8_t* debug_pointer = nullptr;
        bool drawlist_changed = mRectDrawBuffer.mDrawListChangedThisFrame;
        
        bool needs_validation = !mDriver.get() || drawlist_changed;
        if (needs_validation) {
            FASTLED_WARN_IF(!drawlist_changed, "I2SEsp32S3_Group::showPixelsOnceThisFrame: changing the strip configuration is not tested after FastLED.show() is invoked");
            mDriver.reset();
            mDriver.reset(new fl::I2SClocklessLedDriveresp32S3());
            fl::FixedVector<int, 16> pinList;
            FASTLED_WARN_IF(mRectDrawBuffer.mDrawList.size() > 16, "I2SEsp32S3_Group::showPixelsOnceThisFrame: too many strips, we support a maximum of 16");
            for (auto it = mRectDrawBuffer.mDrawList.begin(); it != mRectDrawBuffer.mDrawList.end(); ++it) {
                pinList.push_back(it->mPin);
            }
            uint32_t num_strips = 0;
            uint32_t bytes_per_strip = 0;
            uint32_t total_bytes = 0;
            mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);
            FASTLED_ASSERT(bytes_per_strip % 3 == 0, "FastLED converts RGBW to RGB, so this is always a multiple of 3");
            uint32_t num_leds = total_bytes / 3;
            mDriver->initled(
                mRectDrawBuffer.mAllLedsBufferUint8.data(),
                pinList.data(),
                pinList.size(),
                num_leds
            );
        }
        mDriver->show();
    }
};

} // anonymous namespace


namespace fl {

void I2S_Esp32::beginShowLeds(int datapin, int nleds) {
    I2SEsp32S3_Group &group = I2SEsp32S3_Group::getInstance();
    group.onNewFrame();
    group.addObject(datapin, nleds, false);
}

void I2S_Esp32::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    I2SEsp32S3_Group &group = I2SEsp32S3_Group::getInstance();
    group.onPreDraw();
    const Rgbw rgbw = pixel_iterator.get_rgbw();
    int numLeds = pixel_iterator.size();
    Slice<uint8_t> strip_bytes = group.mRectDrawBuffer.getLedsBufferBytesForPin(data_pin, true);
    if (rgbw.active()) {
        uint8_t r, g, b, w;
        while (pixel_iterator.has(1)) {
            FASTLED_ASSERT(!strip_bytes.size() >= 4, "I2S_Esp32::showPixels: buffer overflow");
            pixel_iterator.loadAndScaleRGBW(&r, &g, &b, &w);
            strip_bytes[0] = r;
            strip_bytes[1] = g;
            strip_bytes[2] = b;
            strip_bytes[3] = w;
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
        }
    } else {
        uint8_t r, g, b;
        while (pixel_iterator.has(1)) {
            FASTLED_ASSERT(!strip_bytes.size() >= 3, "I2S_Esp32::showPixels: buffer overflow");
            pixel_iterator.loadAndScaleRGB(&r, &g, &b);
            strip_bytes[0] = r;
            strip_bytes[1] = g;
            strip_bytes[2] = b;
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
        }
    }
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

#endif // __has_include("esp_memory_utils.h")

#endif // ESP32

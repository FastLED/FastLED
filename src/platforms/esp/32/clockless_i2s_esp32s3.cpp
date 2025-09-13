
#if defined(ESP32)

#include "sdkconfig.h"
#include "fl/has_include.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)

#if !FL_HAS_INCLUDE("esp_memory_utils.h")
#warning "esp_memory_utils.h is not available, are you on esp-idf 4? The parallel clockless i2s driver will not be available"
#else



#define CONFIG_FREERTOS_ENABLE_BACKWARD_COMPATIBILITY 1

#define FASTLED_INTERNAL
#include "FastLED.h"

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
#include "fl/unique_ptr.h"
#include "fl/assert.h"
#include "fl/rectangular_draw_buffer.h"
#include "cpixel_ledcontroller.h"

#include "clockless_i2s_esp32s3.h"

namespace { // anonymous namespace

typedef fl::FixedVector<int, 16> PinList16;
typedef uint8_t I2SPin;  // Renamed to avoid conflict with FastLED Pin class

bool gPsramInited = false;



// Maps multiple pins and CRGB strips to a single I2S_Esp32 object.
class I2SEsp32S3_Group {
  public:

    fl::unique_ptr<fl::I2SClocklessLedDriveresp32S3> mDriver;
    fl::RectangularDrawBuffer mRectDrawBuffer;
    bool mDrawn = false;

    static I2SEsp32S3_Group &getInstance() {
        return fl::Singleton<I2SEsp32S3_Group>::instance();
    }

    I2SEsp32S3_Group() = default;
    ~I2SEsp32S3_Group() { mDriver.reset(); }

    void onQueuingStart() {
        mRectDrawBuffer.onQueuingStart();
        mDrawn = false;
    }

    void onQueuingDone() {
        mRectDrawBuffer.onQueuingDone();
    }

    void addObject(I2SPin pin, uint16_t numLeds, bool is_rgbw) {
        mRectDrawBuffer.queue(fl::DrawItem(pin, numLeds, is_rgbw));
    }


    void showPixelsOnceThisFrame() {
        if (mDrawn) {
            return;
        }
        mDrawn = true;
        if (!mRectDrawBuffer.mAllLedsBufferUint8Size) {
            return;
        }
        bool drawlist_changed = mRectDrawBuffer.mDrawListChangedThisFrame;
        bool needs_validation = !mDriver.get() || drawlist_changed;
        if (needs_validation) {
            mDriver.reset();
            mDriver.reset(new fl::I2SClocklessLedDriveresp32S3());
            fl::FixedVector<int, 16> pinList;
            for (auto it = mRectDrawBuffer.mDrawList.begin(); it != mRectDrawBuffer.mDrawList.end(); ++it) {
                pinList.push_back(it->mPin);
            }
            uint32_t num_strips = 0;
            uint32_t bytes_per_strip = 0;
            uint32_t total_bytes = 0;
            mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);
            int num_leds_per_strip = bytes_per_strip / 3;
            uint32_t total_leds = total_bytes / 3;
            mDriver->initled(
                mRectDrawBuffer.mAllLedsBufferUint8.get(),
                pinList.data(),
                pinList.size(),
                num_leds_per_strip
            );
        }
        mDriver->show();
    }
};

} // anonymous namespace


namespace fl {

void I2S_Esp32::beginShowLeds(int datapin, int nleds) {
    I2SEsp32S3_Group &group = I2SEsp32S3_Group::getInstance();
    group.onQueuingStart();
    group.addObject(datapin, nleds, false);
}

void I2S_Esp32::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    I2SEsp32S3_Group &group = I2SEsp32S3_Group::getInstance();
    group.onQueuingDone();
    const Rgbw rgbw = pixel_iterator.get_rgbw();
    int numLeds = pixel_iterator.size();
            span<uint8_t> strip_bytes = group.mRectDrawBuffer.getLedsBufferBytesForPin(data_pin, true);
    if (rgbw.active()) {
        uint8_t r, g, b, w;
        while (pixel_iterator.has(1)) {
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
            pixel_iterator.loadAndScaleRGB(&r, &g, &b);
            strip_bytes[0] = r;
            strip_bytes[1] = g;
            strip_bytes[2] = b;
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            strip_bytes.pop_front();
            pixel_iterator.advanceData();
            pixel_iterator.stepDithering();
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
    void initled(uint8_t* leds, const int * pins, int numstrip, int NUM_LED_PER_STRIP) override {
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
    if (!gPsramInited) {
        gPsramInited = true;
        bool ok = psramInit();
        if (!ok) {
            log_e("PSRAM initialization failed, I2S driver may crash.");
        }
    }
    return new Driver();
}

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32S3

#endif // FL_HAS_INCLUDE("esp_memory_utils.h")


#endif // ESP32

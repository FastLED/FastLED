
#if defined(ESP32)

#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32S3)

#define FASTLED_INTERNAL
#include "FastLED.h"

#include "crgb.h"
#include "eorder.h"
#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "pixel_iterator.h"
#include "fl/rectangular_draw_buffer.h"
#include "cpixel_ledcontroller.h"
#include "platforms/assert_defs.h"
#include "clockless_lcd_esp32s3.h"

namespace { // anonymous namespace

typedef fl::FixedVector<int, 16> PinList16;
typedef uint8_t LCDPin;

// Maps multiple pins and CRGB strips to a single LCD driver object.
// Template parameter allows separate driver instances per chipset type.
template <typename CHIPSET>
class LCDEsp32S3_Group {
  public:

    fl::unique_ptr<fl::LcdLedDriver<CHIPSET>> mDriver;
    fl::RectangularDrawBuffer mRectDrawBuffer;
    bool mDrawn = false;

    static LCDEsp32S3_Group<CHIPSET> &getInstance() {
        return fl::Singleton<LCDEsp32S3_Group<CHIPSET>>::instance();
    }

    LCDEsp32S3_Group() = default;
    ~LCDEsp32S3_Group() { mDriver.reset(); }

    void onQueuingStart() {
        mRectDrawBuffer.onQueuingStart();
        mDrawn = false;
    }

    void onQueuingDone() {
        mRectDrawBuffer.onQueuingDone();
    }

    void addObject(LCDPin pin, uint16_t numLeds, bool is_rgbw) {
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
            mDriver.reset(new fl::LcdLedDriver<CHIPSET>());

            // Build pin list and config
            fl::LcdDriverConfig config;
            config.num_lanes = 0;
            for (auto it = mRectDrawBuffer.mDrawList.begin(); it != mRectDrawBuffer.mDrawList.end(); ++it) {
                // Check for invalid USB-JTAG pins (ESP32-S2/S3)
                if (it->mPin == 19 || it->mPin == 20) {
                    FASTLED_ASSERT(false, "GPIO19 and GPIO20 are reserved for USB-JTAG on ESP32-S2/S3 and CANNOT be used for LED output. "
                                          "Using these pins WILL BREAK USB flashing capability. Please choose a different pin.");
                    return; // Don't continue if assertion doesn't halt
                }
                config.gpio_pins[config.num_lanes++] = it->mPin;
            }

            uint32_t num_strips = 0;
            uint32_t bytes_per_strip = 0;
            uint32_t total_bytes = 0;
            mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);
            int num_leds_per_strip = bytes_per_strip / 3;

            // Initialize the driver
            bool ok = mDriver->begin(config, num_leds_per_strip);
            if (!ok) {
                FASTLED_ASSERT(false, "Failed to initialize LCD driver");
                return;
            }

            // Attach strips to the driver
            CRGB* strips[16];
            for (int i = 0; i < config.num_lanes; i++) {
                // Get the LED buffer for this pin and convert to CRGB*
                fl::span<uint8_t> pin_buffer = mRectDrawBuffer.getLedsBufferBytesForPin(
                    config.gpio_pins[i], false);
                strips[i] = reinterpret_cast<CRGB*>(pin_buffer.data());
            }
            mDriver->attachStrips(strips);
        }
        mDriver->show();
    }
};

} // anonymous namespace


namespace fl {

template <typename CHIPSET>
void LCD_Esp32<CHIPSET>::beginShowLeds(int datapin, int nleds) {
    LCDEsp32S3_Group<CHIPSET> &group = LCDEsp32S3_Group<CHIPSET>::getInstance();
    group.onQueuingStart();
    group.addObject(datapin, nleds, false);
}

template <typename CHIPSET>
void LCD_Esp32<CHIPSET>::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    LCDEsp32S3_Group<CHIPSET> &group = LCDEsp32S3_Group<CHIPSET>::getInstance();
    group.onQueuingDone();
    const Rgbw rgbw = pixel_iterator.get_rgbw();

    fl::span<uint8_t> strip_bytes = group.mRectDrawBuffer.getLedsBufferBytesForPin(data_pin, true);
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

template <typename CHIPSET>
void LCD_Esp32<CHIPSET>::endShowLeds() {
    // First one to call this draws everything, every other call this frame
    // is ignored.
    LCDEsp32S3_Group<CHIPSET>::getInstance().showPixelsOnceThisFrame();
}

// Explicit template instantiations for supported chipsets
template class LCD_Esp32<WS2812ChipsetTiming>;
template class LCD_Esp32<WS2811ChipsetTiming>;
template class LCD_Esp32<WS2813ChipsetTiming>;
template class LCD_Esp32<SK6812ChipsetTiming>;
template class LCD_Esp32<TM1814ChipsetTiming>;
template class LCD_Esp32<WS2816ChipsetTiming>;

} // namespace fl

#endif  // CONFIG_IDF_TARGET_ESP32S3
#endif  // ESP32

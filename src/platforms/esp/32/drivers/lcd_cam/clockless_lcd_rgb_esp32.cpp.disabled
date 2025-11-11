
#if defined(ESP32)

#include "sdkconfig.h"

// Feature-based detection: compile RGB LCD driver if platform supports it
// The lcd_driver_rgb.h header will provide compile-time errors if headers are missing
#if defined(CONFIG_IDF_TARGET_ESP32P4) && __has_include("esp_lcd_panel_rgb.h")

#define FASTLED_INTERNAL
#include "fl/fastled.h"

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
#include "clockless_lcd_rgb_esp32.h"
#include "lcd_driver_rgb_impl.h"

namespace { // anonymous namespace

typedef fl::FixedVector<int, 16> PinList16;
typedef uint8_t LCDPin;

// Maps multiple pins and CRGB strips to a single RGB LCD driver object.
// Uses WS2812 chipset timing (most common for parallel LCD driver)
class LCDRGBEsp32_Group {
  public:

    fl::unique_ptr<fl::LcdRgbDriver<fl::WS2812ChipsetTiming>> mDriver;
    fl::RectangularDrawBuffer mRectDrawBuffer;
    bool mDrawn = false;

    static LCDRGBEsp32_Group &getInstance() {
        return fl::Singleton<LCDRGBEsp32_Group>::instance();
    }

    LCDRGBEsp32_Group() = default;
    ~LCDRGBEsp32_Group() { mDriver.reset(); }

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
            mDriver.reset(new fl::LcdRgbDriver<fl::WS2812ChipsetTiming>());

            // Build pin list and config
            fl::LcdRgbDriverConfig config;
            config.num_lanes = 0;

            // P4 RGB LCD requires PCLK and data pins
            // For now, use default GPIO assignments - these should be configurable
            // TODO: Make these configurable via user API
            config.pclk_gpio = 10;  // Example PCLK pin
            config.vsync_gpio = -1;  // Optional
            config.hsync_gpio = -1;  // Optional
            config.de_gpio = -1;     // Optional
            config.disp_gpio = -1;   // Optional

            for (auto it = mRectDrawBuffer.mDrawList.begin(); it != mRectDrawBuffer.mDrawList.end(); ++it) {
                // Validate pins using P4-specific validation
                auto result = fl::validate_esp32p4_lcd_pin(it->mPin);
                if (!result.valid) {
                    FASTLED_ASSERT(false, "GPIO" << int(it->mPin) << " validation failed: " << result.error_message);
                    return;
                }

                config.data_gpios[config.num_lanes++] = it->mPin;
            }

            uint32_t num_strips = 0;
            uint32_t bytes_per_strip = 0;
            uint32_t total_bytes = 0;
            mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);
            int num_leds_per_strip = bytes_per_strip / 3;

            // Initialize PSRAM if not already done
            // Skip PSRAM initialization in QEMU environment (not emulated)
            #ifndef FASTLED_ESP32_IS_QEMU
            static bool gPsramInited = false;
            if (!gPsramInited) {
                gPsramInited = true;
                bool psram_ok = psramInit();
                if (!psram_ok) {
                    log_e("PSRAM initialization failed, LCD driver will use internal RAM.");
                }
            }
            #endif

            // Initialize the driver
            bool ok = mDriver->begin(config, num_leds_per_strip);
            if (!ok) {
                FASTLED_ASSERT(false, "Failed to initialize P4 LCD driver");
                return;
            }

            // Attach strips to the driver
            CRGB* strips[16];
            for (int i = 0; i < config.num_lanes; i++) {
                // Get the LED buffer for this pin and convert to CRGB*
                fl::span<uint8_t> pin_buffer = mRectDrawBuffer.getLedsBufferBytesForPin(
                    config.data_gpios[i], false);
                strips[i] = reinterpret_cast<CRGB*>(pin_buffer.data());
            }
            mDriver->attachStrips(strips);
        }
        mDriver->show();
    }
};

} // anonymous namespace


namespace fl {

void LCD_RGB_Esp32::beginShowLeds(int datapin, int nleds) {
    LCDRGBEsp32_Group &group = LCDRGBEsp32_Group::getInstance();
    group.onQueuingStart();
    group.addObject(datapin, nleds, false);
}

void LCD_RGB_Esp32::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    LCDRGBEsp32_Group &group = LCDRGBEsp32_Group::getInstance();
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

void LCD_RGB_Esp32::endShowLeds() {
    // First one to call this draws everything, every other call this frame
    // is ignored.
    LCDRGBEsp32_Group::getInstance().showPixelsOnceThisFrame();
}

// Explicit template instantiation for WS2812 chipset (forces compilation)
template class LcdRgbDriver<WS2812ChipsetTiming>;

} // namespace fl

#endif  // CONFIG_IDF_TARGET_ESP32P4
#endif  // ESP32

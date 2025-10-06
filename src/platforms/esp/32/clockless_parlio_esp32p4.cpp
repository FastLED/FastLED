#if defined(ESP32)
#include "sdkconfig.h"

#if defined(CONFIG_IDF_TARGET_ESP32P4)

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
#include "fl/allocator.h"
#include "fl/unique_ptr.h"
#include "fl/assert.h"
#include "fl/rectangular_draw_buffer.h"
#include "cpixel_ledcontroller.h"

#include "clockless_parlio_esp32p4.h"
#include "parlio/parlio_driver.h"

namespace { // anonymous namespace

typedef uint8_t ParlioPin;

/// @brief Manages all PARLIO strips and bulk driver
class ParlioEsp32P4_Group {
public:
    fl::unique_ptr<fl::ParlioLedDriverBase> mDriver;
    fl::RectangularDrawBuffer mRectDrawBuffer;
    bool mDrawn = false;
    uint8_t mClkPin = 9;  // Default clock pin (GPIO 9)

    static ParlioEsp32P4_Group& getInstance() {
        return fl::Singleton<ParlioEsp32P4_Group>::instance();
    }

    ParlioEsp32P4_Group() = default;
    ~ParlioEsp32P4_Group() { mDriver.reset(); }

    void onQueuingStart() {
        mRectDrawBuffer.onQueuingStart();
        mDrawn = false;
    }

    void onQueuingDone() {
        mRectDrawBuffer.onQueuingDone();
    }

    void addObject(ParlioPin pin, uint16_t numLeds, bool is_rgbw) {
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

            // Build pin list
            fl::FixedVector<int, 16> pinList;
            for (auto it = mRectDrawBuffer.mDrawList.begin();
                 it != mRectDrawBuffer.mDrawList.end(); ++it) {
                pinList.push_back(it->mPin);
            }

            // Get buffer info
            uint32_t num_strips = 0;
            uint32_t bytes_per_strip = 0;
            uint32_t total_bytes = 0;
            mRectDrawBuffer.getBlockInfo(&num_strips, &bytes_per_strip, &total_bytes);
            int num_leds_per_strip = bytes_per_strip / 3;

            // Auto-select optimal width
            uint8_t optimal_width = selectOptimalWidth(num_strips);

            // Instantiate driver based on width
            switch (optimal_width) {
                case 1:
                    mDriver.reset(new fl::ParlioLedDriver<1, fl::WS2812ChipsetTiming>());
                    break;
                case 2:
                    mDriver.reset(new fl::ParlioLedDriver<2, fl::WS2812ChipsetTiming>());
                    break;
                case 4:
                    mDriver.reset(new fl::ParlioLedDriver<4, fl::WS2812ChipsetTiming>());
                    break;
                case 8:
                    mDriver.reset(new fl::ParlioLedDriver<8, fl::WS2812ChipsetTiming>());
                    break;
                case 16:
                    mDriver.reset(new fl::ParlioLedDriver<16, fl::WS2812ChipsetTiming>());
                    break;
                default:
                    FL_ASSERT(false, "Invalid PARLIO width: " << int(optimal_width));
                    return;
            }

            // Configure driver
            fl::ParlioDriverConfig config;
            config.clk_gpio = mClkPin;
            config.num_lanes = num_strips;
            config.clock_freq_hz = 12000000;  // 12 MHz
            config.buffer_strategy = fl::ParlioBufferStrategy::BREAK_PER_COLOR;  // Use LSB-safe buffer breaking

            for (int i = 0; i < num_strips; i++) {
                config.data_gpios[i] = pinList[i];
            }

            // Initialize
            bool ok = mDriver->begin(config, num_leds_per_strip);
            if (!ok) {
                FL_ASSERT(false, "Failed to initialize PARLIO driver");
                return;
            }

            // Attach strips
            for (int i = 0; i < num_strips; i++) {
                fl::span<uint8_t> pin_buffer =
                    mRectDrawBuffer.getLedsBufferBytesForPin(pinList[i], false);
                CRGB* strip = reinterpret_cast<CRGB*>(pin_buffer.data());
                mDriver->set_strip(i, strip);
            }
        }

        // Show and wait
        mDriver->show_grb();
        mDriver->wait();
    }

private:
    uint8_t selectOptimalWidth(uint8_t num_strips) {
        if (num_strips <= 1) return 1;
        if (num_strips <= 2) return 2;
        if (num_strips <= 4) return 4;
        if (num_strips <= 8) return 8;
        return 16;
    }
};

} // anonymous namespace

namespace fl {

void Parlio_Esp32P4::beginShowLeds(int data_pin, int nleds) {
    ParlioEsp32P4_Group& group = ParlioEsp32P4_Group::getInstance();
    group.onQueuingStart();
    group.addObject(data_pin, nleds, false);
}

void Parlio_Esp32P4::showPixels(uint8_t data_pin, PixelIterator& pixel_iterator) {
    ParlioEsp32P4_Group& group = ParlioEsp32P4_Group::getInstance();
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

void Parlio_Esp32P4::endShowLeds() {
    ParlioEsp32P4_Group::getInstance().showPixelsOnceThisFrame();
}

} // namespace fl

#endif // CONFIG_IDF_TARGET_ESP32P4
#endif // ESP32

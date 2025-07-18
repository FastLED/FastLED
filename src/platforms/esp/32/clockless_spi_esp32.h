#pragma once


#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "crgb.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "spi_ws2812/strip_spi.h"
#include "fl/unique_ptr.h"
#include "fl/assert.h"

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessSpiWs2812Controller : public CPixelLEDController<RGB_ORDER>
{
private:
    // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");
    fl::unique_ptr<ISpiStripWs2812> mLedStrip;

public:
    ClocklessSpiWs2812Controller() = default;

    void init() override { }
    virtual uint16_t getMaxRefreshRate() const { return 800; }

protected:
    // Prepares data for the draw.
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override
    {
        auto rgbw = this->getRgbw();
        const bool is_rgbw = rgbw.active();
        PixelIterator iterator = pixels.as_iterator(rgbw);
        if (!mLedStrip) {
            auto strip = ISpiStripWs2812::Create(DATA_PIN, iterator.size(), is_rgbw);
            mLedStrip.reset(strip);
        }
        else {
            FASTLED_ASSERT(
                mLedStrip->numPixels() == iterator.size(),
                "mLedStrip->numPixels() (" << mLedStrip->numPixels() << ") != pixels.size() (" << iterator.size() << ")");
        }
        auto output_iterator = mLedStrip->outputIterator();
        if (is_rgbw) {
            uint8_t r, g, b, w;
            for (uint16_t i = 0; iterator.has(1); i++) {
                iterator.loadAndScaleRGBW(&r, &g, &b, &w);
                output_iterator(r);
                output_iterator(g);
                output_iterator(b);
                output_iterator(w);
                iterator.advanceData();
                iterator.stepDithering();
            }
        } else {
            uint8_t r, g, b;
            for (uint16_t i = 0; iterator.has(1); i++) {
                iterator.loadAndScaleRGB(&r, &g, &b);
                output_iterator(r);
                output_iterator(g);
                output_iterator(b);
                iterator.advanceData();
                iterator.stepDithering();
            }
        }
        output_iterator.finish();
        mLedStrip->drawAsync();
    }
};

#endif  // FASTLED_RMT5

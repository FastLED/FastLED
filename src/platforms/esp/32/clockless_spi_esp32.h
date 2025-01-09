#pragma once

// signal to the world that we have a ClocklessController to allow WS2812 and others.
#define FASTLED_HAS_CLOCKLESS 1

#include "crgb.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "third_party/espressif/led_strip/strip_spi.h"
#include "fl/scoped_ptr.h"
#include "fl/assert.h"

template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class ClocklessSpiWs2812Controller : public CPixelLEDController<RGB_ORDER>
{
private:
    // -- Verify that the pin is valid
    static_assert(FastPin<DATA_PIN>::validpin(), "Invalid pin specified");
    fl::scoped_ptr<ISpiStripWs2812> mLedStrip;

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
            auto strip = ISpiStripWs2812::Create(
                DATA_PIN, iterator.size(), is_rgbw,
            );
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


template <int DATA_PIN, EOrder RGB_ORDER = GRB>
class ClocklessSpiInvalidController : public CPixelLEDController<RGB_ORDER>
{
public:
    ClocklessSpiInvalidController() = default;

    void init() override {
        FASTLED_ASSERT(false, "Spi Controller only works for WS2812");
    }
    virtual uint16_t getMaxRefreshRate() const { return 800; }
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override {
        FASTLED_ASSERT(false, "Spi Controller only works for WS2812");
    }
};

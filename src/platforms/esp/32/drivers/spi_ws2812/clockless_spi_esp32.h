// ok no namespace fl
#pragma once


#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI

#include "crgb.h"
#include "eorder.h"
#include "pixel_iterator.h"
#include "strip_spi.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/assert.h"
#include "fl/chipsets/timing_traits.h"

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessSpiWs2812Controller : public CPixelLEDController<RGB_ORDER>
{
private:
    // -- Verify that the pin is valid
    static_assert(fl::FastPin<DATA_PIN>::validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");
    fl::unique_ptr<fl::ISpiStripWs2812> mLedStrip;

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
            auto strip = fl::ISpiStripWs2812::Create(DATA_PIN, iterator.size(), is_rgbw);
            mLedStrip.reset(strip);
        }
        else {
            FASTLED_ASSERT(
                mLedStrip->numPixels() == iterator.size(),
                "mLedStrip->numPixels() (" << mLedStrip->numPixels() << ") != pixels.size() (" << iterator.size() << ")");
        }
        auto output_iterator = mLedStrip->outputIterator();
        iterator.writeWS2812(&output_iterator);
        output_iterator.finish();
        mLedStrip->drawAsync();
    }
};

// Convenient alias for SPI-based clockless controller (legacy)
// Note: New ChannelEngine-based ClocklessSPI is defined in idf5_clockless_spi_esp32.h
// This alias is preserved for backward compatibility with code that explicitly uses ClocklessSpiWs2812Controller
namespace fl {
// Only define alias if new driver isn't already defining it
#if !defined(FL_CLOCKLESS_SPI_CHANNEL_ENGINE_DEFINED)
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
using ClocklessSPI = ::ClocklessSpiWs2812Controller<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME>;
#endif
}

#endif  // FASTLED_RMT5

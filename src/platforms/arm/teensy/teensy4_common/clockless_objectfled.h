/// FastLED mapping of the ObjectFLED driver for Teensy 4.0/4.1.
///
/// This driver supports up to 42 parallel strips of ANY clockless LED chipsets!
/// ~7x that of OctoWS2811!
///
/// NEW: Generic proxy architecture supporting ALL clockless chipsets:
/// - WS2812, WS2811, WS2813, WS2815
/// - SK6812, SK6822
/// - And any other clockless chipset with timing definitions
///
/// The theoretical limit of Teensy 4.0, if frames per second is not a concern, is
/// more than 200k pixels. However, realistically, to run 42 strips at 550 pixels
/// each at 60fps, is 23k pixels.
///
/// @author Kurt Funderburg
/// @reddit: reddit.com/u/Tiny_Structure_7
/// The FastLED code was written by Zach Vorhies
/// @author Kurt Funderburg
/// @reddit: reddit.com/u/Tiny_Structure_7
/// @author: Zach Vorhies (FastLED code)
/// @reddit: reddit.com/u/ZachVorhies


#pragma once

#include "cpixel_ledcontroller.h"
#include "pixel_iterator.h"
#include "fl/vector.h"

#ifndef FASTLED_OBJECTFLED_LATCH_DELAY
#define FASTLED_OBJECTFLED_LATCH_DELAY 300  // WS2812-5VB
#endif

// Forward declare so templates can reference them
// Full definitions are in the .cpp file
namespace fl {
template <typename TIMING> class ObjectFLEDGroup;
class ObjectFLEDRegistry;

/// Generic proxy controller for ObjectFLED on Teensy 4.x
/// Works with ANY clockless chipset timing (WS2812, SK6812, WS2811, etc.)
///
/// Automatically routes to the correct ObjectFLEDGroup<TIMING> singleton
/// based on the TIMING template parameter.
template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_ObjectFLED_Proxy
    : public CPixelLEDController<RGB_ORDER> {
  private:
    typedef CPixelLEDController<RGB_ORDER> Base;

  public:
    ClocklessController_ObjectFLED_Proxy();

    void init() override {}

    virtual uint16_t getMaxRefreshRate() const override {
        // Calculate based on timing: if total period > 2000ns, it's 400kHz, else 800kHz
        uint32_t total_ns = TIMING::T1 + TIMING::T2 + TIMING::T3;
        return (total_ns > 2000) ? 400 : 800;
    }

  protected:
    virtual void *beginShowLeds(int nleds) override;
    virtual void showPixels(PixelController<RGB_ORDER> &pixels) override;
    virtual void endShowLeds(void *data) override;
};

// ============================================================================
// Template Member Function Implementations (must be in header for visibility)
// ============================================================================

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::ClocklessController_ObjectFLED_Proxy()
    : Base() {
    // Latch delay is automatically determined from TIMING::RESET
}

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
void *ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::beginShowLeds(int nleds) {
    void *data = Base::beginShowLeds(nleds);

    // Auto-grab the singleton for THIS chipset type
    ObjectFLEDGroup<TIMING>& group = ObjectFLEDGroup<TIMING>::getInstance();

    // Flush any pending groups of DIFFERENT chipset types
    ObjectFLEDRegistry::getInstance().flushAllExcept(&group);

    // Start queuing for this group
    group.onQueuingStart();

    return data;
}

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
void ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::showPixels(
    PixelController<RGB_ORDER> &pixels) {
    // Auto-grab the singleton for THIS chipset type
    ObjectFLEDGroup<TIMING>& group = ObjectFLEDGroup<TIMING>::getInstance();

    // Add this strip to the group
    auto pixel_iterator = pixels.as_iterator(this->getRgbw());
    group.addStrip(DATA_PIN, pixel_iterator);
}

template <typename TIMING, int DATA_PIN, EOrder RGB_ORDER>
void ClocklessController_ObjectFLED_Proxy<TIMING, DATA_PIN, RGB_ORDER>::endShowLeds(void *data) {
    Base::endShowLeds(data);

    // DON'T flush here - let chipset change detection or frame end handle it
    // This is handled by the next controller's beginShowLeds() or FastLED.show() end
}

// Legacy WS2812-specific controller (for backward compatibility)
// Now just a typedef to the proxy with WS2812 timing
template <int DATA_PIN, EOrder RGB_ORDER = RGB>
using ClocklessController_ObjectFLED_WS2812 = ClocklessController_ObjectFLED_Proxy<
    struct TIMING_WS2812_800KHZ,
    DATA_PIN,
    RGB_ORDER>;

} // namespace fl
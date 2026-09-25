#pragma once

// IWYU pragma: private

// Channel-based clockless controller for stub platform
// Models ESP32's channel-based clockless architecture for testing

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#define FL_CLOCKLESS_STUB_CHANNEL_ENGINE_DEFINED 1
#define FASTLED_CLOCKLESS_STUB_DEFINED 1

#include "eorder.h"
#include "fl/stl/compiler_control.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/channels/bus.h"
#include "fl/channels/data.h"
#include "fl/channels/slim_bridge_controller.h"
#include "pixel_iterator.h"
#include "fl/log/log.h"
#include "fl/stl/vector.h"
#include "platforms/shared/active_strip_tracker/active_strip_tracker.h"
#include "platforms/stub/bus_traits.h"
#include "platforms/stub/stub_gpio.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Channel-based clockless controller for stub platform
///
/// This controller integrates with the channel driver infrastructure,
/// allowing the legacy FastLED.addLeds<>() API to route through channel drivers
/// for testing. It mirrors the architecture of ESP32's ClocklessIdf5.
///
/// The stub registers `BusTraits<Bus::BIT_BANG>` with `ChannelManager` via
/// `registerWithManager()` in the `SlimBridgeController` base constructor;
/// frames are flushed by `ChannelManager::onEndFrame()`.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::BIT_BANG>> {
private:
    // LED capture tracker for simulation/testing
    ActiveStripTracker mTracker;
    fl::vector<u8> mCaptureData;

public:
#if defined(FASTLED_TESTING)
    /// @brief Encoded controller output for host backend-path tests.
    const ChannelDataPtr& channelDataForTesting() const FL_NO_EXCEPT {
        return this->channelData();
    }
#endif

protected:
    virtual void onBeforeEncode(PixelController<RGB_ORDER>& pixels) FL_NO_EXCEPT override
    {
        // Capture LED data for simulation/testing BEFORE encoding
        // Use separate pixel controller with RGB order and no color adjustment
        mCaptureData.clear();
        PixelController<RGB> pixels_rgb = pixels; // Converts to RGB pixels
        // NOTE: disableColorAdjustment() only sets color=white but keeps brightness
        // We need to manually ensure full brightness for accurate capture
        #if FASTLED_HD_COLOR_MIXING
        pixels_rgb.mColorAdjustment.brightness = 255;
        #endif
        pixels_rgb.disableColorAdjustment();
        auto capture_iterator = pixels_rgb.as_iterator(RgbwInvalid());
        capture_iterator.writeWS2812(&mCaptureData);
        mTracker.update(mCaptureData);
    }
};

// Adapter for timing-like objects via duck typing
template <int DATA_PIN, typename TIMING_LIKE, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
struct ClocklessControllerAdapter : public ClocklessController<DATA_PIN, TIMING_LIKE, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {
    // Inherits all functionality from ClocklessController
};

// ClocklessBlockController for type-based timing
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessBlockController : public ClocklessController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {
    // Inherits all functionality from ClocklessController
};

}  // namespace fl

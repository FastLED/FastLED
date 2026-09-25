#pragma once

// IWYU pragma: private

// Channel-based clockless controller for WASM platform
// Models stub platform's channel driver integration for web builds

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#define FL_CLOCKLESS_WASM_CHANNEL_ENGINE_DEFINED 1

#include "eorder.h"
#include "fl/stl/compiler_control.h"
#include "fl/channels/slim_bridge_controller.h"
#include "fl/stl/vector.h"
#include "platforms/shared/active_strip_tracker/active_strip_tracker.h"
#include "platforms/stub/bus_traits.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief Channel-based clockless controller for WASM platform
///
/// This controller integrates with the channel driver infrastructure,
/// allowing the legacy FastLED.addLeds<>() API to route through channel drivers
/// for web builds. Uses stub driver (no real hardware in browser).
///
/// Registered with `ChannelManager` via the `SlimBridgeController` base
/// constructor and flushed by `ChannelManager::onEndFrame()`.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public SlimBridgeController<DATA_PIN, TIMING, RGB_ORDER, WAIT_TIME, BusTraits<Bus::BIT_BANG>> {
private:
    // LED capture tracker for ActiveStripData (feeds frame data to JavaScript)
    ActiveStripTracker mTracker;
    fl::vector<u8> mCaptureData;

public:
    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }

protected:
    // Feed ActiveStripData BEFORE the frame is encoded so JavaScript can
    // retrieve it via getFrameData().
    void onBeforeEncode(PixelController<RGB_ORDER>& pixels) FL_NO_EXCEPT override {
        mCaptureData.clear();
        PixelController<RGB> pixels_rgb = pixels;
        // disableColorAdjustment() removes color correction but keeps brightness
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

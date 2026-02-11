#pragma once

// Channel-based clockless controller for WASM platform
// Models stub platform's channel engine integration for web builds

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#define FL_CLOCKLESS_WASM_CHANNEL_ENGINE_DEFINED 1

#include "eorder.h"
#include "fl/unused.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/channels/data.h"
#include "fl/channels/engine.h"
#include "fl/channels/bus_manager.h"
#include "pixel_iterator.h"
#include "fl/warn.h"

namespace fl {

/// @brief Channel-based clockless controller for WASM platform
///
/// This controller integrates with the channel engine infrastructure,
/// allowing the legacy FastLED.addLeds<>() API to route through channel engines
/// for web builds. Uses stub engine (no real hardware in browser).
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
private:
    // Channel data for transmission
    ChannelDataPtr mChannelData;

    // Channel engine reference (selected dynamically from bus manager)
    fl::shared_ptr<IChannelEngine> mEngine;

public:
    ClocklessController()
        : mEngine(getWasmEngine())
    {
        // Create channel data with pin and timing configuration
        ChipsetTimingConfig timing = makeTimingConfig<TIMING>();
        mChannelData = ChannelData::create(DATA_PIN, timing);
    }

    virtual void init() override { }
    virtual u16 getMaxRefreshRate() const { return 400; }

protected:
    // -- Show pixels
    //    This is the main entry point for the controller.
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override
    {
        if (!mEngine) {
            FL_WARN_EVERY(100, "No Engine");
            return;
        }
        // Wait for previous transmission to complete and release buffer
        // This prevents race conditions when show() is called faster than hardware can transmit
        u32 startTime = fl::millis();
        u32 lastWarnTime = startTime;
        if (mChannelData->isInUse()) {
            FL_WARN_EVERY(100, "ClocklessController(wasm): engine should have finished transmitting by now - waiting");
            bool finished = mEngine->waitForReady();
            if (!finished) {
                FL_ERROR("ClocklessController(wasm): Engine still busy after " << fl::millis() - startTime << "ms");
                return;
            }
        }

        // Convert pixels to encoded byte data
        fl::PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        auto& data = mChannelData->getData();
        data.clear();
        iterator.writeWS2812(&data);

        // Enqueue for transmission (will be sent when engine->show() is called)
        mEngine->enqueue(mChannelData);
    }

    static fl::shared_ptr<IChannelEngine> getWasmEngine() {
        return ChannelBusManager::instance().getEngineByName("STUB");
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

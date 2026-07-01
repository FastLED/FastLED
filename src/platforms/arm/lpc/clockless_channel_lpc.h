#pragma once

// IWYU pragma: private

/// @file clockless_channel_lpc.h
/// @brief LPC845 channels-API ClocklessController adapter
///
/// Routes the classic `addLeds<WS2812, PIN, GRB>()` API through the LPC845
/// SCT+DMA channels-API engine (`ChannelEngineLpcSctDma`) via
/// `BusTraits<Bus::BIT_BANG>::instancePtr()`. Meta issue #3517 Phase A.2.
///
/// Follows the exact pattern established by
/// `src/platforms/wasm/clockless_channel_wasm.h` and
/// `src/platforms/stub/clockless_channel_stub.h`: define
/// `FL_CLOCKLESS_CONTROLLER_DEFINED` so the fallback template
/// selection in `fastled_arm_lpc.h` compiles out; expose a
/// `ClocklessController<>` template that enqueues into the channels
/// engine on every `showPixels()`.
///
/// Only compiles when the user opts into the SCT+DMA path with
/// `-DFASTLED_LPC_PWM_DMA=1`. Without that flag `fastled_arm_lpc.h`
/// still picks the bit-bang default from `clockless_arm_lpc.h`.
///
/// On host/stub builds the engine's transmitter is a no-op (see
/// `lpc_sct_dma_runtime.cpp.hpp` host-mode block), so `addLeds<>` +
/// `FastLED.show()` compile and run without silicon. Real silicon
/// verification lands under Phase A.1 of #3517.

#include "platforms/arm/lpc/is_lpc.h"
#include "platforms/is_platform.h"

// Only take over the ClocklessController template when the user has opted
// into the SCT+DMA fast path. Without FASTLED_LPC_PWM_DMA the sketch keeps
// using the bit-bang controller from clockless_arm_lpc.h.
#if defined(FL_IS_ARM_LPC_845) && defined(FASTLED_LPC_PWM_DMA)

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#define FL_CLOCKLESS_LPC_CHANNEL_ENGINE_DEFINED 1

#include "eorder.h"
#include "fl/channels/bus.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/log/log.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"
#include "pixel_iterator.h"
#include "platforms/arm/lpc/drivers/sct_dma/bus_traits.h"

namespace fl {

/// @brief LPC845 channels-API `ClocklessController` — routes addLeds<> to
///        the SCT+DMA engine via `BusTraits<Bus::BIT_BANG>`.
///
/// Template parameters mirror the classic `ClocklessController<>` template
/// so `addLeds<WS2812, PIN, GRB>()` binds without any user-facing change.
/// The SCT+DMA engine handles the actual byte-to-wire timing.
template <int DATA_PIN,
          typename TIMING,
          EOrder RGB_ORDER = RGB,
          int XTRA0 = 0,
          bool FLIP = false,
          int WAIT_TIME = 0>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
private:
    ChannelDataPtr mChannelData;
    fl::shared_ptr<IChannelDriver> mDriver;

public:
    ClocklessController()
        : mDriver(getLpcEngine())
    {
        ChipsetTimingConfig timing = makeTimingConfig<TIMING>();
        mChannelData = ChannelData::create(DATA_PIN, timing);
    }

    void init() override {}
    u16 getMaxRefreshRate() const override { return 400; }

protected:
    virtual void showPixels(PixelController<RGB_ORDER>& pixels) override {
        if (!mDriver) {
            FL_WARN_F_EVERY(100, "LPC channel engine unavailable");
            return;
        }
        // Wait for any prior transmission to release the buffer. Prevents
        // races when show() is called faster than the SCT+DMA path drains.
        if (mChannelData->isInUse()) {
            bool finished = mDriver->waitForReady();
            if (!finished) {
                FL_ERROR_F("LPC clockless: engine still busy after wait");
                return;
            }
        }

        // Encode pixels → GRB byte stream into the channel-data buffer.
        fl::PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        auto& data = mChannelData->getData();
        data.clear();
        iterator.writeWS2812(&data);

        // Hand off to the engine. The next FastLED.show() drain
        // (either via ChannelManager::show or a direct BusTraits show)
        // kicks the SCT+DMA stream.
        mDriver->enqueue(mChannelData);
        mDriver->show();
    }

    static fl::shared_ptr<IChannelDriver> getLpcEngine() FL_NO_EXCEPT {
        // Same ODR-link pattern as `getWasmEngine()` in
        // `clockless_channel_wasm.h`: naming
        // `BusTraits<Bus::BIT_BANG>::instancePtr()` here forces the
        // LPC-specific specialization from
        // `drivers/sct_dma/bus_traits.h` to be link-visible.
        return BusTraits<Bus::BIT_BANG>::instancePtr();
    }
};

// Adapter for timing-like objects via duck typing. Matches the WASM /
// stub adapter signature so external code that instantiates it
// (e.g. FastLED chipset wrappers) still compiles.
template <int DATA_PIN, typename TIMING_LIKE, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
struct ClocklessControllerAdapter
    : public ClocklessController<DATA_PIN, TIMING_LIKE, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {};

// ClocklessBlockController for type-based timing (same behavior — the
// engine handles multi-block internally).
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessBlockController
    : public ClocklessController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_PWM_DMA

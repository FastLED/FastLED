/// @file slim_bridge_controller.h
/// @brief Shared "slim bridge" legacy `addLeds<>()` controller for channel drivers.
///
/// Extracted (issue #4581, gap register A1-A12 in #4580) from the ESP32 RMT5
/// legacy bridge prototyped in draft PR #4576
/// (`platforms/esp/32/drivers/rmt/rmt_5/idf5_clockless.h`) and the
/// `ClocklessIdf4` `reset_us` folding pattern. This header has NO platform
/// includes -- it is pure `fl::` glue over `IChannelDriver` so every
/// platform's legacy bridge controller can share one implementation and one
/// conformance suite instead of re-deriving the enqueue/wait/encode dance
/// per driver.

#pragma once

#include "fl/stl/static_assert.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/vector.h"

#include "cpixel_ledcontroller.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/channels/bus_traits.h"  // IWYU pragma: keep (DriverTraits<Bus::X> satisfies the DriverTraits contract)
#include "fl/channels/config.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/log/log.h"
#include "fl/system/sketch_macros.h"

/// Small-memory tier: the bridge drives its driver directly instead of
/// through ChannelManager, so the registry (~3 KB) is not linked. Measured on
/// the LPC845, where it decides whether AutoResearch fits in 64 KB
/// (FastLED#4671). Override with -DFL_SLIM_BRIDGE_DIRECT_SHOW=0/1.
#ifndef FL_SLIM_BRIDGE_DIRECT_SHOW
#define FL_SLIM_BRIDGE_DIRECT_SHOW (!FL_PLATFORM_HAS_LARGE_MEMORY)
#endif

namespace fl {

/// How a SlimBridgeController reaches its driver. Specialized (not an
/// `if`) so the direct variant never names ChannelManager at all.
template <bool DIRECT_SHOW>
struct SlimBridgeShowPolicy;

/// Default: registered with ChannelManager, which owns enable/disable and
/// flushes every registered driver once at the end of FastLED.show().
template <>
struct SlimBridgeShowPolicy<false> {
    template <typename DriverTraits>
    static void attach() FL_NO_EXCEPT { DriverTraits::registerWithManager(); }
    static bool enabled(IChannelDriver& driver) FL_NO_EXCEPT {
        return ChannelManager::registry().isDriverEnabled(driver);
    }
    static void afterEnqueue(IChannelDriver&) FL_NO_EXCEPT {}
};

/// Direct: no registry, so there is no runtime enable/disable or
/// exclusive-driver selection, and no end-of-show flush -- the controller
/// shows its own frame, as the pre-bridge LPC controllers did.
template <>
struct SlimBridgeShowPolicy<true> {
    template <typename DriverTraits>
    static void attach() FL_NO_EXCEPT {}
    static bool enabled(IChannelDriver&) FL_NO_EXCEPT { return true; }
    static void afterEnqueue(IChannelDriver& driver) FL_NO_EXCEPT { driver.show(); }
};

/// @brief Shared legacy `addLeds<>()` bridge controller over an `IChannelDriver`.
///
/// This is the driver-agnostic "slim bridge": a `CPixelLEDController` that
/// owns one `ChannelDataPtr`, builds it once at construction, and on every
/// `showPixels()` call re-encodes into that same buffer (no per-frame
/// allocation) and hands it to the driver named by `DriverTraits`.
///
/// `DriverTraits` contract (mirrors `fl::BusTraits<Bus::X>`, see
/// `fl/channels/bus_traits.h`):
///   - `static IChannelDriver& instance() FL_NO_EXCEPT;`
///       Meyers-singleton accessor for the concrete driver.
///   - `static void registerWithManager() FL_NO_EXCEPT;`
///       Registers `instance()` with `ChannelManager` at some priority.
///       MUST be idempotent -- called from every `SlimBridgeController`
///       instantiation's constructor. `fl::BusTraits<Bus::X>` (see
///       `fl/channels/bus_traits.h`) already satisfies this contract, so
///       any existing `BusTraits<Bus::X>` specialization can be passed
///       directly as `DriverTraits`.
///
/// @tparam DATA_PIN    GPIO data pin for the clockless chipset
/// @tparam TIMING      Compile-time chipset timing trait (see
///                     `fl/chipsets/chipset_timing_config.h`,
///                     `makeTimingConfig<TIMING>()`)
/// @tparam RGB_ORDER   Color byte ordering (RGB, GBR, etc.)
/// @tparam WAIT_TIME   Minimum inter-frame wait time in microseconds. If
///                     greater than the TIMING trait's `reset_us`, this
///                     folds into the built `ChannelData`'s timing so the
///                     driver honors the longer of the two (mirrors the
///                     historical `ClocklessIdf4` `reset_us` folding).
/// @tparam DriverTraits Trait type satisfying the contract documented above.
/// @tparam XTRA0       Legacy clockless trailing zero bits per byte, stored
///                     on the ChannelData for engines that honour it.
/// @tparam DIRECT_SHOW Bypass ChannelManager (see SlimBridgeShowPolicy).
///                     Defaults to the platform tier via
///                     FL_SLIM_BRIDGE_DIRECT_SHOW.
template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER, int WAIT_TIME, typename DriverTraits,
          int XTRA0 = 0, bool DIRECT_SHOW = (FL_SLIM_BRIDGE_DIRECT_SHOW != 0)>
class SlimBridgeController : public CPixelLEDController<RGB_ORDER> {
    using Policy = SlimBridgeShowPolicy<DIRECT_SHOW>;

public:
    SlimBridgeController() FL_NO_EXCEPT {
        ChipsetTimingConfig timing = makeTimingConfig<TIMING>();
        // (A?) Fold WAIT_TIME (microseconds) into reset_us, mirroring the
        // historical ClocklessIdf4 behavior: the driver must honor whichever
        // is longer -- the chipset's own latch/reset requirement, or the
        // caller-requested minimum inter-frame wait.
        if (WAIT_TIME > 0 && static_cast<u32>(WAIT_TIME) > timing.reset_us) {
            timing.reset_us = static_cast<u32>(WAIT_TIME);
        }
        // A clockless controller bound to a unified parallel-IO bus needs
        // that engine's clockless pipeline linked (#4793).
        detail::enable_clockless<DriverTraits>();
        mData = ChannelData::create(DATA_PIN, timing, fl::vector_psram<u8>(),
                                     ChannelPixelFormat::RGB);
        FL_STATIC_ASSERT(XTRA0 >= 0 && XTRA0 <= 32, "XTRA0 out of range");
        mData->setExtraZeroBitsPerByte(static_cast<u8>(XTRA0));
        // Idempotent by contract -- safe to call from every instantiation's
        // constructor (mirrors ClocklessIdf5's registerWithManager() call).
        // A no-op in direct-show mode.
        Policy::template attach<DriverTraits>();
    }

    void init() FL_NO_EXCEPT override {}

    /// @brief Expose the underlying `ChannelDataPtr` for conformance tests.
    const ChannelDataPtr& channelData() const FL_NO_EXCEPT { return mData; }

protected:
    void showPixels(PixelController<RGB_ORDER>& pixels) FL_NO_EXCEPT override {
        IChannelDriver& driver = DriverTraits::instance();

        // (A1) Respect runtime driver enable/disable: ChannelManager is the
        // single source of truth. A disabled driver means "drop this frame",
        // not "enqueue and hope" -- silently enqueuing to a disabled driver
        // is the #2517 silent-drop failure mode.
        if (!Policy::enabled(driver)) {
            FL_WARN_ONCE("SlimBridgeController: driver '" << driver.getName().c_str() << "' is disabled - dropping frame");
            return;
        }

        // (A8) If the previous frame's buffer is still in flight, block
        // (bounded) until the driver reports it can accept more work rather
        // than racing the driver's in-progress DMA/RMT transfer.
        if (mData->isInUse()) {
            if (!driver.waitForReady()) {
                FL_WARN_ONCE("SlimBridgeController: driver '" << driver.getName().c_str() << "' did not become ready in time - dropping frame");
                return;
            }
        }

        // Give subclasses a chance to observe the accepted frame before it is
        // encoded (e.g. feeding ActiveStripTracker for stub/WASM channels).
        onBeforeEncode(pixels);

        // (A5) Publish the pixel byte layout so the driver (and any
        // downstream padding/diagnostic code) knows the encoded stride.
        if (this->getRgbww().active()) {
            mData->setPixelFormat(ChannelPixelFormat::RGBWW);
        } else if (this->getRgbw().active()) {
            mData->setPixelFormat(ChannelPixelFormat::RGBW);
        } else {
            mData->setPixelFormat(ChannelPixelFormat::RGB);
        }

        // (A3) Re-encode into the same buffer every frame -- clear() keeps
        // capacity, so steady-state operation is allocation-free.
        mData->getData().clear();
        fl::PixelIterator iterator(&pixels, this->getRgbw(), this->getRgbww());
        iterator.writeWS2812(&mData->getData());

        driver.enqueue(mData);
        Policy::afterEnqueue(driver);
    }

    /// Called once per accepted frame, after the enabled/ready gates pass and BEFORE the
    /// frame is encoded and enqueued. Default: no-op. Used by stub/WASM to feed ActiveStripTracker.
    virtual void onBeforeEncode(PixelController<RGB_ORDER>& pixels) FL_NO_EXCEPT { (void)pixels; }

private:
    ChannelDataPtr mData;
};

}  // namespace fl

// IWYU pragma: private

#ifndef __INC_BLOCK_CLOCKLESS_ARM_MXRT1062_H
#define __INC_BLOCK_CLOCKLESS_ARM_MXRT1062_H

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "cpixel_ledcontroller.h"
#include "fl/channels/bus.h"
#include "fl/channels/config.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/chipsets/encoders/pixel_iterator.h"
#include "fl/chipsets/timing_traits.h"
#include "fl/log/log.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/vector.h"
#include "platforms/arm/teensy/teensy4_common/block_lane_pins.h"
#include "platforms/arm/teensy/teensy4_common/block_lane_pins.hpp"
#include "platforms/arm/teensy/teensy4_common/drivers/objectfled/bus_traits.h"

namespace fl {

// Legacy multi-lane "inline block" clockless controller for Teensy 4.x.
//
// Slim bridge (issue #4588): each lane owns one ChannelData that is
// re-encoded every frame and enqueued on the ObjectFLED channel engine
// (`BusTraits<Bus::FLEX_IO, 0>` == ChannelEngineObjectFLED). The lane pins are
// the same GPIO block sequence the historical bit-bang `_BLOCK_PIN` switch
// selected (see block_lane_pins.h); the engine transmits lanes with matching
// timing in parallel.
template <u8 LANES, int FIRST_PIN, typename TIMING, EOrder RGB_ORDER = GRB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class FlexibleInlineBlockClocklessController
    : public CPixelLEDController<RGB_ORDER, LANES, (1 << LANES) - 1> {
    FL_STATIC_ASSERT(LANES >= 1 && LANES <= 16, "LANES must be in [1, 16]");
    FL_STATIC_ASSERT(XTRA0 >= 0 && XTRA0 <= 32, "XTRA0 out of range");

    using Traits = BusTraits<Bus::FLEX_IO, 0>;
    using MultiPixels = PixelController<RGB_ORDER, LANES, (1 << LANES) - 1>;

    u8 mPins[LANES];
    u8 mNActualLanes = 0;
    ChannelDataPtr mData[LANES];

public:
    FlexibleInlineBlockClocklessController() FL_NO_EXCEPT {
        Traits::registerWithManager();
    }

    int size() const FL_NO_EXCEPT override { return CLEDController::size() * mNActualLanes; }

    void init() FL_NO_EXCEPT override {
        mNActualLanes = teensy4BlockLanePins(FIRST_PIN, LANES, mPins);
        ChipsetTimingConfig timing = makeTimingConfig<TIMING>();
        // Fold WAIT_TIME into reset_us, as SlimBridgeController does.
        if (WAIT_TIME > 0 && static_cast<u32>(WAIT_TIME) > timing.reset_us) {
            timing.reset_us = static_cast<u32>(WAIT_TIME);
        }
        for (u8 i = 0; i < mNActualLanes; ++i) {
            mData[i] = ChannelData::create(mPins[i], timing, fl::vector_psram<u8>(),
                                           ChannelPixelFormat::RGB);
            mData[i]->setExtraZeroBitsPerByte(static_cast<u8>(XTRA0));
        }
    }

    u16 getMaxRefreshRate() const FL_NO_EXCEPT override { return 400; }

protected:
    void showPixels(MultiPixels& pixels) FL_NO_EXCEPT override {
        if (mNActualLanes == 0) {
            return;
        }
        IChannelDriver& driver = Traits::instance();
        if (!ChannelManager::registry().isDriverEnabled(driver.getName().c_str())) {
            FL_WARN_ONCE("FlexibleInlineBlockClocklessController: driver '%s' is disabled - dropping frame",
                         driver.getName().c_str());
            return;
        }
        bool inUse = false;
        for (u8 i = 0; i < mNActualLanes; ++i) {
            if (mData[i]->isInUse()) { inUse = true; }
        }
        if (inUse && !driver.waitForReady()) {
            FL_WARN_ONCE("FlexibleInlineBlockClocklessController: driver '%s' did not become ready in time "
                         "- dropping frame", driver.getName().c_str());
            return;
        }

        ChannelPixelFormat format = ChannelPixelFormat::RGB;
        if (this->getRgbww().active()) {
            format = ChannelPixelFormat::RGBWW;
        } else if (this->getRgbw().active()) {
            format = ChannelPixelFormat::RGBW;
        }

        for (u8 lane = 0; lane < mNActualLanes; ++lane) {
            // Single-lane view over this lane's pixels: the multi-lane
            // controller stores lane `i` at mData + mOffsets[i] (initOffsets).
            PixelController<RGB_ORDER> view(pixels.mData + pixels.mOffsets[lane], pixels.mLen,
                                            pixels.mColorAdjustment, DISABLE_DITHER,
                                            pixels.mAdvance != 0, 0);
            view.mAdvance = pixels.mAdvance;
            for (int c = 0; c < 3; ++c) {
                view.d[c] = pixels.d[c];
                view.e[c] = pixels.e[c];
            }

            ChannelDataPtr& data = mData[lane];
            data->setPixelFormat(format);
            data->getData().clear();
            fl::PixelIterator iterator(&view, this->getRgbw(), this->getRgbww());
            iterator.writeWS2812(&data->getData());
            driver.enqueue(data);
        }
    }
};

template<template<u8 DATA_PIN, fl::EOrder RGB_ORDER> class CHIPSET, u8 DATA_PIN, int NUM_LANES, fl::EOrder RGB_ORDER=GRB>
class __FIBCC : public FlexibleInlineBlockClocklessController<NUM_LANES,DATA_PIN,typename CHIPSET<DATA_PIN,RGB_ORDER>::__TIMING,RGB_ORDER,CHIPSET<DATA_PIN,RGB_ORDER>::__XTRA0(),CHIPSET<DATA_PIN,RGB_ORDER>::__FLIP(),CHIPSET<DATA_PIN,RGB_ORDER>::__WAIT_TIME()> {};

#define __FASTLED_HAS_FIBCC 1

}  // namespace fl

#endif  // FL_IS_TEENSY_4X

#endif

#pragma once

// IWYU pragma: private

/// @file clockless_channel_lpc_uart_dma.h
/// @brief LPC845 channels-API ClocklessController adapter for UART DMA.

#include "platforms/arm/is_arm.h"
#include "platforms/arm/lpc/is_lpc.h"

#if defined(FL_IS_ARM_LPC_845) && FASTLED_LPC_UART_DMA

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1
#define FL_CLOCKLESS_LPC_UART_CHANNEL_ENGINE_DEFINED 1

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
#include "platforms/arm/lpc/drivers/uart_dma/bus_traits.h"

namespace fl {

FL_STATIC_ASSERT(DefaultBus<ClocklessChipset>::value == Bus::UART,
                 "FASTLED_LPC_UART_DMA must make LPC845 clockless AUTO resolve to Bus::UART");

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
    void showPixels(PixelController<RGB_ORDER>& pixels) override {
        if (!mDriver) {
            FL_WARN_F_EVERY(100, "LPC UART DMA channel engine unavailable");
            return;
        }
        if (mChannelData->isInUse() && !mDriver->waitForReady()) {
            FL_ERROR_F("LPC UART DMA: engine still busy after wait");
            return;
        }

        fl::PixelIterator iterator = pixels.as_iterator(this->getRgbw());
        auto& data = mChannelData->getData();
        data.clear();
        iterator.writeWS2812(&data);

        mDriver->enqueue(mChannelData);
        mDriver->show();
    }

    static fl::shared_ptr<IChannelDriver> getLpcEngine() FL_NO_EXCEPT {
        return BusTraits<Bus::UART>::instancePtr();
    }
};

template <int DATA_PIN, typename TIMING_LIKE, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
struct ClocklessControllerAdapter
    : public ClocklessController<DATA_PIN, TIMING_LIKE, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {};

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB,
          int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 0>
class ClocklessBlockController
    : public ClocklessController<DATA_PIN, TIMING, RGB_ORDER, XTRA0, FLIP, WAIT_TIME> {};

}  // namespace fl

#endif  // FL_IS_ARM_LPC_845 && FASTLED_LPC_UART_DMA

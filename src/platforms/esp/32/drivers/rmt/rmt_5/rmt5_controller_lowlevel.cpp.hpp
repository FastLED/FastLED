// IWYU pragma: private

// This "low level" code uset to be large, but through the refactor
// to a channel bus manager this has become a thin stub of it's former self
// and is pending deletion.


#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "fl/stl/compiler_control.h"

#include "platforms/esp/32/feature_flags/enabled.h"
#include "fl/stl/cstring.h"

#if FASTLED_RMT5

#include "platforms/esp/32/drivers/rmt/rmt_5/rmt5_controller_lowlevel.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/channels/manager.h"

FL_EXTERN_C_BEGIN

#include "esp_log.h"

FL_EXTERN_C_END

#include "fl/stl/assert.h"
#include "fl/warn.h"
#include "fl/system/log.h"
#include "fl/chipsets/led_timing.h"

#define RMT5_CONTROLLER_TAG "rmt5_controller_lowlevel"

namespace fl {

RmtController5LowLevel::RmtController5LowLevel(
    int pin,
    const ChipsetTiming& timing
)
{
    // Create ChipsetTimingConfig from ChipsetTiming
    ChipsetTimingConfig timingConfig(
        timing.T1,
        timing.T2,
        timing.T3,
        timing.RESET,
        timing.name
    );

    // Create ChannelData for this controller
    mChannelData = ChannelData::create(pin, timingConfig);

    // Get RMT driver from ChannelManager by name
    auto& manager = channelManager();
    mDriver = manager.getDriverByName("RMT");
}

RmtController5LowLevel::~RmtController5LowLevel() {
    // ChannelData is held by shared_ptr and will be cleaned up automatically
    // Engine holds reference until transmission completes
}

void RmtController5LowLevel::loadPixelData(PixelIterator& pixels) {
    // Safety check: don't modify buffer if driver is transmitting it
    if (mChannelData->isInUse()) {
        FL_WARN("RMT5 Controller: Skipping update - buffer in use by driver");
        return;
    }

    // Get the data buffer
    auto& data = mChannelData->getData();
    data.clear();

    // Write pixel data to buffer
    pixels.writeWS2812(&data);
}

void RmtController5LowLevel::showPixels() {
    // Get RMT driver if not already set
    if (!mDriver) {
        auto& manager = channelManager();
        mDriver = manager.getDriverByName("RMT");
    }

    // Enqueue channel data to RMT driver if available
    if (mDriver) {
        mDriver->enqueue(mChannelData);
    } else {
        FL_WARN("RMT5 Controller: RMT driver not available for showPixels()");
    }
}

} // namespace fl

#endif // FASTLED_RMT5

#endif // FL_IS_ESP32

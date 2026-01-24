// This "low level" code uset to be large, but through the refactor
// to a channel bus manager this has become a thin stub of it's former self
// and is pending deletion.


#ifdef ESP32

#include "fl/compiler_control.h"

#include "platforms/esp/32/feature_flags/enabled.h"
#include "fl/stl/cstring.h"

#if FASTLED_RMT5

#include "rmt5_controller_lowlevel.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/channels/bus_manager.h"

FL_EXTERN_C_BEGIN

#include "esp_log.h"

FL_EXTERN_C_END

#include "fl/stl/assert.h"
#include "fl/warn.h"
#include "fl/log.h"
#include "fl/chipsets/led_timing.h"

#define RMT5_CONTROLLER_TAG "rmt5_controller_lowlevel"

namespace fl {

RmtController5LowLevel::RmtController5LowLevel(
    int pin,
    const ChipsetTiming& timing
)
{
    // Get the ChannelBusManager singleton instance (which manages RMT/SPI engines)
    mEngine = &channelBusManager();

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
}

RmtController5LowLevel::~RmtController5LowLevel() {
    // ChannelData is held by shared_ptr and will be cleaned up automatically
    // Engine holds reference until transmission completes
}

void RmtController5LowLevel::loadPixelData(PixelIterator& pixels) {
    // Safety check: don't modify buffer if engine is transmitting it
    if (mChannelData->isInUse()) {
        FL_WARN("RMT5 Controller: Skipping update - buffer in use by engine");
        return;
    }

    // Get the data buffer
    auto& data = mChannelData->getData();
    data.clear();

    // Write pixel data to buffer
    pixels.writeWS2812(&data);
}

void RmtController5LowLevel::showPixels() {
    // Enqueue channel data to engine
    mEngine->enqueue(mChannelData);


}

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32

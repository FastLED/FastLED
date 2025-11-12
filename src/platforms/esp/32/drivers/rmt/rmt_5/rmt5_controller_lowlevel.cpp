#ifdef ESP32

#include "fl/compiler_control.h"

#include "third_party/espressif/led_strip/src/enabled.h"
#include "fl/cstring.h"

#if FASTLED_RMT5

#include "rmt5_controller_lowlevel.h"
#include "channel_engine_rmt.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/singleton.h"

FL_EXTERN_C_BEGIN

#include "esp_log.h"

FL_EXTERN_C_END

#include "ftl/assert.h"
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
    // Get the singleton ChannelEngineRMT instance
    mEngine = &fl::Singleton<ChannelEngineRMT>::instance();

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

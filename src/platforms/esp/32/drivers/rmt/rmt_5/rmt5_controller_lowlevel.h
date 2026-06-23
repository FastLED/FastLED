#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"
#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "pixel_iterator.h"
#include "fl/stl/stdint.h"
#include "fl/chipsets/led_timing.h"
#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/noexcept.h"

FL_EXTERN_C_BEGIN

// IWYU pragma: begin_keep
#include "driver/gpio.h"

// IWYU pragma: end_keep
FL_EXTERN_C_END

namespace fl {

/**
 * RmtController5LowLevel - Lightweight FastLED controller using IChannelDriver
 *
 * Architecture:
 * - Creates ChannelData for transmission via IChannelDriver
 * - Uses ChannelEngineRMT for actual RMT worker management
 * - Integrates with FastLED via standard controller interface
 *
 * Lifecycle:
 * 1. Constructor: Create ChannelData
 * 2. loadPixelData(): Copy pixel data to ChannelData buffer
 * 3. showPixels(): Enqueue ChannelData to driver and trigger transmission
 *
 * Memory Model:
 * - Controller owns ChannelData (persistent)
 * - IChannelDriver manages RMT workers (temporary)
 */
class RmtController5LowLevel {
public:
    // Constructor
    RmtController5LowLevel(
        int data_pin,
        const ChipsetTiming& timing
    ) FL_NOEXCEPT;

    ~RmtController5LowLevel();

    // FastLED interface
    void loadPixelData(PixelIterator& pixels) FL_NOEXCEPT;
    void showPixels() FL_NOEXCEPT;

private:
    // Channel data for transmission
    ChannelDataPtr mChannelData;

    // Channel driver for RMT transmission
    fl::shared_ptr<IChannelDriver> mDriver;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // FL_IS_ESP32

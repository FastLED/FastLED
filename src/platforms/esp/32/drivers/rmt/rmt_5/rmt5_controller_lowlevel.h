#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_RMT5

#include "pixel_iterator.h"
#include "fl/stl/stdint.h"
#include "fl/chipsets/led_timing.h"
#include "fl/channels/data.h"
#include "fl/channels/engine.h"

FL_EXTERN_C_BEGIN

#include "driver/gpio.h"

FL_EXTERN_C_END

namespace fl {

/**
 * RmtController5LowLevel - Lightweight FastLED controller using IChannelEngine
 *
 * Architecture:
 * - Creates ChannelData for transmission via IChannelEngine
 * - Uses ChannelEngineRMT for actual RMT worker management
 * - Integrates with FastLED via standard controller interface
 *
 * Lifecycle:
 * 1. Constructor: Create ChannelData
 * 2. loadPixelData(): Copy pixel data to ChannelData buffer
 * 3. showPixels(): Enqueue ChannelData to engine and trigger transmission
 *
 * Memory Model:
 * - Controller owns ChannelData (persistent)
 * - IChannelEngine manages RMT workers (temporary)
 */
class RmtController5LowLevel {
public:
    // Constructor
    RmtController5LowLevel(
        int data_pin,
        const ChipsetTiming& timing
    );

    ~RmtController5LowLevel();

    // FastLED interface
    void loadPixelData(PixelIterator& pixels);
    void showPixels();

private:
    // Channel data for transmission
    ChannelDataPtr mChannelData;

    // Channel engine for RMT transmission
    IChannelEngine* mEngine;
};

} // namespace fl

#endif // FASTLED_RMT5

#endif // ESP32

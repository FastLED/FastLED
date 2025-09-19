#pragma once

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "pixel_iterator.h"
#include "fl/stdint.h"
#include "fl/namespace.h"

namespace fl {

class IRmtStrip;

// NOTE: LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS controls the memory block size.
// See codebase.
class RmtController5
{
public:
    enum DmaMode {
        DMA_AUTO,
        DMA_ENABLED,
        DMA_DISABLED
    };

    // Bridge between FastLED and the ESP RMT5 driver.
    RmtController5() = delete;
    RmtController5(RmtController5 &&) = delete;
    RmtController5 &operator=(const RmtController5 &) = delete;
    RmtController5(const RmtController5 &) = delete;
    RmtController5(
        int DATA_PIN,
        int T1, int T2, int T3,
        DmaMode dma_mode);  // FastLED bit timings. See embedded python script in chipsets.h for how to calculate these. 

    ~RmtController5();

    void loadPixelData(PixelIterator &pixels);
    void showPixels();

private:
    int mPin;
    int mT1, mT2, mT3;
    IRmtStrip *mLedStrip = nullptr;
    DmaMode mDmaMode;
};

} // namespace fl



#endif  // FASTLED_RMT5

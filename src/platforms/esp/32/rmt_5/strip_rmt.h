#pragma once

#include "fl/stdint.h"
#include "fl/int.h"
#include "fl/namespace.h"

namespace fl {

// NOTE: LED_STRIP_RMT_DEFAULT_MEM_BLOCK_SYMBOLS controls the memory block size.
// See codebase.
class IRmtStrip
{
public:
    enum DmaMode {
        DMA_AUTO,  // Use DMA if available, otherwise use RMT.
        DMA_ENABLED,
        DMA_DISABLED,
    };

    static IRmtStrip* Create(
        int pin, uint32_t led_count, bool is_rgbw,
        uint32_t th0, uint32_t tl0, uint32_t th1, uint32_t tl1, uint32_t reset,
        DmaMode dma_config = DMA_AUTO, uint8_t interrupt_priority = 3, 
        uint8_t* external_pixel_buf = nullptr);

    virtual ~IRmtStrip() {}
    virtual void setPixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue) = 0;
    virtual void setPixelRGBW(uint32_t index, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) = 0;
    virtual void drawSync()
    {
        drawAsync();
        waitDone();
    }
    virtual void drawAsync() = 0;
    virtual void waitDone() = 0;
    virtual bool isDrawing() = 0;
    virtual void fill(uint8_t red, uint8_t green, uint8_t blue) = 0;
    virtual void fillRGBW(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) = 0;
    virtual fl::u32 numPixels() = 0;
};

} // namespace fl

#pragma once

#include <stdint.h>

class ISpiStripWs2812
{
public:
    enum SpiHostMode {
        SPI_HOST_MODE_AUTO,  // Binds to SPI_HOST_MODE_2, then SPI_HOST_MODE_3 (if available), then SPI_HOST_MODE_1.
        SPI_HOST_MODE_1,
        SPI_HOST_MODE_2,
        SPI_HOST_MODE_3,  // Not supported on all chipsets.
    };

    enum DmaMode {
        DMA_AUTO,  // Use DMA if available, otherwise use RMT.
        DMA_ENABLED,
        DMA_DISABLED,
    };

    static ISpiStripWs2812* Create(int pin, uint32_t led_count, SpiHostMode spi_bus = SPI_HOST_MODE_AUTO, DmaMode dma_mode = DMA_AUTO);
    virtual ~ISpiStripWs2812() {}
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
};

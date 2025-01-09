#pragma once

#include <stdint.h>

class ISpiStripWs2812 {
  public:
    enum SpiHostMode {
        SPI_HOST_MODE_AUTO, // Binds to SPI_HOST_MODE_2, then SPI_HOST_MODE_3
                            // (if available), then SPI_HOST_MODE_1.
        SPI_HOST_MODE_1,
        SPI_HOST_MODE_2,
        SPI_HOST_MODE_3, // Not supported on all chipsets.
    };

    enum DmaMode {
        DMA_AUTO, // Use DMA if available, otherwise use RMT.
        DMA_ENABLED,
        DMA_DISABLED,
    };

    class OutputIterator {
      public:
        OutputIterator(ISpiStripWs2812 *strip, uint32_t num_leds);
        OutputIterator(OutputIterator &) = default;
        OutputIterator(OutputIterator &&) = default;
        ~OutputIterator();

        void flush();
        void operator()(uint8_t value);
        void finish();  // Must call this at the end.

        uint32_t mPosition = 0;
        uint32_t mWritten = 0; // whenever this hits 3, we flush.
        uint8_t mRed = 0;
        uint8_t mGreen = 0;
        uint8_t mBlue = 0;
        ISpiStripWs2812 *mStrip;
        uint32_t mNumLeds;
    };

    static ISpiStripWs2812 *Create(int pin, uint32_t led_count, bool is_rgbw,
                                   SpiHostMode spi_bus = SPI_HOST_MODE_AUTO,
                                   DmaMode dma_mode = DMA_AUTO);
    virtual ~ISpiStripWs2812() {}
    virtual void drawSync() {
        drawAsync();
        waitDone();
    }
    virtual void drawAsync() = 0;
    virtual void waitDone() = 0;
    virtual bool isDrawing() = 0;

    virtual void fill(uint8_t red, uint8_t green, uint8_t blue) = 0;
    virtual uint32_t numPixels() = 0;

    // Useful for iterating over the LEDs in a strip, especially RGBW mode which the spi
    // api does not support natively.
    virtual OutputIterator outputIterator() = 0;

protected:
    // Don't use this Use outputIterator() instead.
    virtual void setPixel(uint32_t index, uint8_t red, uint8_t green,
                          uint8_t blue) = 0;
};

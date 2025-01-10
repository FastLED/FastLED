
#ifdef ESP32

#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI


/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "third_party/espressif/led_strip/src/led_strip.h"
#include "esp_log.h"
#include "esp_err.h"

#include "strip_spi.h"

#include "rgbw.h"
#include "fl/warn.h"


static const char *TAG = "strip_spi";

led_strip_handle_t configure_led(int pin, uint32_t led_count, led_model_t led_model, spi_host_device_t spi_bus, bool with_dma)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = pin, // The GPIO that connected to the LED strip's data line
        .max_leds = led_count,      // The number of LEDs in the strip,
        .led_model = led_model,        // LED strip model
        // set the color order of the strip: GRB
        .color_component_format = {
            .format = {
                .r_pos = 0, // red is the second byte in the color data
                .g_pos = 1, // green is the first byte in the color data
                .b_pos = 2, // blue is the third byte in the color data
                .num_components = 3, // total 3 color components
            },
        },
        .flags = {
            .invert_out = false, // don't invert the output signal
        }
    };

    // LED strip backend configuration: SPI
    led_strip_spi_config_t spi_config = {
        .clk_src = SPI_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
        .spi_bus = spi_bus,           // SPI bus ID
        .flags = {
            .with_dma = with_dma, // Using DMA can improve performance and help drive more LEDs
        }
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with SPI backend");
    return led_strip;
}

struct SpiHostUsed {
    spi_host_device_t spi_host;
    bool used;
};

static SpiHostUsed gSpiHostUsed[] = {
    {SPI2_HOST, false},  // in order of preference
#if SOC_SPI_PERIPH_NUM > 2
    {SPI3_HOST, false},
#endif
    {SPI1_HOST, false},
};

static spi_host_device_t getNextAvailableSpiHost()
{
    for (int i = 0; i < sizeof(gSpiHostUsed) / sizeof(gSpiHostUsed[0]); i++)
    {
        if (!gSpiHostUsed[i].used)
        {
            gSpiHostUsed[i].used = true;
            return gSpiHostUsed[i].spi_host;
        }
    }
    ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
    return SPI_HOST_MAX;
}


static void releaseSpiHost(spi_host_device_t spi_host)
{
    for (int i = 0; i < sizeof(gSpiHostUsed) / sizeof(gSpiHostUsed[0]); i++)
    {
        if (gSpiHostUsed[i].spi_host == spi_host)
        {
            gSpiHostUsed[i].used = false;
            return;
        }
    }
    ESP_ERROR_CHECK(ESP_ERR_NOT_FOUND);
}


class SpiStripWs2812 : public ISpiStripWs2812 {
public:
    SpiStripWs2812(int pin, uint32_t led_count, ISpiStripWs2812::SpiHostMode spi_bus_mode, ISpiStripWs2812::DmaMode dma_mode = DMA_AUTO)
        : mIsRgbw(false), // SPI implementation currently only supports RGB
          mLedCount(led_count)
    {
        switch (spi_bus_mode) {
            case ISpiStripWs2812::SPI_HOST_MODE_AUTO:
                mSpiHost = getNextAvailableSpiHost();
                break;
            case ISpiStripWs2812::SPI_HOST_MODE_1:
                mSpiHost = SPI1_HOST;
                break;
            case ISpiStripWs2812::SPI_HOST_MODE_2:
                mSpiHost = SPI2_HOST;
                break;
#if SOC_SPI_PERIPH_NUM > 2
            case ISpiStripWs2812::SPI_HOST_MODE_3:
                mSpiHost = SPI3_HOST;
                break;
#endif
            default:
                ESP_ERROR_CHECK(ESP_ERR_NOT_SUPPORTED);
        }

        bool with_dma = dma_mode == ISpiStripWs2812::DMA_ENABLED || dma_mode == ISpiStripWs2812::DMA_AUTO;
        led_strip_handle_t led_strip = configure_led(pin, led_count, LED_MODEL_WS2812, mSpiHost, with_dma);
        mStrip = led_strip;
    }

    ~SpiStripWs2812() override
    {
        waitDone();
        led_strip_del(mStrip);
        releaseSpiHost(mSpiHost);
        mStrip = nullptr;
    }

    void setPixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue) override
    {
        ESP_ERROR_CHECK(led_strip_set_pixel(mStrip, index, red, green, blue));
    }

    void drawAsync() override
    {
        if (mDrawIssued)
        {
            waitDone();
        }
        ESP_ERROR_CHECK(led_strip_refresh_async(mStrip));
        mDrawIssued = true;
    }

    void waitDone() override
    {
        if (!mDrawIssued)
        {
            return;
        }
        ESP_ERROR_CHECK(led_strip_refresh_wait_done(mStrip));
        mDrawIssued = false;
    }

    bool isDrawing() override
    {
        return mDrawIssued;
    }

    void fill(uint8_t red, uint8_t green, uint8_t blue) {
        for (int i = 0; i < mLedCount; i++)
        {
            setPixel(i, red, green, blue);
        }
    }

    void clear()
    {
        ESP_ERROR_CHECK(led_strip_clear(mStrip));
    }

    void fill_color(uint8_t red, uint8_t green, uint8_t blue)
    {
        for (int i = 0; i < mLedCount; i++)
        {
            setPixel(i, red, green, blue);
        }
    }

    OutputIterator outputIterator() override
    {
        return OutputIterator(this, mLedCount);
    }

    uint32_t numPixels() override
    {
        return mLedCount;
    }

private:
    spi_host_device_t mSpiHost = SPI2_HOST;
    led_strip_handle_t mStrip;
    bool mDrawIssued = false;
    bool mIsRgbw;
    uint32_t mLedCount = 0;
};



ISpiStripWs2812::OutputIterator::OutputIterator(ISpiStripWs2812 *strip, uint32_t num_leds)
    : mStrip(strip), mNumLeds(num_leds) {}

ISpiStripWs2812::OutputIterator::~OutputIterator() {
    if (mWritten) {
        FASTLED_WARN("finish() was not called on OutputIterator before destruction.");
        finish();
    }
}

void ISpiStripWs2812::OutputIterator::flush() {
    mStrip->setPixel(mPosition, mRed, mGreen, mBlue);
    mRed = mGreen = mBlue = 0;
}

void ISpiStripWs2812::OutputIterator::operator()(uint8_t value) {
    switch (mWritten) {
    case 0:
        mRed = value;
        break;
    case 1:
        mGreen = value;
        break;
    case 2:
        mBlue = value;
        break;
    }
    mWritten++;
    if (mWritten == 3) {
        flush();
        mWritten = 0;
        mPosition++;
    }
}

void ISpiStripWs2812::OutputIterator::finish() {
    if (mWritten) {
        flush();
    }
}

ISpiStripWs2812* ISpiStripWs2812::Create(int pin, uint32_t led_count, bool is_rgbw, ISpiStripWs2812::SpiHostMode spi_bus, ISpiStripWs2812::DmaMode dma_mode) {
    if (!is_rgbw) {
        return new SpiStripWs2812(pin, led_count, spi_bus, dma_mode);
    }
    // Emulate RGBW mode by pretending the RGBW pixels are RGB pixels.
    uint32_t size_as_rgb = Rgbw::size_as_rgb(led_count);
    return new SpiStripWs2812(pin, size_as_rgb, spi_bus, dma_mode);
}

#endif  // FASTLED_ESP32_HAS_CLOCKLESS_SPI

#endif  // ESP32


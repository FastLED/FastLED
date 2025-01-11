#ifdef ESP32

#include <stdint.h>
#include "third_party/espressif/led_strip/src/enabled.h"

#if FASTLED_RMT5

#include "strip_rmt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "third_party/espressif/led_strip/src/led_strip.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"

namespace {  // anonymous namespace

static const char *TAG = "strip_rmt";

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

led_strip_handle_t configure_led_with_timings(int pin, uint32_t led_count, bool is_rgbw, uint32_t t0h, uint32_t t0l, uint32_t t1h, uint32_t t1l, uint32_t reset, bool with_dma, uint8_t interrupt_priority)
{
    led_strip_encoder_timings_t timings = {
        .t0h = t0h,
        .t1h = t1h,
        .t0l = t0l,
        .t1l = t1l,
        .reset = reset};

    // is always going to fail, so it's disabled for now.
    uint32_t memory_block_symbols = with_dma ? 1024 : 0;
    led_color_component_format_t color_component_format =
        is_rgbw ? LED_STRIP_COLOR_COMPONENT_FMT_RGBW : LED_STRIP_COLOR_COMPONENT_FMT_RGB;

    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = pin,                            // The GPIO that connected to the LED strip's data line
        .max_leds = led_count,                            // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,                    // LED strip model
        .color_component_format = color_component_format, // The color order of the strip: GRB
        .flags = {
            .invert_out = false, // don't invert the output signal
        },
        .timings = timings
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,            // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ,     // RMT counter clock frequency
        .mem_block_symbols = memory_block_symbols, // the memory size of each RMT channel, in words (4 bytes)
        .flags = {
            .with_dma = false, // DMA feature is available on chips like ESP32-S3/P4
        },
        .interrupt_priority = interrupt_priority, // RMT interrupt priority
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}


class RmtStrip : public IRmtStrip
{
public:
    RmtStrip(int pin, uint32_t led_count, bool is_rgbw, uint32_t th0, uint32_t tl0, uint32_t th1, uint32_t tl1, uint32_t reset, IRmtStrip::DmaMode dma_mode, uint8_t interrupt_priority)
        : mIsRgbw(is_rgbw), mLedCount(led_count)
    {
        bool with_dma = dma_mode == IRmtStrip::DMA_ENABLED;
        led_strip_handle_t led_strip = configure_led_with_timings(pin, led_count, is_rgbw, th0, tl0, th1, tl1, reset, with_dma, interrupt_priority);
        mStrip = led_strip;
    }

    ~RmtStrip() override
    {
        waitDone();
        led_strip_del(mStrip);
        mStrip = nullptr;
    }

    void setPixel(uint32_t index, uint8_t red, uint8_t green, uint8_t blue) override
    {
        ESP_ERROR_CHECK(mIsRgbw ? ESP_ERR_INVALID_ARG : ESP_OK);
        ESP_ERROR_CHECK(led_strip_set_pixel(mStrip, index, red, green, blue));
    }

    void setPixelRGBW(uint32_t index, uint8_t red, uint8_t green, uint8_t blue, uint8_t white) override
    {
        ESP_ERROR_CHECK(mIsRgbw ? ESP_OK : ESP_ERR_INVALID_ARG);
        ESP_ERROR_CHECK(led_strip_set_pixel_rgbw(mStrip, index, red, green, blue, white));
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
            // ESP_LOGE(TAG, "No draw issued, skipping wait");
            return;
        }
        ESP_ERROR_CHECK(led_strip_refresh_wait_done(mStrip));
        mDrawIssued = false;
    }

    bool isDrawing() override
    {
        return mDrawIssued;
    }

    void clear()
    {
        ESP_ERROR_CHECK(led_strip_clear(mStrip));
    }

    void fill(uint8_t red, uint8_t green, uint8_t blue) override {
        for (int i = 0; i < mLedCount; i++)
        {
            setPixel(i, red, green, blue);
        }
    }

    void fillRGBW(uint8_t red, uint8_t green, uint8_t blue, uint8_t white) override {
        for (int i = 0; i < mLedCount; i++)
        {
            setPixelRGBW(i, red, green, blue, white);
        }
    }

    uint32_t numPixels() override
    {
        return mLedCount;
    }

private:
    led_strip_handle_t mStrip;
    bool mDrawIssued = false;
    bool mIsRgbw;
    uint32_t mLedCount;
};

}  // namespace



IRmtStrip *IRmtStrip::Create(
    int pin, uint32_t led_count, bool is_rgbw,
    uint32_t th0, uint32_t tl0, uint32_t th1, uint32_t tl1, uint32_t reset,
    DmaMode dma_config, uint8_t interrupt_priority)
{
    return new RmtStrip(
        pin, led_count, is_rgbw,
        th0, tl0, th1, tl1, reset,
        dma_config, interrupt_priority
    );
}


#endif  // FASTLED_RMT5

#endif  // ESP32

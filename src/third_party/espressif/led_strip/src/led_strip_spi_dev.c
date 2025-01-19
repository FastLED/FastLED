#ifdef ESP32

#include "enabled.h"

#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include <string.h>
#include <sys/cdefs.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_rom_gpio.h"
#include "soc/spi_periph.h"
#include "led_strip.h"
#include "led_strip_interface.h"

#define LED_STRIP_SPI_DEFAULT_RESOLUTION (2.5 * 1000 * 1000) // 2.5MHz resolution
#define LED_STRIP_SPI_DEFAULT_TRANS_QUEUE_SIZE 4

#define SPI_BYTES_PER_COLOR_BYTE 3
#define SPI_BITS_PER_COLOR_BYTE (SPI_BYTES_PER_COLOR_BYTE * 8)

static const char *TAG = "led_strip_spi";

typedef struct {
    led_strip_t base;
    spi_host_device_t spi_host;
    spi_device_handle_t spi_device;
    uint32_t strip_len;
    uint8_t bytes_per_pixel;
    led_color_component_format_t component_fmt;
    uint8_t pixel_buf[];
} led_strip_spi_obj;

// please make sure to zero-initialize the buf before calling this function
static void __led_strip_spi_bit(uint8_t data, uint8_t *buf)
{
    // Each color of 1 bit is represented by 3 bits of SPI, low_level:100 ,high_level:110
    // So a color byte occupies 3 bytes of SPI.
    *(buf + 2) |= data & BIT(0) ? BIT(2) | BIT(1) : BIT(2);
    *(buf + 2) |= data & BIT(1) ? BIT(5) | BIT(4) : BIT(5);
    *(buf + 2) |= data & BIT(2) ? BIT(7) : 0x00;
    *(buf + 1) |= BIT(0);
    *(buf + 1) |= data & BIT(3) ? BIT(3) | BIT(2) : BIT(3);
    *(buf + 1) |= data & BIT(4) ? BIT(6) | BIT(5) : BIT(6);
    *(buf + 0) |= data & BIT(5) ? BIT(1) | BIT(0) : BIT(1);
    *(buf + 0) |= data & BIT(6) ? BIT(4) | BIT(3) : BIT(4);
    *(buf + 0) |= data & BIT(7) ? BIT(7) | BIT(6) : BIT(7);
}

static esp_err_t led_strip_spi_set_pixel(led_strip_t *strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue)
{
    led_strip_spi_obj *spi_strip = __containerof(strip, led_strip_spi_obj, base);
    ESP_RETURN_ON_FALSE(index < spi_strip->strip_len, ESP_ERR_INVALID_ARG, TAG, "index out of maximum number of LEDs");
    // 3 pixels take 72bits(9bytes)
    uint32_t start = index * spi_strip->bytes_per_pixel * SPI_BYTES_PER_COLOR_BYTE;
    uint8_t *pixel_buf = spi_strip->pixel_buf;
    led_color_component_format_t component_fmt = spi_strip->component_fmt;
    memset(pixel_buf + start, 0, spi_strip->bytes_per_pixel * SPI_BYTES_PER_COLOR_BYTE);

    __led_strip_spi_bit(red, &pixel_buf[start + SPI_BYTES_PER_COLOR_BYTE * component_fmt.format.r_pos]);
    __led_strip_spi_bit(green, &pixel_buf[start + SPI_BYTES_PER_COLOR_BYTE * component_fmt.format.g_pos]);
    __led_strip_spi_bit(blue, &pixel_buf[start + SPI_BYTES_PER_COLOR_BYTE * component_fmt.format.b_pos]);
    if (component_fmt.format.num_components > 3) {
        __led_strip_spi_bit(0, &pixel_buf[start + SPI_BYTES_PER_COLOR_BYTE * component_fmt.format.w_pos]);
    }

    return ESP_OK;
}

static esp_err_t led_strip_spi_set_pixel_rgbw(led_strip_t *strip, uint32_t index, uint32_t red, uint32_t green, uint32_t blue, uint32_t white)
{
    led_strip_spi_obj *spi_strip = __containerof(strip, led_strip_spi_obj, base);
    led_color_component_format_t component_fmt = spi_strip->component_fmt;
    ESP_RETURN_ON_FALSE(index < spi_strip->strip_len, ESP_ERR_INVALID_ARG, TAG, "index out of maximum number of LEDs");
    ESP_RETURN_ON_FALSE(component_fmt.format.num_components == 4, ESP_ERR_INVALID_ARG, TAG, "led doesn't have 4 components");

    // LED_PIXEL_FORMAT_GRBW takes 96bits(12bytes)
    uint32_t start = index * spi_strip->bytes_per_pixel * SPI_BYTES_PER_COLOR_BYTE;
    uint8_t *pixel_buf = spi_strip->pixel_buf;
    memset(pixel_buf + start, 0, spi_strip->bytes_per_pixel * SPI_BYTES_PER_COLOR_BYTE);

    __led_strip_spi_bit(red, &pixel_buf[start + SPI_BYTES_PER_COLOR_BYTE * component_fmt.format.r_pos]);
    __led_strip_spi_bit(green, &pixel_buf[start + SPI_BYTES_PER_COLOR_BYTE * component_fmt.format.g_pos]);
    __led_strip_spi_bit(blue, &pixel_buf[start + SPI_BYTES_PER_COLOR_BYTE * component_fmt.format.b_pos]);
    __led_strip_spi_bit(white, &pixel_buf[start + SPI_BYTES_PER_COLOR_BYTE * component_fmt.format.w_pos]);

    return ESP_OK;
}


static esp_err_t spi_led_strip_refresh_async(led_strip_t *strip)
{
    led_strip_spi_obj *spi_strip = __containerof(strip, led_strip_spi_obj, base);
    spi_transaction_t tx_conf;
    memset(&tx_conf, 0, sizeof(tx_conf));

    tx_conf.length = spi_strip->strip_len * spi_strip->bytes_per_pixel * SPI_BITS_PER_COLOR_BYTE;
    tx_conf.tx_buffer = spi_strip->pixel_buf;
    tx_conf.rx_buffer = NULL;
    spi_device_queue_trans(spi_strip->spi_device, &tx_conf, portMAX_DELAY);
    return ESP_OK;
}

static esp_err_t spi_led_strip_refresh_wait_done(led_strip_t *strip)
{
    led_strip_spi_obj *spi_strip = __containerof(strip, led_strip_spi_obj, base);
    spi_transaction_t* tx_conf = 0;
    spi_device_get_trans_result(spi_strip->spi_device, &tx_conf, portMAX_DELAY);
    return ESP_OK;
}

static esp_err_t led_strip_spi_refresh(led_strip_t *strip)
{
    ESP_RETURN_ON_ERROR(spi_led_strip_refresh_async(strip), TAG, "refresh async failed");
    ESP_RETURN_ON_ERROR(spi_led_strip_refresh_wait_done(strip), TAG, "wait for done failed");
    return ESP_OK;
}


static esp_err_t led_strip_spi_clear(led_strip_t *strip)
{
    led_strip_spi_obj *spi_strip = __containerof(strip, led_strip_spi_obj, base);
    //Write zero to turn off all leds
    memset(spi_strip->pixel_buf, 0, spi_strip->strip_len * spi_strip->bytes_per_pixel * SPI_BYTES_PER_COLOR_BYTE);
    uint8_t *buf = spi_strip->pixel_buf;
    for (int index = 0; index < spi_strip->strip_len * spi_strip->bytes_per_pixel; index++) {
        __led_strip_spi_bit(0, buf);
        buf += SPI_BYTES_PER_COLOR_BYTE;
    }

    return led_strip_spi_refresh(strip);
}

static esp_err_t led_strip_spi_del(led_strip_t *strip)
{
    led_strip_spi_obj *spi_strip = __containerof(strip, led_strip_spi_obj, base);

    ESP_RETURN_ON_ERROR(spi_bus_remove_device(spi_strip->spi_device), TAG, "delete spi device failed");
    ESP_RETURN_ON_ERROR(spi_bus_free(spi_strip->spi_host), TAG, "free spi bus failed");

    free(spi_strip);
    return ESP_OK;
}

esp_err_t led_strip_new_spi_device(const led_strip_config_t *led_config, const led_strip_spi_config_t *spi_config, led_strip_handle_t *ret_strip)
{
    led_strip_spi_obj *spi_strip = NULL;
    esp_err_t ret = ESP_OK;
    ESP_GOTO_ON_FALSE(led_config && spi_config && ret_strip, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    led_color_component_format_t component_fmt = led_config->color_component_format;
    // If R/G/B order is not specified, set default GRB order as fallback
    if (component_fmt.format_id == 0) {
        component_fmt = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
    }
    // check the validation of the color component format
    uint8_t mask = 0;
    if (component_fmt.format.num_components == 3) {
        mask = BIT(component_fmt.format.r_pos) | BIT(component_fmt.format.g_pos) | BIT(component_fmt.format.b_pos);
        // Check for invalid values
        ESP_RETURN_ON_FALSE(mask == 0x07, ESP_ERR_INVALID_ARG, TAG, "invalid order argument");
    } else if (component_fmt.format.num_components == 4) {
        mask = BIT(component_fmt.format.r_pos) | BIT(component_fmt.format.g_pos) | BIT(component_fmt.format.b_pos) | BIT(component_fmt.format.w_pos);
        // Check for invalid values
        ESP_RETURN_ON_FALSE(mask == 0x0F, ESP_ERR_INVALID_ARG, TAG, "invalid order argument");
    } else {
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG, "invalid number of color components: %d", component_fmt.format.num_components);
    }
    // TODO: we assume each color component is 8 bits, may need to support other configurations in the future, e.g. 10bits per color component?
    uint8_t bytes_per_pixel = component_fmt.format.num_components;
    uint32_t mem_caps = MALLOC_CAP_DEFAULT;
    if (spi_config->flags.with_dma) {
        // DMA buffer must be placed in internal SRAM
        mem_caps |= MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA;
    }
    spi_strip = heap_caps_calloc(1, sizeof(led_strip_spi_obj) + led_config->max_leds * bytes_per_pixel * SPI_BYTES_PER_COLOR_BYTE, mem_caps);

    ESP_GOTO_ON_FALSE(spi_strip, ESP_ERR_NO_MEM, err, TAG, "no mem for spi strip");

    spi_strip->spi_host = spi_config->spi_bus;
    // for backward compatibility, if the user does not set the clk_src, use the default value
    spi_clock_source_t clk_src = SPI_CLK_SRC_DEFAULT;
    if (spi_config->clk_src) {
        clk_src = spi_config->clk_src;
    }

    spi_bus_config_t spi_bus_cfg = {
        .mosi_io_num = led_config->strip_gpio_num,
        //Only use MOSI to generate the signal, set -1 when other pins are not used.
        .miso_io_num = -1,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = led_config->max_leds * bytes_per_pixel * SPI_BYTES_PER_COLOR_BYTE,
    };
    ESP_GOTO_ON_ERROR(spi_bus_initialize(spi_strip->spi_host, &spi_bus_cfg, spi_config->flags.with_dma ? SPI_DMA_CH_AUTO : SPI_DMA_DISABLED), err, TAG, "create SPI bus failed");

    if (led_config->flags.invert_out == true) {
        esp_rom_gpio_connect_out_signal(led_config->strip_gpio_num, spi_periph_signal[spi_strip->spi_host].spid_out, true, false);
    }

    spi_device_interface_config_t spi_dev_cfg = {
        .clock_source = clk_src,
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = LED_STRIP_SPI_DEFAULT_RESOLUTION,
        .mode = 0,
        //set -1 when CS is not used
        .spics_io_num = -1,
        .queue_size = LED_STRIP_SPI_DEFAULT_TRANS_QUEUE_SIZE,
    };

    ESP_GOTO_ON_ERROR(spi_bus_add_device(spi_strip->spi_host, &spi_dev_cfg, &spi_strip->spi_device), err, TAG, "Failed to add spi device");
    //ensure the reset time is enough
    esp_rom_delay_us(10);
    int clock_resolution_khz = 0;
    spi_device_get_actual_freq(spi_strip->spi_device, &clock_resolution_khz);
    // TODO: ideally we should decide the SPI_BYTES_PER_COLOR_BYTE by the real clock resolution
    // But now, let's fixed the resolution, the downside is, we don't support a clock source whose frequency is not multiple of LED_STRIP_SPI_DEFAULT_RESOLUTION
    // clock_resolution between 2.2MHz to 2.8MHz is supported
    ESP_GOTO_ON_FALSE((clock_resolution_khz < LED_STRIP_SPI_DEFAULT_RESOLUTION / 1000 + 300) && (clock_resolution_khz > LED_STRIP_SPI_DEFAULT_RESOLUTION / 1000 - 300), ESP_ERR_NOT_SUPPORTED, err,
                      TAG, "unsupported clock resolution:%dKHz", clock_resolution_khz);

    spi_strip->component_fmt = component_fmt;
    spi_strip->bytes_per_pixel = bytes_per_pixel;
    spi_strip->strip_len = led_config->max_leds;
    spi_strip->base.set_pixel = led_strip_spi_set_pixel;
    spi_strip->base.set_pixel_rgbw = led_strip_spi_set_pixel_rgbw;
    spi_strip->base.refresh = led_strip_spi_refresh;
    spi_strip->base.refresh_async = spi_led_strip_refresh_async;
    spi_strip->base.refresh_wait_done = spi_led_strip_refresh_wait_done;
    spi_strip->base.clear = led_strip_spi_clear;
    spi_strip->base.del = led_strip_spi_del;

    *ret_strip = &spi_strip->base;
    return ESP_OK;
err:
    if (spi_strip) {
        if (spi_strip->spi_device) {
            spi_bus_remove_device(spi_strip->spi_device);
        }
        if (spi_strip->spi_host) {
            spi_bus_free(spi_strip->spi_host);
        }
        free(spi_strip);
    }
    return ret;
}


#endif  // FASTLED_ESP_HAS_CLOCKLESS_SPI

#endif  // ESP32

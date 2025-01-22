
#if !__has_include("esp_memory_utils.h")
#error                                                                         \
    "esp_memory_utils.h is not available, are you using esp-idf 4.4 or earlier?"
#else

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#pragma GCC diagnostic ignored "-Wvolatile"

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_intr_alloc.h"
#include "esp_memory_utils.h"
#include "esp_pm.h"
#include <stdint.h>
#include <stdio.h> // ok include
#include <string.h>
// #include "esp_lcd_panel_io_interface.h"
// #include "esp_lcd_panel_io.h"
#include "driver/gpio.h"
#include "esp_memory_utils.h"
#include "esp_private/gdma.h"
#include "esp_rom_gpio.h"
#include "hal/dma_types.h"
#include "hal/gpio_hal.h"
#include "soc/rtc.h" // for `rtc_clk_xtal_freq_get()`
#include "soc/soc_caps.h"
// #include "esp_private/periph_ctrl.h"

// #include "esp_lcd_common.h"
#include "hal/lcd_hal.h"
#include "hal/lcd_ll.h"
#include "soc/lcd_periph.h"

#include "esp_heap_caps.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_io_interface.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "soc/gdma_reg.h"
#include "platforms/esp/esp_version.h"

#define IDF_5_3_OR_EARLIER (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 4, 0))

// According to bug reports, this driver does not work well with the new WS2812-v5b. This is
// probably due to the extrrra long reset time requirements of this chipset. so we put in
// a hack that will always add 300 uS to the reset time.
#define FASTLED_EXPERIMENTAL_YVES_EXTRA_WAIT_MICROS 300

#ifndef NUMSTRIPS
#define NUMSTRIPS 16
#endif

#ifndef SNAKEPATTERN
#define SNAKEPATTERN 1
#endif

#ifndef ALTERNATEPATTERN
#define ALTERNATEPATTERN 1
#endif

#define I2S_DEVICE 0

#define AA (0x00AA00AAL)
#define CC (0x0000CCCCL)
#define FF (0xF0F0F0F0L)
#define FF2 (0x0F0F0F0FL)

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define __OFFSET 0 //  (24*3*2*2*2+2)

#define __OFFSET_END (24 * 3 * 2 * 2 * 2 + 2)
#ifndef HARDWARESPRITES
#define HARDWARESPRITES 0
#endif

#if HARDWARESPRITES == 1
#include "hardwareSprite.h"
#endif

#ifdef COLOR_ORDER_GRBW
#define _p_r 1
#define _p_g 0
#define _p_b 2
#define _nb_components 4
#else
#ifdef COLOR_ORDER_RGB
#define _p_r 0
#define _p_g 1
#define _p_b 2
#define _nb_components 3
#else
#ifdef COLOR_ORDER_RBG
#define _p_r 0
#define _p_g 2
#define _p_b 1
#define _nb_components 3
#else
#ifdef COLOR_ORDER_GBR
#define _p_r 2
#define _p_g 0
#define _p_b 1
#define _nb_components 3
#else
#ifdef COLOR_ORDER_BGR
#define _p_r 2
#define _p_g 1
#define _p_b 0
#define _nb_components 3
#else
#ifdef COLOR_ORDER_BRG
#define _p_r 1
#define _p_g 2
#define _p_b 0
#define _nb_components 3
#else
#ifdef COLOR_ORDER_GRB
#define _p_r 1
#define _p_g 0
#define _p_b 2
#define _nb_components 3
#else

#define _p_r 1
#define _p_g 0
#define _p_b 2
#define _nb_components 3
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#ifdef USE_PIXELSLIB
// #include "pixelslib.h"
#else
#include "___pixeltypes.h"
#endif

#define LCD_DRIVER_PSRAM_DATA_ALIGNMENT 64
#define CLOCKLESS_PIXEL_CLOCK_HZ (24 * 100 * 1000)

namespace fl {

static bool IRAM_ATTR flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                  esp_lcd_panel_io_event_data_t *edata,
                                  void *user_ctx);

typedef union {
    uint8_t bytes[16];
    uint32_t shorts[8];
    uint32_t raw[2];
} Lines;

enum colorarrangment {
    ORDER_GRBW,
    ORDER_RGB,
    ORDER_RBG,
    ORDER_GRB,
    ORDER_GBR,
    ORDER_BRG,
    ORDER_BGR,
};

enum displayMode {
    NO_WAIT,
    WAIT,
    LOOP,
    LOOP_INTERUPT,
};

bool DRIVER_READY = true;

typedef struct led_driver_t led_driver_t;

struct led_driver_t {
    size_t (*init)();
    void (*update)(uint8_t *colors, size_t len);
};
volatile xSemaphoreHandle I2SClocklessLedDriverS3_sem = NULL;
volatile bool isDisplaying = false;
volatile bool iswaiting = false;

static void IRAM_ATTR transpose16x1_noinline2(unsigned char *A, uint16_t *B) {

    uint32_t x, y, x1, y1, t;

    y = *(unsigned int *)(A);
#if NUMSTRIPS > 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif

#if NUMSTRIPS > 8
    y1 = *(unsigned int *)(A + 8);
#else
    y1 = 0;
#endif
#if NUMSTRIPS > 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

    // pre-transform x
#if NUMSTRIPS > 4
    t = (x ^ (x >> 7)) & AA;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & CC;
    x = x ^ t ^ (t << 14);
#endif
#if NUMSTRIPS > 12
    t = (x1 ^ (x1 >> 7)) & AA;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & CC;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & AA;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & CC;
    y = y ^ t ^ (t << 14);
#if NUMSTRIPS > 8
    t = (y1 ^ (y1 >> 7)) & AA;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & CC;
    y1 = y1 ^ t ^ (t << 14);
#endif
    // final transform
    t = (x & FF) | ((y >> 4) & FF2);
    y = ((x << 4) & FF) | (y & FF2);
    x = t;

    t = (x1 & FF) | ((y1 >> 4) & FF2);
    y1 = ((x1 << 4) & FF) | (y1 & FF2);
    x1 = t;

    *((uint16_t *)(B)) =
        (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 3)) =
        (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
    *((uint16_t *)(B + 6)) =
        (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 9)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
    *((uint16_t *)(B + 12)) =
        (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 15)) =
        (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
    *((uint16_t *)(B + 18)) =
        (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 21)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
}

esp_lcd_panel_io_handle_t led_io_handle = NULL;

class I2SClocklessLedDriveresp32S3 {

  public:
    int testcount;
    uint16_t *buffers[2];
    uint16_t *led_output = NULL;
    uint16_t *led_output2 = NULL;
    uint8_t *ledsbuff = NULL;
    int num_leds_per_strip;
    int _numstrips;
    int currentframe;

    uint8_t __green_map[256];
    uint8_t __blue_map[256];
    uint8_t __red_map[256];
    uint8_t __white_map[256];
    uint8_t _brightness;
    float _gammar, _gammab, _gammag, _gammaw;

    void setBrightness(int brightness) {
        _brightness = brightness;
        float tmp;
        for (int i = 0; i < 256; i++) {
            tmp = powf((float)i / 255, 1 / _gammag);
            __green_map[i] = (uint8_t)(tmp * brightness);
            tmp = powf((float)i / 255, 1 / _gammag);
            __blue_map[i] = (uint8_t)(tmp * brightness);
            tmp = powf((float)i / 255, 1 / _gammag);
            __red_map[i] = (uint8_t)(tmp * brightness);
            tmp = powf((float)i / 255, 1 / _gammag);
            __white_map[i] = (uint8_t)(tmp * brightness);
        }
    }

    void setGamma(float gammar, float gammab, float gammag, float gammaw) {
        _gammag = gammag;
        _gammar = gammar;
        _gammaw = gammaw;
        _gammab = gammab;
        setBrightness(_brightness);
    }

    void setGamma(float gammar, float gammab, float gammag) {
        _gammag = gammag;
        _gammar = gammar;
        _gammab = gammab;
        setBrightness(_brightness);
    }

    void _initled(uint8_t *leds, const int *pins, int numstrip,
                  int NUM_LED_PER_STRIP) {

        // esp_lcd_panel_io_handle_t init_lcd_driver(unsigned int
        // CLOCKLESS_PIXEL_CLOCK_HZ, size_t _nb_components) {

        esp_lcd_i80_bus_handle_t i80_bus = NULL;

        esp_lcd_i80_bus_config_t bus_config;

        bus_config.clk_src = LCD_CLK_SRC_PLL160M;
        bus_config.dc_gpio_num = 0;
        bus_config.wr_gpio_num = 0;
        // bus_config.data_gpio_nums = (int*)malloc(16*sizeof(int));
        for (int i = 0; i < numstrip; i++) {
            bus_config.data_gpio_nums[i] = pins[i];
        }
        if (numstrip < 16) {
            for (int i = numstrip; i < 16; i++) {
                bus_config.data_gpio_nums[i] = 0;
            }
        }
        bus_config.bus_width = 16;
        bus_config.max_transfer_bytes =
            _nb_components * NUM_LED_PER_STRIP * 8 * 3 * 2 + __OFFSET;
        #if IDF_5_3_OR_EARLIER
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        #endif
        // In IDF 5.3, psram_trans_align became deprecated. We kick the can down
        // the road a little bit and suppress the warning until idf 5.4 arrives.
        bus_config.psram_trans_align = LCD_DRIVER_PSRAM_DATA_ALIGNMENT;
        bus_config.sram_trans_align = 4;
        #if IDF_5_3_OR_EARLIER
        #pragma GCC diagnostic pop
        #endif

        ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

        esp_lcd_panel_io_i80_config_t io_config;

        io_config.cs_gpio_num = -1;
        io_config.pclk_hz = CLOCKLESS_PIXEL_CLOCK_HZ;
        io_config.trans_queue_depth = 1;
        io_config.dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        };
        //.on_color_trans_done = flush_ready,
        // .user_ctx = nullptr,
        io_config.lcd_cmd_bits = 0;
        io_config.lcd_param_bits = 0;
        io_config.user_ctx = this;

        io_config.on_color_trans_done = flush_ready;
        ESP_ERROR_CHECK(
            esp_lcd_new_panel_io_i80(i80_bus, &io_config, &led_io_handle));
    }

    void initled(uint8_t *leds, const int *pins, int numstrip,
                 int NUM_LED_PER_STRIP) {
        currentframe = 0;
        _gammab = 1;
        _gammar = 1;
        _gammag = 1;
        _gammaw = 1;
        setBrightness(255);
        if (I2SClocklessLedDriverS3_sem == NULL) {
            I2SClocklessLedDriverS3_sem = xSemaphoreCreateBinary();
        }
        // esp_lcd_panel_io_handle_t init_lcd_driver(unsigned int
        // CLOCKLESS_PIXEL_CLOCK_HZ, size_t _nb_components) {
        led_output = (uint16_t *)heap_caps_aligned_alloc(
            LCD_DRIVER_PSRAM_DATA_ALIGNMENT,
            8 * _nb_components * NUM_LED_PER_STRIP * 3 * 2 + __OFFSET +
                __OFFSET_END,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        memset(led_output, 0,
               8 * _nb_components * NUM_LED_PER_STRIP * 3 * 2 + __OFFSET +
                   __OFFSET_END);

        led_output2 = (uint16_t *)heap_caps_aligned_alloc(
            LCD_DRIVER_PSRAM_DATA_ALIGNMENT,
            8 * _nb_components * NUM_LED_PER_STRIP * 3 * 2 + __OFFSET +
                __OFFSET_END,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        memset(led_output2, 0,
               8 * _nb_components * NUM_LED_PER_STRIP * 3 * 2 + __OFFSET +
                   __OFFSET_END);
        buffers[0] = led_output;
        buffers[1] = led_output2;
        // led_output[0] = 0xFFFF; //the +1 because it's like the first value
        // doesnt get pushed do not ask me why for now
        // led_output2[0] = 0xFFFF;
        led_output2 += __OFFSET / 2;
        led_output += __OFFSET / 2;

        for (int i = 0; i < NUM_LED_PER_STRIP * _nb_components * 8; i++) {
            led_output[3 * i + 1] =
                0xFFFF; // the +1 because it's like the first value doesnt get
                        // pushed do not ask me why for now
            led_output2[3 * i + 1] = 0xFFFF;
        }
        ledsbuff = leds;
        _numstrips = numstrip;
        num_leds_per_strip = NUM_LED_PER_STRIP;
        _initled(leds, pins, numstrip, NUM_LED_PER_STRIP);
    }

    void transposeAll(uint16_t *ledoutput) {

        uint16_t ledToDisplay = 0;
        Lines secondPixel[_nb_components];
        uint16_t *buff =
            ledoutput + 2; //+1 pour le premier empty +1 pour le 1 systématique
        uint16_t jump = num_leds_per_strip * _nb_components;
        for (int j = 0; j < num_leds_per_strip; j++) {
            uint8_t *poli = ledsbuff + ledToDisplay * _nb_components;
            for (int i = 0; i < _numstrips; i++) {

                secondPixel[_p_g].bytes[i] = __green_map[*(poli + 1)];
                secondPixel[_p_r].bytes[i] = __red_map[*(poli + 0)];
                secondPixel[_p_b].bytes[i] = __blue_map[*(poli + 2)];
                if (_nb_components > 3)
                    secondPixel[3].bytes[i] = __white_map[*(poli + 3)];
                // #endif
                poli += jump;
            }
            ledToDisplay++;
            transpose16x1_noinline2(secondPixel[0].bytes, buff);
            buff += 24;
            transpose16x1_noinline2(secondPixel[1].bytes, buff);
            buff += 24;
            transpose16x1_noinline2(secondPixel[2].bytes, buff);
            buff += 24;
            if (_nb_components > 3) {
                transpose16x1_noinline2(secondPixel[3].bytes, buff);
                buff += 24;
            }
        }
    }

    void show() {
        transposeAll(buffers[currentframe]);
        if (isDisplaying) {
            // Serial.println("on display dejà");
            iswaiting = true;
            if (I2SClocklessLedDriverS3_sem == NULL)
                I2SClocklessLedDriverS3_sem = xSemaphoreCreateBinary();
            xSemaphoreTake(I2SClocklessLedDriverS3_sem, portMAX_DELAY);
        }
        isDisplaying = true;

        if (FASTLED_EXPERIMENTAL_YVES_EXTRA_WAIT_MICROS) {
            delayMicroseconds(FASTLED_EXPERIMENTAL_YVES_EXTRA_WAIT_MICROS);
        }

        led_io_handle->tx_color(led_io_handle, 0x2C, buffers[currentframe],
                                _nb_components * num_leds_per_strip * 8 * 3 *
                                        2 +
                                    __OFFSET + __OFFSET_END);

        currentframe = (currentframe + 1) % 2;
    }
};

static bool IRAM_ATTR flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                  esp_lcd_panel_io_event_data_t *edata,
                                  void *user_ctx) {
    // printf("we're here");
    DRIVER_READY = true;
    isDisplaying = false;
    I2SClocklessLedDriveresp32S3 *cont =
        (I2SClocklessLedDriveresp32S3 *)user_ctx;
    cont->testcount++;
    if (iswaiting) {
        portBASE_TYPE HPTaskAwoken = 0;
        iswaiting = false;
        xSemaphoreGiveFromISR(I2SClocklessLedDriverS3_sem, &HPTaskAwoken);
        if (HPTaskAwoken == pdTRUE)
            portYIELD_FROM_ISR(HPTaskAwoken);
    }
    return false;
}

#pragma GCC diagnostic pop

} // namespace fl

#endif // __has_include("esp_memory_utils.h")

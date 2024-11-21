
/*

 */

#pragma once
#ifdef CONFIG_IDF_TARGET_ESP32S3

#include "esp_heap_caps.h"
#include "freertos/semphr.h"
#include <stdio.h>  // ok include

#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
#include "esp_private/periph_ctrl.h"
#else
#include "driver/periph_ctrl.h"
#endif

#include <soc/gdma_channel.h>
//#include "esp_private/periph_ctrl.h"
//#include "gdma_priv.h"
#include <hal/gdma_types.h>
#include <esp_private/gdma.h>
#include <hal/dma_types.h>
#include <hal/gpio_hal.h>
#include <soc/lcd_cam_struct.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_ll.h"
#include "esp_rom_gpio.h"
#include "driver/gpio.h"
#include "esp_log.h"

namespace fl {
typedef struct gdma_pair_t gdma_pair_t;


struct gdma_channel_t {
    gdma_pair_t *pair;  // which pair the channel belongs to
    intr_handle_t intr; // per-channel interrupt handle
    portMUX_TYPE spinlock;  // channel level spinlock
    gdma_channel_direction_t direction; // channel direction
    int periph_id; // Peripheral instance ID, indicates which peripheral is connected to this GDMA channel
    size_t int_mem_alignment; // alignment for memory in internal memory
    size_t ext_mem_alignment; // alignment for memory in external memory
    esp_err_t (*del)(gdma_channel_t *channel); // channel deletion function, it's polymorphic, see `gdma_del_tx_channel` or `gdma_del_rx_channel`
    struct {
        uint32_t start_stop_by_etm: 1; // whether the channel is started/stopped by ETM
    } flags;
};

#ifdef OVER_CLOCK_MAX
#define CLOCK_DIV_NUM 4
#define CLOCK_DIV_A 20
#define CLOCK_DIV_B 9
#endif
#ifdef OVERCLOCK_1MHZ
#define CLOCK_DIV_NUM 5
#define CLOCK_DIV_A 1
#define CLOCK_DIV_B 0
#endif
#ifdef OVERCLOCK_1_1MHZ
#define CLOCK_DIV_NUM 4
#define CLOCK_DIV_A 8
#define CLOCK_DIV_B 4
#endif
#ifndef CLOCK_DIV_NUM
#define CLOCK_DIV_NUM 6 
#define CLOCK_DIV_A  4
#define CLOCK_DIV_B  1
#endif

typedef struct 
{
    int div_num;
    int div_a;
    int div_b;
}clock_speed;

clock_speed clock_1123KHZ={4,20,9};
clock_speed clock_1111KHZ={4,2,1};
clock_speed clock_1000KHZ={5,1,0};
clock_speed clock_800KHZ={6,4,1};

} // namespace fl



#define WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE (576*2)



#else
#include "esp_heap_caps.h"
#include "soc/soc.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/io_mux_reg.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "rom/lldesc.h"
#include <cstring>  // ok include
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>  // ok include
#include <rom/ets_sys.h>
// #include "esp32-hal-log.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "hal/gpio_ll.h"
#include "soc/gpio_struct.h"
#include "rom/gpio.h"
#endif
#include "esp_log.h"
#include <soc/rtc.h>

#endif

#include "math.h"
#include "helper.h"
#define I2S_DEVICE 0

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define NUM_VIRT_PINS 7

#ifndef NBIS2SERIALPINS
#define NBIS2SERIALPINS 1
#endif

#ifndef NUM_LEDS_PER_STRIP
#define NUM_LEDS_PER_STRIP 256
#endif

#ifndef HARDWARESPRITES
#define HARDWARESPRITES 0
#endif

// to define coleor different per strip
#ifndef STATICCOLOR
#define STATICCOLOR 1
#endif

#ifndef DELTA_OFFSET_LATCH
#define DELTA_OFFSET_LATCH 0
#endif
#ifdef COLOR_RGBW
#define p_r 1
#define p_g 0
#define p_b 2
#define nb_components 4
#else
#ifdef COLOR_RGB
#define p_r 0
#define p_g 1
#define p_b 2
#define nb_components 3
#else
#ifdef COLOR_RBG
#define p_r 0
#define p_g 2
#define p_b 1
#define nb_components 3
#else
#ifdef COLOR_GBR
#define p_r 2
#define p_g 0
#define p_b 1
#define nb_components 3
#else
#ifdef COLOR_BGR
#define p_r 2
#define p_g 1
#define p_b 0
#define nb_components 3
#else
#ifdef COLOR_BRG
#define p_r 1
#define p_g 2
#define p_b 0
#define nb_components 3
#else
#ifdef COLOR_GRB
#define p_r 1
#define p_g 0
#define p_b 2
#define nb_components 3
#else

#define p_r 1
#define p_g 0
#define p_b 2
#define nb_components 3
#endif
#endif
#endif
#endif
#endif
#endif
#endif

#ifdef _USE_PALETTE
#ifndef PALETTE_SIZE
#define _palette_size 1
#else
#define _palette_size PALETTE_SIZE
#endif
#else
#define _palette_size nb_components
#endif

#define OFFSET (NUM_VIRT_PINS + 1)
#define I2S_OFF (((NUM_VIRT_PINS + 1) * NUM_LEDS_PER_STRIP) * _palette_size)
#define I2S_OFF2 ((I2S_OFF * NBIS2SERIALPINS - NUM_LEDS_PER_STRIP * _palette_size))
#define I2S_OFF3 ((I2S_OFF * NBIS2SERIALPINS + NUM_LEDS_PER_STRIP * _palette_size))
#define I2S_OFF4 ((I2S_OFF * NBIS2SERIALPINS - 3 * NUM_LEDS_PER_STRIP * _palette_size))
#define I2S_OFF_MAP (((NUM_VIRT_PINS + 1) * NUM_LEDS_PER_STRIP))
#define I2S_OFF2_MAP ((I2S_OFF_MAP * NBIS2SERIALPINS - NUM_LEDS_PER_STRIP))
#define I2S_OFF3_MAP ((I2S_OFF_MAP * NBIS2SERIALPINS + NUM_LEDS_PER_STRIP))
#define I2S_OFF4_MAP ((I2S_OFF_MAP * NBIS2SERIALPINS - 3 * NUM_LEDS_PER_STRIP))
#define BUFFOFF ((NBIS2SERIALPINS * 8) - 1)
#define AAA (0x00AA00AAL)
#define CCC (0x0000CCCCL)
#define FFF (0xF0F0F0F0L)
#define FFF2 (0xF0F0F0FL)

// this below is only the the ws281X if you need more time then you are able to extend the size of the buffer
#ifndef _DMA_EXTENSTION
#define _DMA_EXTENSTION 0
#endif

#ifndef __NB_DMA_BUFFER

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define __NB_DMA_BUFFER 10
#else
#define __NB_DMA_BUFFER 2
#endif

#endif

#ifndef __MAX_BRIGTHNESS
#define __MAX_BRIGTHNESS 255
#endif

#ifndef __BRIGHTNESS_BIT
#define __BRIGHTNESS_BIT 8
#else
#define __HARDWARE_BRIGHTNESS
#endif

#define _BRIGHTNES_8 ((8 - __BRIGHTNESS_BIT) * 48)
#define _BRIGHTNES_7 ((8 - __BRIGHTNESS_BIT + 1) * 48)
#define _BRIGHTNES_6 ((8 - __BRIGHTNESS_BIT + 2) * 48)
#define _BRIGHTNES_5 ((8 - __BRIGHTNESS_BIT + 3) * 48)
#define _BRIGHTNES_4 ((8 - __BRIGHTNESS_BIT + 4) * 48)
#define _BRIGHTNES_3 ((8 - __BRIGHTNESS_BIT + 5) * 48)
#define _BRIGHTNES_2 ((8 - __BRIGHTNESS_BIT + 6) * 48)
#define _BRIGHTNES_1 ((8 - __BRIGHTNESS_BIT + 7) * 48)

#define _NB_BIT (_DMA_EXTENSTION * 2 + (NUM_VIRT_PINS + 1) * nb_components * 8 * 3)
#define _BUFFER_TIMING ((_NB_BIT / 19.2) - 4)
#define _BASE_BUFFER_TIMING (((NUM_VIRT_PINS + 1) * nb_components * 8 * 3 / 19.2) - 4)

#define I2S_DRIVER_MODE_OPTION_VIRTUAL_PIN 0x100
#define I2S_DRIVER_MODE_OPTION_CLOCKLESS 0x200
#define I2S_DRIVER_MODE_OPTION_CLOCKBASED 0x400
#define I2S_MAPPING_MODE_OPTION_NONE 0x1
#define I2S_MAPPING_MODE_OPTION_DIRECT_CALCULATION 0x2
#define I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY 0x4
#define I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE 0x8
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE 0x20
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY 0x10
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_ALL_IN_MEMORY (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY | I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY)
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY_SOFTWARE (I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE)
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_IN_MEMORY (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY | I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)

#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_ALL_IN_MEMORY
#define I2S_MAPING_MODE_OPTION_MAPPING I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY

#define I2S_MAPPING_MODE_OPTION_INTERRUPT_LINE 0x1000

#ifndef I2S_DRIVER_MODE
#define I2S_DRIVER_MODE I2S_DRIVER_MODE_OPTION_CLOCKLESS
#endif
#ifndef I2S_MAPPING_MODE
#define I2S_MAPPING_MODE I2S_MAPPING_MODE_OPTION_NONE
#endif
#define INTERUPT_NUM_LINE_MAX 100
#define SCALEMAX_BIT 6
#define SCALEMAX (1 << SCALEMAX_BIT)

#ifdef CONFIG_IDF_TARGET_ESP32S3
#define WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE (576*2)
#else
#define WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE ((NUM_VIRT_PINS + 1) * nb_components * 8 * 3 * 2 + _DMA_EXTENSTION * 4)
#endif

#include "___pixeltypes.h"

// Note - Unused and can be removed
#define __delay (((NUM_LEDS_PER_STRIP * 125 * 8 * nb_components) / 100000) + 1)

#define _MAX_VALUE 5000

#include "framebuffer.h"

namespace fl {

uint16_t __default__mapping(uint16_t pos)
{
    return pos;
}
typedef union
{
    uint8_t bytes[16 * 8];
    uint32_t shorts[16 * 2];
} Lines;

#ifdef CONFIG_IDF_TARGET_ESP32S3
static uint8_t signalsID[16]={
LCD_DATA_OUT0_IDX,
LCD_DATA_OUT1_IDX,
LCD_DATA_OUT2_IDX,
LCD_DATA_OUT3_IDX,
LCD_DATA_OUT4_IDX,
LCD_DATA_OUT5_IDX,
LCD_DATA_OUT6_IDX,
LCD_DATA_OUT7_IDX,
LCD_DATA_OUT8_IDX,
LCD_DATA_OUT9_IDX,
LCD_DATA_OUT10_IDX,
LCD_DATA_OUT11_IDX,
LCD_DATA_OUT12_IDX,
LCD_DATA_OUT13_IDX,
LCD_DATA_OUT14_IDX,
LCD_DATA_OUT15_IDX,

};
static gdma_channel_handle_t dma_chan;

#endif

class I2SClocklessVirtualLedDriver;
struct OffsetDisplay
{
    int offsetx;
    int offsety;
    int panel_height;
    int panel_width;
    int image_height;
    int image_width;
    int window_width;
    int window_height;
    int _offx;
    int xc;
    int yc;
    float rotation;
    bool enableLoopx;
    bool enableLoopy;
    bool enableRotation;
    float scaling;
    long _deltax;
    long _deltay;
    int _defaultvalue;
    bool isOffsetDisplay;
    int _cos;
    int _sin;
    int _offy;
};

typedef struct
{
    int xc;
    int yc;
    int _cos;
    int _sin;
    int _offx;
    int _offy;
    int panel_height;
    int panel_width;
    int image_height;
    int image_width;
    bool enableLoopx;
    bool enableLoopy;
    int enableOption;
} __OffsetDisplay;
#if CORE_DEBUG_LEVEL >= 1
static const char *TAG = "I2SClocklessVirtualLedDriver";
#endif
#ifdef CONFIG_IDF_TARGET_ESP32S3
static IRAM_ATTR bool _I2SClocklessVirtualLedDriverinterruptHandler(gdma_channel_handle_t dma_chan,gdma_event_data_t *event_data, void *user_data);
#else
static void IRAM_ATTR _I2SClocklessVirtualLedDriverinterruptHandler(void *arg);
#endif
static void IRAM_ATTR transpose16x1_noinline2(unsigned char *A, uint8_t *B);

static void IRAM_ATTR loadAndTranspose(I2SClocklessVirtualLedDriver *driver);
static TaskHandle_t I2SClocklessVirtualLedDriver_dispTaskHandle = 0;
static TaskHandle_t I2SClocklessVirtualLedDriver_returnTaskHandle = 0;
static void showPixelsTask(void *pvParameters);
int interruptSource;

enum colorarrangment
{
    ORDER_GRBW,
    ORDER_RGB,
    ORDER_RBG,
    ORDER_GRB,
    ORDER_GBR,
    ORDER_BRG,
    ORDER_BGR,
};

enum displayMode
{
    NO_WAIT,
    WAIT,
    LOOP,
    LOOP_INTERUPT,
};

__OffsetDisplay _internalOffsetDisplay;

static void IRAM_ATTR i2sReset()
{
    #ifdef CONFIG_IDF_TARGET_ESP32S3
gdma_reset(dma_chan);  
LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
    #else
    const unsigned long lc_conf_reset_flags = I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
    //(&I2S0)->lc_conf.val |= lc_conf_reset_flags;
    (&I2S0)->lc_conf.val = (&I2S0)->lc_conf.val | lc_conf_reset_flags;
    //(&I2S0)->lc_conf.val &= ~lc_conf_reset_flags;
    (&I2S0)->lc_conf.val = (&I2S0)->lc_conf.val & (~lc_conf_reset_flags);
    const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M | I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
    // (&I2S0)->conf.val |= conf_reset_flags;
    (&I2S0)->conf.val = (&I2S0)->conf.val | conf_reset_flags;
    // (&I2S0)->conf.val &= ~conf_reset_flags;
    (&I2S0)->conf.val = (&I2S0)->conf.val & (~conf_reset_flags);
    #endif
}
class I2SClocklessVirtualLedDriver
{
#ifndef CONFIG_IDF_TARGET_ESP32S3
    struct I2SClocklessVirtualLedDriverDMABuffer
    {
        lldesc_t descriptor;
        uint8_t *buffer;
    };
#endif
 #ifndef CONFIG_IDF_TARGET_ESP32S3
    const int deviceBaseIndex[2] = {I2S0O_DATA_OUT0_IDX, I2S1O_DATA_OUT0_IDX};
    const int deviceClockIndex[2] = {I2S0O_BCK_OUT_IDX, I2S1O_BCK_OUT_IDX};
    const int deviceWordSelectIndex[2] = {I2S0O_WS_OUT_IDX, I2S1O_WS_OUT_IDX};
    const periph_module_t deviceModule[2] = {PERIPH_I2S0_MODULE, PERIPH_I2S1_MODULE};
#endif
public:
    Lines firstPixel[nb_components];
 #ifndef CONFIG_IDF_TARGET_ESP32S3
    i2s_dev_t *i2s;
    #endif
     #ifdef CONFIG_IDF_TARGET_ESP32S3
     clock_speed _clockspeed=clock_800KHZ;
     #endif
    int _maxtime;
    int _max_pixels_out_of_time;
    int _over_frames;
    int _nb_frames_displayed;
    uint8_t __green_map[256];
    uint8_t __blue_map[256];
    uint8_t __red_map[256];
    uint8_t __white_map[256];
    uint8_t g_map[120];
    uint8_t r_map[120];
    uint8_t b_map[120];
    uint8_t *palette;
    intr_handle_t _gI2SClocklessDriver_intr_handle = NULL;
    uint8_t _brightness;
    int startleds;
    // int linewidth;
    float _gammar, _gammab, _gammag, _gammaw;
    OffsetDisplay _offsetDisplay, _defaultOffsetDisplay;

    volatile xSemaphoreHandle I2SClocklessVirtualLedDriver_sem = NULL;
    volatile xSemaphoreHandle I2SClocklessVirtualLedDriver_semSync = NULL;
    volatile xSemaphoreHandle I2SClocklessVirtualLedDriver_semDisp = NULL;
    volatile xSemaphoreHandle I2SClocklessVirtualLedDriver_waitDisp = NULL;
    bool isOffsetDisplay;
    bool isRunOnCore = false;
    int runCore;
    volatile long tims;
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_INTERRUPT_LINE) > 0
    int _scalingx[INTERUPT_NUM_LINE_MAX * 3];
    int offsetsx[INTERUPT_NUM_LINE_MAX];
    float scalingx[INTERUPT_NUM_LINE_MAX];
    float scalingy[INTERUPT_NUM_LINE_MAX];
#endif
#if CORE_DEBUG_LEVEL >= 4
    uint32_t _times[NUM_LEDS_PER_STRIP];
#endif
    frameBuffer *framebuff;
    bool useFrame = false;
    uint16_t *_hmap, *_defaulthmap;
    uint16_t *_hmapoff;
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY) > 0

    void setHmap(uint16_t *map)
    {
        _defaulthmap = map;
    }
#endif

#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE)) > 0
    uint16_t *_hmapscroll;
#endif

#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY | I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)) > 0
    uint16_t (*mapLed)(uint16_t led);

    inline void setMapLed(uint16_t (*newMapLed)(uint16_t led))
    {
        if (newMapLed != NULL)
            mapLed = newMapLed;
        else
            mapLed = __default__mapping;
        ESP_LOGD(TAG, "calculate mapping");
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY) > 0
        calculateDefaultMapping();
#endif
        ESP_LOGD(TAG, " mapping done");
    }

#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_DIRECT_CALCULATION) > 0
#ifndef _USE_PALETTE
    Pixel (*pixelCalc)(uint16_t led, int pin, int virtualpin);

    void setPixelCalc(Pixel (*newPixelCalc)(uint16_t led, int pin, int virtualpin))
    {
        pixelCalc = newPixelCalc;
        ESP_LOGD(TAG, "calculate mapping");
        // calculateMapping(_defaultOffsetDisplay);
        ESP_LOGD(TAG, " mapping done");
    }
#else
    uint16_t (*pixelCalc)(uint16_t led, int pin, int virtualpin);

    void setPixelCalc(uint16_t (*newPixelCalc)(uint16_t led, int pin, int virtualpin))
    {
        pixelCalc = newPixelCalc;
        ESP_LOGD(TAG, "calculate mapping");
        // calculateMapping(_defaultOffsetDisplay);
        ESP_LOGD(TAG, " mapping done");
    }
#endif
#endif

    bool driverInit = false;

    /*
     This flag is used when using the NO_WAIT mode
     */

    volatile bool isDisplaying = false;
    volatile bool __enableDriver = true;
    volatile bool isWaiting = true;
    volatile bool framesync = false;
    volatile bool wasWaitingtofinish = false;
    volatile int counti;

    I2SClocklessVirtualLedDriver() {};

    void setPins(int *Pins, int clock_pin, int latch_pin)
    {
         #ifndef CONFIG_IDF_TARGET_ESP32S3
         
        for (int i = 0; i < NBIS2SERIALPINS; i++)
        {
            PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[Pins[i]], PIN_FUNC_GPIO);
            gpio_set_direction((gpio_num_t)Pins[i], (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
            gpio_matrix_out(Pins[i], deviceBaseIndex[I2S_DEVICE] + i + 8, false, false);
        }
        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[latch_pin], PIN_FUNC_GPIO);
        gpio_set_direction((gpio_num_t)latch_pin, (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
        gpio_matrix_out(latch_pin, deviceBaseIndex[I2S_DEVICE] + NBIS2SERIALPINS + 8, false, false);
        gpio_set_direction((gpio_num_t)clock_pin, (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
        gpio_matrix_out(clock_pin, deviceClockIndex[I2S_DEVICE], false, false);
        #else
 for (int i = 0; i < NBIS2SERIALPINS; i++)
        {
                    esp_rom_gpio_connect_out_signal(Pins[i], signalsID[i], false, false);
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[Pins[i]], PIN_FUNC_GPIO);
        gpio_set_drive_capability((gpio_num_t)Pins[i], (gpio_drive_cap_t)3);

        }
                           esp_rom_gpio_connect_out_signal(latch_pin, signalsID[NBIS2SERIALPINS], false, false);
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[latch_pin], PIN_FUNC_GPIO);
        gpio_set_drive_capability((gpio_num_t)latch_pin, (gpio_drive_cap_t)3);

                            esp_rom_gpio_connect_out_signal(clock_pin, LCD_PCLK_IDX, false, false);
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[clock_pin], PIN_FUNC_GPIO);
        gpio_set_drive_capability((gpio_num_t)clock_pin, (gpio_drive_cap_t)3);
        #endif
    }

    void setColorOrderPerStrip(int stripnumber, colorarrangment arr)
    {
        switch (arr)
        {
        case ORDER_RGB:

            r_map[stripnumber] = 0;
            g_map[stripnumber] = 1;
            b_map[stripnumber] = 2;
            break;
        case ORDER_RBG:
            r_map[stripnumber] = 0;
            g_map[stripnumber] = 2;
            b_map[stripnumber] = 1;

            break;
        case ORDER_GRB:
            r_map[stripnumber] = 1;
            g_map[stripnumber] = 0;
            b_map[stripnumber] = 2;
            break;
        case ORDER_GBR:
            r_map[stripnumber] = 2;
            g_map[stripnumber] = 0;
            b_map[stripnumber] = 1;

            break;
        case ORDER_BRG:
            r_map[stripnumber] = 1;
            g_map[stripnumber] = 2;
            b_map[stripnumber] = 0;

            break;
        case ORDER_BGR:
            r_map[stripnumber] = 2;
            g_map[stripnumber] = 1;
            b_map[stripnumber] = 0;
            break;
        case ORDER_GRBW:
            r_map[stripnumber] = 1;
            g_map[stripnumber] = 0;
            b_map[stripnumber] = 2;
            break;
        }
    }
    void setBrightness(int brightness)
    {
        _brightness = brightness;
        float tmp;
        for (int i = 0; i < 256; i++)
        {
            tmp = powf((float)(i) / 255, _gammag);
            __green_map[i] = (uint8_t)(tmp * brightness);
            tmp = powf((float)(i) / 255, _gammab);
            __blue_map[i] = (uint8_t)(tmp * brightness);
            tmp = powf((float)(i) / 255, _gammar);
            __red_map[i] = (uint8_t)(tmp * brightness);
            tmp = powf((float)(i) / 255, _gammaw);
            __white_map[i] = (uint8_t)(tmp * brightness);
        }
    }

    void setGamma(float gammar, float gammab, float gammag, float gammaw)
    {
        _gammag = gammag;
        _gammar = gammar;
        _gammaw = gammaw;
        _gammab = gammab;
        setBrightness(_brightness);
    }

    void setGamma(float gammar, float gammab, float gammag)
    {
        _gammag = gammag;
        _gammar = gammar;
        _gammab = gammab;
        setBrightness(_brightness);
    }
    void i2sInit()
    {

        #ifdef CONFIG_IDF_TARGET_ESP32S3
    periph_module_enable(PERIPH_LCD_CAM_MODULE);
    periph_module_reset(PERIPH_LCD_CAM_MODULE);

    // Reset LCD bus
    LCD_CAM.lcd_user.lcd_reset = 1;
    esp_rom_delay_us(100);

    LCD_CAM.lcd_clock.clk_en = 1;             // Enable peripheral clock
    LCD_CAM.lcd_clock.lcd_clk_sel = 2;        // XTAL_CLK source
    LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;    // PCLK low in 1st half cycle
    LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;   // PCLK low idle
    LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 0; // PCLK = CLK / (CLKCNT_N+1)
    LCD_CAM.lcd_clock.lcd_clkm_div_num =  _clockspeed.div_num;   // 1st stage 1:250 divide
    LCD_CAM.lcd_clock.lcd_clkm_div_a = _clockspeed.div_a;     // 0/1 fractional divide
    LCD_CAM.lcd_clock.lcd_clkm_div_b = _clockspeed.div_b;
    LCD_CAM.lcd_clock.lcd_clkcnt_n = 1; //

  LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 0;    // i8080 mode (not RGB)
    LCD_CAM.lcd_rgb_yuv.lcd_conv_bypass = 0; // Disable RGB/YUV converter
    LCD_CAM.lcd_misc.lcd_next_frame_en = 0;  // Do NOT auto-frame
    LCD_CAM.lcd_data_dout_mode.val = 0;      // No data delays
    LCD_CAM.lcd_user.lcd_always_out_en = 1;  // Enable 'always out' mode
    LCD_CAM.lcd_user.lcd_8bits_order = 0;    // Do not swap bytes
    LCD_CAM.lcd_user.lcd_bit_order = 0;      // Do not reverse bit order
    LCD_CAM.lcd_user.lcd_byte_order = 0 ;
    LCD_CAM.lcd_user.lcd_2byte_en = 1;       // 8-bit data mode
    LCD_CAM.lcd_user.lcd_dummy = 0;          // Dummy phase(s) @ LCD start
    LCD_CAM.lcd_user.lcd_dummy_cyclelen = 0; // 1 dummy phase
    LCD_CAM.lcd_user.lcd_cmd = 0;            // No command at LCD start
    LCD_CAM.lcd_misc.lcd_bk_en = 1;
        // -- Create a semaphore to block execution until all the controllers are done
gdma_channel_alloc_config_t dma_chan_config = {
        .sibling_chan = NULL,
        .direction = GDMA_CHANNEL_DIRECTION_TX,
        .flags = {
            .reserve_sibling = 0}};
    gdma_new_channel(&dma_chan_config, &dma_chan);
    gdma_connect(dma_chan, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
    gdma_strategy_config_t strategy_config = {
        .owner_check = false,
        .auto_update_desc = false};
    gdma_apply_strategy(dma_chan, &strategy_config);
    /*
    gdma_transfer_ability_t ability = {
        .psram_trans_align = 64,
        //.sram_trans_align = 64,
    };
    gdma_set_transfer_ability(dma_chan, &ability);
*/
    // Enable DMA transfer callback
    gdma_tx_event_callbacks_t tx_cbs = {
        .on_trans_eof = _I2SClocklessVirtualLedDriverinterruptHandler};
    gdma_register_tx_event_callbacks(dma_chan, &tx_cbs, this);
     // esp_intr_disable((*dma_chan).intr);
    LCD_CAM.lcd_user.lcd_start=0;
        #else

        if (I2S_DEVICE == 0)
        {
            i2s = &I2S0;
            periph_module_enable(PERIPH_I2S0_MODULE);
            interruptSource = ETS_I2S0_INTR_SOURCE;
            i2s_base_pin_index = I2S0O_DATA_OUT0_IDX;
        }
        else
        {
            i2s = &I2S1;
            periph_module_enable(PERIPH_I2S1_MODULE);
            interruptSource = ETS_I2S1_INTR_SOURCE;
            i2s_base_pin_index = I2S1O_DATA_OUT0_IDX;
        }
        i2sReset();
        i2sReset_DMA();
        i2sReset_FIFO();
        i2s->conf.tx_right_first = 0;

        // -- Set parallel mode
        i2s->conf2.val = 0;
        i2s->conf2.lcd_en = 1;
        i2s->conf2.lcd_tx_wrx2_en = 1; // 0 for 16 or 32 parallel output
        i2s->conf2.lcd_tx_sdx2_en = 0; // HN

        // -- Set up the clock rate and sampling
        i2s->sample_rate_conf.val = 0;
        i2s->sample_rate_conf.tx_bits_mod = 16; // Number of parallel bits/pins
        i2s->clkm_conf.val = 0;
        // i2s->sample_rate_conf.tx_bck_div_num = 1;
#ifdef __DL_CLK
        // Serial.println("norml clock");
        i2s->clkm_conf.clka_en = 0;
        // rtc_clk_apll_enable(true, 31, 133,7, 1); //19.2Mhz 7 pins +1 latchrtc_clk_apll_enable(true, 31, 133,7, 1); //19.2Mhz 7 pins +1 latch

        // -- Data clock is computed as Base/(div_num + (div_b/div_a))
        //    Base is 80Mhz, so 80/(3+ 7/6) = 19.2Mhz

        i2s->clkm_conf.clkm_div_a = 6;   // CLOCK_DIVIDER_A;
        i2s->clkm_conf.clkm_div_b = 7;   // CLOCK_DIVIDER_B;ƒ◊
        i2s->clkm_conf.clkm_div_num = 3; // CLOCK_DIVIDER_N;

#else
        // Serial.println("precise clock");

#ifndef _20_MHZ_CLK
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        rtc_clk_apll_enable(true);
        rtc_clk_apll_coeff_set(1, 31, 133, 7);
#else
        rtc_clk_apll_enable(true, 31, 133, 7, 1); // 19.2Mhz 7 pins +1 latchrtc_clk_apll_enable(true, 31, 133,7, 1); //19.2Mhz 7 pins +1 latch
#endif
#else
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        rtc_clk_apll_enable(true);
        rtc_clk_apll_coeff_set(1, 0, 0, 8);
#else
        rtc_clk_apll_enable(true, 0, 0, 8, 1); // 19.2Mhz 7 pins +1 latchrtc_clk_apll_enable(true, 31, 133,7, 1); //19.2Mhz 7 pins +1 latch
#endif
        // rtc_clk_apll_enable(true, 0, 0, 8, 1);
#endif
        i2s->clkm_conf.clka_en = 1;
        i2s->clkm_conf.clkm_div_a = 1;   // CLOCK_DIVIDER_A;
        i2s->clkm_conf.clkm_div_b = 0;   // CLOCK_DIVIDER_B;
        i2s->clkm_conf.clkm_div_num = 1; // CLOCK_DIVIDER_N;
#endif
        i2s->fifo_conf.val = 0;
        i2s->fifo_conf.tx_fifo_mod_force_en = 1;
        i2s->fifo_conf.tx_fifo_mod = 1;  // 16-bit single channel data
        i2s->fifo_conf.tx_data_num = 32; // 32; // fifo length
        i2s->fifo_conf.dscr_en = 1;      // fifo will use dma

        i2s->sample_rate_conf.tx_bck_div_num = 1;
        i2s->conf1.val = 0;
        i2s->conf1.tx_stop_en = 0;
        i2s->conf1.tx_pcm_bypass = 1;

        i2s->conf_chan.val = 0;
        i2s->conf_chan.tx_chan_mod = 1; // Mono mode, with tx_msb_right = 1, everything goes to right-channel

        i2s->timing.val = 0;

        // -- Create a semaphore to block execution until all the controllers are done

#endif
        if (I2SClocklessVirtualLedDriver_sem == NULL)
        {
            I2SClocklessVirtualLedDriver_sem = xSemaphoreCreateBinary();
        }

        if (I2SClocklessVirtualLedDriver_semSync == NULL)
        {
            I2SClocklessVirtualLedDriver_semSync = xSemaphoreCreateBinary();
        }
        if (I2SClocklessVirtualLedDriver_semDisp == NULL)
        {
            I2SClocklessVirtualLedDriver_semDisp = xSemaphoreCreateBinary();
        }
    }

    void initDMABuffers()
    {

DMABuffersTampon=(I2SClocklessVirtualLedDriverDMABuffer ** )heap_caps_malloc(sizeof(I2SClocklessVirtualLedDriverDMABuffer  *) *(__NB_DMA_BUFFER+2),MALLOC_CAP_DMA);
        for (int num_buff = 0; num_buff < __NB_DMA_BUFFER + 2; num_buff++)
        {
            DMABuffersTampon[num_buff] = allocateDMABuffer(WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE);
            putdefaultlatch((uint16_t *)DMABuffersTampon[num_buff]->buffer);
        } // the buffers for the

        for (int num_buff = 0; num_buff < __NB_DMA_BUFFER; num_buff++)
        {
            putdefaultones((uint16_t *)DMABuffersTampon[num_buff]->buffer);
        }
 #ifdef CONFIG_IDF_TARGET_ESP32S3
        for (int buff_num = 0; buff_num < __NB_DMA_BUFFER - 1; buff_num++)
        {
    
            DMABuffersTampon[buff_num]->next = DMABuffersTampon[buff_num + 1];

        }
   

  DMABuffersTampon[__NB_DMA_BUFFER - 1]->next = DMABuffersTampon[0];
        DMABuffersTampon[__NB_DMA_BUFFER]->next = DMABuffersTampon[0];
    //memset(DMABuffersTampon[__NB_DMA_BUFFER]->buffer,0,WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE);
     //memset(DMABuffersTampon[__NB_DMA_BUFFER+1]->buffer,0,WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE);
      DMABuffersTampon[__NB_DMA_BUFFER+1]->next = NULL;
     DMABuffersTampon[__NB_DMA_BUFFER]->dw0.suc_eof = 0;
     #endif
    }

    void setPixel(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
    {

        uint8_t *offset = leds + (pos << 2); // faster than doing * 4

        *(offset) = red;
        *(++offset) = green;
        *(++offset) = blue;
        *(++offset) = white;
    }

    void setPixel(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue)
    {
        if (!driverInit)
        {
            // printf("Driver not initialized\n");
            return;
        }
        uint8_t *offset = leds + (pos << 1) + pos;

        if (nb_components == 3)
        {
            // uint8_t *offset = leds + (pos << 1) + pos;
            *(offset) = red;
            *(++offset) = green;
            *(++offset) = blue;
        }
        else
        {
            /*
                Code to transform RBG into RGBW thanks to @Jonathanese https://github.com/Jonathanese/NodeMCUPoleDriver/blob/master/LED_Framework.cpp
            */
            uint8_t W = MIN(red, green);
            W = MIN(W, blue);
            red = red - W;
            green = green - W;
            blue = blue - W;
            setPixel(pos, red, green, blue, W);
        }
    }

    void setPalette(uint8_t *pal)
    {
        palette = pal;
    }
    OffsetDisplay getDefaultOffset()
    {
        return _defaultOffsetDisplay;
    }

    void waitDisplay()
    {
        if (isDisplaying == true)
        {
            wasWaitingtofinish = true;
            const TickType_t xDelay = 100;
            xSemaphoreTake(I2SClocklessVirtualLedDriver_waitDisp, xDelay);
        }
        isDisplaying = true;
    }

    void calculateOffsetDisplay(OffsetDisplay offdisp)
    {
        if (offdisp.image_width == 0 or offdisp.image_width == _MAX_VALUE)
        {
            offdisp.image_width = offdisp.panel_width;
        }
        if (offdisp.image_height == 0 or offdisp.image_height == _MAX_VALUE)
        {
            offdisp.image_height = offdisp.panel_height;
        }
        if (offdisp.window_width == 0 or offdisp.window_width == _MAX_VALUE)
        {

            offdisp.window_width = offdisp.image_width;
        }
        if (offdisp.window_height == 0 or offdisp.window_height == _MAX_VALUE)
        {
            offdisp.window_height = offdisp.image_height;
        }
        int enableOption = 0;
        if (offdisp.enableLoopx)
        {
            offdisp._offx = -offdisp.offsetx + 10 * offdisp.window_width + offdisp.xc;
            enableOption += 1;
        }
        else
            offdisp._offx = -offdisp.offsetx + offdisp.xc;
        if (offdisp.enableLoopy)
        {
            offdisp._offy = -offdisp.offsety + 10 * offdisp.window_height + offdisp.yc;
            enableOption += 2;
        }
        else
            offdisp._offy = -offdisp.offsety + offdisp.yc;

        _internalOffsetDisplay.enableOption = enableOption;

        if (offdisp.scaling < 0.1 and offdisp.scaling >= 0)
            offdisp.scaling = 0.1;
        if (offdisp.scaling > -0.1 and offdisp.scaling < 0)
            offdisp.scaling = -0.1;

        offdisp._cos = (int)(float)(128 * cos(-offdisp.rotation) / offdisp.scaling);

        offdisp._sin = (int)(float)(128 * sin(-offdisp.rotation) / offdisp.scaling);

        _offsetDisplay = offdisp;
        _internalOffsetDisplay.image_height = offdisp.image_height;
        _internalOffsetDisplay.image_width = offdisp.image_width;
        _internalOffsetDisplay.panel_height = offdisp.panel_height;
        _internalOffsetDisplay.panel_width = offdisp.panel_width;
        _internalOffsetDisplay.xc = offdisp.xc;
        _internalOffsetDisplay.yc = offdisp.yc;
        _internalOffsetDisplay._cos = offdisp._cos;
        _internalOffsetDisplay._sin = offdisp._sin;
        _internalOffsetDisplay._offx = offdisp._offx + offdisp.yc * offdisp._sin / 128 - offdisp.xc * offdisp._cos / 128;
        _internalOffsetDisplay._offy = offdisp._offy - offdisp.yc * offdisp._cos / 128 - offdisp.xc * offdisp._sin / 128;
        _internalOffsetDisplay.enableLoopx = _offsetDisplay.enableLoopx;
        _internalOffsetDisplay.enableLoopy = _offsetDisplay.enableLoopy;
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_INTERRUPT_LINE) > 0
        for (int i = 0; i < INTERUPT_NUM_LINE_MAX; i++)
        {
            if (abs(scalingx[i]) < 0.05)
            {
                if (scalingx[i] > 0)
                    scalingx[i] = 0.05;
                else
                    scalingx[i] = -0.05;
            }
            if (abs(scalingy[i]) < 0.05)
            {
                if (scalingy[i] > 0)
                    scalingy[i] = 0.05;
                else
                    scalingy[i] = -0.05;
            }
            _scalingx[3 * i] = SCALEMAX / scalingx[i];
            _scalingx[3 * i + 1] = ((-_internalOffsetDisplay.panel_width / 2) * SCALEMAX + (_internalOffsetDisplay.panel_width / 2 + offsetsx[i]) * _scalingx[3 * i]) / SCALEMAX;

            if (i >= 1)
            {
                _scalingx[3 * i + 2] = _scalingx[3 * (i - 1) + 2] + 1 / scalingy[i] * SCALEMAX;
            }
        }
#endif
    }

    void showPixels(displayMode dispmode, OffsetDisplay offdisp)
    {
        waitDisplay();
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
        _offsetDisplay = offdisp;
        isOffsetDisplay = offdisp.isOffsetDisplay;
        __displayMode = dispmode;
        if (useFrame)
        {
            leds = framebuff->getFrametoDisplay();
        }
        else
        {
            leds = saveleds;
        }
        __showPixels();
#endif
    }

    void showPixels(uint8_t *newleds)
    {

        waitDisplay();
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY) > 0
        isOffsetDisplay = false;
        _hmapoff = _defaulthmap;
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY) > 0
        _hmapscroll = _defaulthmap;
#endif
#endif

        leds = newleds;
        __displayMode = WAIT;
        _offsetDisplay = _defaultOffsetDisplay;
        __showPixels();
    }
    void showPixels(OffsetDisplay offdisp)
    {
        waitDisplay();
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
        _offsetDisplay = offdisp;
        isOffsetDisplay = offdisp.isOffsetDisplay;

        if (useFrame)
        {
            leds = framebuff->getFrametoDisplay();
            __displayMode = NO_WAIT;
        }
        else
        {
            leds = saveleds;
            __displayMode = WAIT;
        }

        __showPixels();
#endif
    }

    void showPixels(displayMode dispmode, uint8_t *newleds, OffsetDisplay offd)
    {
        waitDisplay();
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
        _offsetDisplay = offd;
        isOffsetDisplay = offd.isOffsetDisplay;
        __displayMode = dispmode;
        leds = newleds;
        __showPixels();
        // calculateMapping(_defaultOffsetDisplay);
        // _offsetDisplay = _defaultOffsetDisplay;
#endif
    }
    void showPixels(displayMode dispmode)
    {

        waitDisplay();
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY) > 0
        _hmap = _defaulthmap;
        isOffsetDisplay = false;
        _hmapoff = _defaulthmap;
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY) > 0
        _hmapscroll = _defaulthmap;
#endif
#endif
        // leds = newleds;
        if (useFrame)
        {
            leds = framebuff->getFrametoDisplay();
        }
        else
        {
            leds = saveleds;
        }
        __displayMode = dispmode;
        _offsetDisplay = _defaultOffsetDisplay;
        __showPixels();
    }
    void showPixels(uint8_t *newleds, OffsetDisplay offd)
    {
        waitDisplay();
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
        _offsetDisplay = offd;
        isOffsetDisplay = offd.isOffsetDisplay;
        __displayMode = WAIT;
        leds = newleds;
        __showPixels();

#endif
    }

    void showPixels(displayMode dispmode, uint8_t *newleds)
    {

        waitDisplay();
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY) > 0
        isOffsetDisplay = false;
        _hmapoff = _defaulthmap;
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY) > 0
        _hmapscroll = _defaulthmap;
#endif
        _hmap = _defaulthmap;
#endif
        leds = newleds;
        __displayMode = dispmode;
        _offsetDisplay = _defaultOffsetDisplay;
        __showPixels();
    }

    void showPixels()
    {
        if (!__enableDriver)
            return;
        waitDisplay();

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY) > 0

        isOffsetDisplay = false;
        _hmapoff = _defaulthmap;
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY) > 0
        _hmapscroll = _defaulthmap;
#endif
#endif
        if (useFrame)
        {

            leds = framebuff->getFrametoDisplay();
            if (leds == NULL)
            {
                ESP_LOGD(TAG, "no buffer");
                return;
            }
            __displayMode = NO_WAIT;
            // __displayMode=WAIT;
        }
        else
        {

            leds = saveleds;
            __displayMode = WAIT;
        }
        _offsetDisplay = _defaultOffsetDisplay;

        __showPixels();
    }

    void _runShowPixelsOnCore()
    {

        if (I2SClocklessVirtualLedDriver_returnTaskHandle == 0)
        {

            I2SClocklessVirtualLedDriver_returnTaskHandle = xTaskGetCurrentTaskHandle();

            // -- Trigger the show task
            xTaskNotifyGive(I2SClocklessVirtualLedDriver_dispTaskHandle);

            // -- Wait to be notified that it's done
            // ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            // delay(100);
            // interrupts();
            I2SClocklessVirtualLedDriver_returnTaskHandle = 0;
        }
    }

    void disableShowPixelsOnCore()
    {
        if (I2SClocklessVirtualLedDriver_dispTaskHandle)
        {
            vTaskDelete(I2SClocklessVirtualLedDriver_dispTaskHandle);
        }
        if (_gI2SClocklessDriver_intr_handle != NULL)
        {
            esp_intr_free(_gI2SClocklessDriver_intr_handle);
        }
        _gI2SClocklessDriver_intr_handle = NULL;
        runCore = 0;
        isRunOnCore = false;
    }
    void enableShowPixelsOnCore(int corenum)
    {
        if (corenum > 1)
        {
            ESP_LOGE(TAG, "enableShowPixelsOnCore error corenum > 1 core should be 0 or 1");
            return;
        }
        if (!driverInit)
        {
            ESP_LOGE(TAG, "Driver not initiated this will have no effect ... it will be executed at first call");
            return;
        }
        if (I2SClocklessVirtualLedDriver_dispTaskHandle)
        {
            vTaskDelete(I2SClocklessVirtualLedDriver_dispTaskHandle);
        }
        runCore = corenum;
        isRunOnCore = true;
        xTaskCreatePinnedToCore(showPixelsTask, "showPixelsTask", 2000, this, 3, &I2SClocklessVirtualLedDriver_dispTaskHandle, corenum);
    }

    void __showPixels()
    {
        if (isRunOnCore)
        {
            if (!I2SClocklessVirtualLedDriver_dispTaskHandle)
            {
                ESP_LOGI(TAG, "No running core defined, rexecuting enable");
                enableShowPixelsOnCore(runCore);
                vTaskDelay(10);
            }
            _runShowPixelsOnCore();
        }
        else
        {
            ___showPixels();
        }
    }

    void ___showPixels()
    {
        if (!__enableDriver)
        {
            return;
        }
        #ifndef CONFIG_IDF_TARGET_ESP32S3
        if (_gI2SClocklessDriver_intr_handle == NULL)
        {

            ESP_LOGV(TAG, "setting interupt handler");
            esp_err_t e = esp_intr_alloc(interruptSource, ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM, &_I2SClocklessVirtualLedDriverinterruptHandler, this, &_gI2SClocklessDriver_intr_handle);
            if (e != ESP_OK)
            {
                ESP_LOGE(TAG, "Impossible to create interupt allocation");
                return;
            }
            ESP_LOGV(TAG, "interupt handler set on core %d", xPortGetCoreID());
        }
        else
        {
            ESP_LOGV(TAG, "Interrupt alreay setup on core");
        }

        ESP_LOGV(TAG, "Running on core:%d", xPortGetCoreID());
#endif
        calculateOffsetDisplay(_offsetDisplay);
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE)) > 0

        if (isOffsetDisplay)
        {
            ESP_LOGV(TAG, "calcualting data");

            //  calculateOffsetDisplay(_offsetDisplay);
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY) > 0

            calculateScrollMapping();

#endif
            ESP_LOGV(TAG, "cdone alcualting data");
        }

        isOffsetDisplay = false;
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY) > 0
        _hmapoff = _hmapscroll;
#endif

#endif
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE) > 0
        _hmapoff = _defaulthmap;
#endif

        if (!driverInit)
        {
            ESP_LOGE(TAG, "Driver not initialized");
            return;
        }
#if !(I2S_MAPPING_MODE == I2S_MAPPING_MODE_OPTION_DIRECT_CALCULATION)
        if (leds == NULL)
        {
            ESP_LOGE(TAG, "no leds buffer defined");
            return;
        }
#endif

        transpose = true;
          #ifndef CONFIG_IDF_TARGET_ESP32S3
        for (int buff_num = 0; buff_num < __NB_DMA_BUFFER - 1; buff_num++)
        {
  
 DMABuffersTampon[buff_num]->descriptor.qe.stqe_next = &(DMABuffersTampon[buff_num + 1]->descriptor);
            
        }
        

        DMABuffersTampon[__NB_DMA_BUFFER - 1]->descriptor.qe.stqe_next = &(DMABuffersTampon[0]->descriptor);
        DMABuffersTampon[__NB_DMA_BUFFER]->descriptor.qe.stqe_next = &(DMABuffersTampon[0]->descriptor);
        DMABuffersTampon[__NB_DMA_BUFFER + 1]->descriptor.qe.stqe_next = 0;
    
#else
     for (int buff_num = 0; buff_num < __NB_DMA_BUFFER - 1; buff_num++)
        {
    
            DMABuffersTampon[buff_num]->next = DMABuffersTampon[buff_num + 1];

        }
        DMABuffersTampon[__NB_DMA_BUFFER - 1]->next = DMABuffersTampon[0];
        DMABuffersTampon[__NB_DMA_BUFFER]->next = DMABuffersTampon[0];
        DMABuffersTampon[__NB_DMA_BUFFER+1]->next = NULL;
    #endif
        dmaBufferActive = 0;
        // loadAndTranspose(leds, _offsetDisplay, (uint16_t *)DMABuffersTampon[0]->buffer, ledToDisplay, __green_map, __red_map, __blue_map, __white_map, r_map, g_map, b_map);
        ledToDisplay = 0;
        for (int num_buff = 0; num_buff < __NB_DMA_BUFFER - 1; num_buff++)
        {
            loadAndTranspose(this);
            dmaBufferActive = dmaBufferActive + 1;
            ledToDisplay = ledToDisplay + 1;
        }
        ledToDisplay = ledToDisplay - 1;
        dmaBufferActive = __NB_DMA_BUFFER - 1;
        //__displayMode=dispmode;

        isDisplaying = true;
        /*
        for(int i=0;i<NUM_LEDS_PER_STRIP+10;i++)
        {
               ledToDisplay_in[i]=999;
               ledToDisplay_inbuffer[i]=999;
                ledToDisplay_inbufferfor[i]=0;
        }*/
        ledToDisplay_out = 0;
        i2sStart(DMABuffersTampon[__NB_DMA_BUFFER]);
        if (__displayMode == WAIT)
        {
  
            isWaiting = true;
            if (I2SClocklessVirtualLedDriver_sem == NULL)
                I2SClocklessVirtualLedDriver_sem = xSemaphoreCreateBinary();
            xSemaphoreTake(I2SClocklessVirtualLedDriver_sem, portMAX_DELAY);
            
        }
        else
        {
            isWaiting = false;
            // isDisplaying = true;
        }

// vTaskDelay(1/portTICK_PERIOD_MS);
// delay(1);
#if CORE_DEBUG_LEVEL >= 5
        uint32_t total = 0;
        uint32_t totalmax = 0;

        int _min, _max;
        int _proposed_dma_extension;
        _proposed_dma_extension = 0;
        _max = 0;
        _min = 500 * 240;
        _nb_frames_displayed++;
        for (int _time = 1; _time < NUM_LEDS_PER_STRIP; _time++)
        {
            if (_times[_time] > (_BUFFER_TIMING * 240))
            {
                totalmax++;
                // framenum=_time;
            }

            if (_min > _times[_time])
                _min = _times[_time];
            if (_max < _times[_time])
                _max = _times[_time];
            total += _times[_time];
        }
        if (_maxtime < _max)
            _maxtime = _max;
        if (_max_pixels_out_of_time < totalmax)
            _max_pixels_out_of_time = totalmax;
        if (totalmax > 0)
            _over_frames++;
        if (_maxtime > _BASE_BUFFER_TIMING * 240)
        {
            _proposed_dma_extension = (((float)_maxtime / 240 + 4) * 19.2 - (NUM_VIRT_PINS + 1) * nb_components * 8 * 3) / 2;
        }
        else
        {
            _proposed_dma_extension = 0;
        }
        //\n     -  proposed DMA extension:%d
        ESP_LOGV(TAG, "\nFrame data:\n     - frame number:%d\n     - interupt time min:%0.2fus\n     - interupt time max:%0.2fus\n     - interupt time average:%0.2fus\n     - nb of pixel with interuptime > %0.2fus: %d\n\
Driver data (overall frames):\n     - nb of frames displayed:%d\n     - nb of frames with pixels 'out of time':%d\n     - max interuptime %.2fus\n     - max number of pixels out of interuptime in a frame:%d\n     - proposed DMA extension:%d",
                 _nb_frames_displayed, (float)_min / 240, (float)_max / 240, (float)total / 240 / (NUM_LEDS_PER_STRIP - 1), _BUFFER_TIMING, totalmax, _nb_frames_displayed, _over_frames, (float)_maxtime / 240, _max_pixels_out_of_time, _proposed_dma_extension);
#endif
    }

#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
    inline __attribute__((always_inline)) int remapStatic()
    {
        // printf("%d\n",_internalOffsetDisplay.image_width);
        int ye = (*_hmapoff) / _internalOffsetDisplay.panel_width;
        int xe = (*_hmapoff) - ye * _internalOffsetDisplay.panel_width;

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_INTERRUPT_LINE) > 0

        xe = (((xe)*_scalingx[3 * ye]) >> 6) - (_scalingx[3 * ye + 1]);
        ye = (_scalingx[3 * ye + 2]) >> 6;

#endif

        int xr = (((xe)*_internalOffsetDisplay._cos - (ye)*_internalOffsetDisplay._sin) >> 7) + _internalOffsetDisplay._offx;
        int yr = (((xe)*_internalOffsetDisplay._sin + (ye)*_internalOffsetDisplay._cos) >> 7) + _internalOffsetDisplay._offy;

        switch (_internalOffsetDisplay.enableOption)
        {

        case 0:

            if (xr < 0 or xr >= _internalOffsetDisplay.image_width)
            {

                return _internalOffsetDisplay.image_width * _internalOffsetDisplay.image_height;
            }
            if (yr < 0 or yr >= _internalOffsetDisplay.image_height)
            {
                return _internalOffsetDisplay.image_width * _internalOffsetDisplay.image_height;
            }
            return (xr + (yr)*_internalOffsetDisplay.image_width);
            break;
        case 1:
            xr = xr % _internalOffsetDisplay.image_width;
            if (yr < 0 or yr >= _internalOffsetDisplay.image_height)
            {
                return _internalOffsetDisplay.image_width * _internalOffsetDisplay.image_height;
            }
            return (xr + (yr)*_internalOffsetDisplay.image_width);
            break;
        case 2:

            if (xr < 0 or xr >= _internalOffsetDisplay.image_width)
            {

                return _internalOffsetDisplay.image_width * _internalOffsetDisplay.image_height;
            }
            yr = yr % _internalOffsetDisplay.image_height;
            return (xr + (yr)*_internalOffsetDisplay.image_width);
            break;

        case 3:

            xr = xr % _internalOffsetDisplay.image_width;
            yr = yr % _internalOffsetDisplay.image_height;
            return (xr + (yr)*_internalOffsetDisplay.image_width);
            break;

        default:
            return _internalOffsetDisplay.image_width * _internalOffsetDisplay.image_height;
            break;
        }

        /*
                if ((_internalOffsetDisplay.enableOption & 1))
                {
                    xr = xr % _internalOffsetDisplay.image_width;

                }
                else
                {
                                if (xr < 0 or xr >= _internalOffsetDisplay.image_width)
                    {

                        return _internalOffsetDisplay.image_width * _internalOffsetDisplay.image_height;
                    }
                }


                if ((_internalOffsetDisplay.enableOption & 2))
                {
                                yr = yr % _internalOffsetDisplay.image_height;

                }
                else
                {
                    if (yr < 0 or yr >= _internalOffsetDisplay.image_height)
                    {
                        return _internalOffsetDisplay.image_width * _internalOffsetDisplay.image_height;
                    }
                }

                return (xr + (yr)*_internalOffsetDisplay.image_width);
               */
    }
#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY) > 0
    void calculateDefaultMapping()
    {

        uint16_t offset2 = 0;
        for (uint16_t leddisp = 0; leddisp < NUM_LEDS_PER_STRIP; leddisp++)
        {
            uint16_t led_tmp = NUM_LEDS_PER_STRIP + leddisp;

            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {

#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
                _defaulthmap[offset2] = mapLed(led_tmp);
#else
                _defaulthmap[offset2] = mapLed(led_tmp) * _palette_size;
#endif
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF3_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
                _defaulthmap[offset2] = mapLed(led_tmp);
#else
                _defaulthmap[offset2] = mapLed(led_tmp) * _palette_size;
#endif
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF4_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
                _defaulthmap[offset2] = mapLed(led_tmp);
#else
                _defaulthmap[offset2] = mapLed(led_tmp) * _palette_size;
#endif
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF3_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
                _defaulthmap[offset2] = mapLed(led_tmp);
#else
                _defaulthmap[offset2] = mapLed(led_tmp) * _palette_size;
#endif
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF4_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
                _defaulthmap[offset2] = mapLed(led_tmp);
#else
                _defaulthmap[offset2] = mapLed(led_tmp) * _palette_size;
#endif
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF3_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
                _defaulthmap[offset2] = mapLed(led_tmp);
#else
                _defaulthmap[offset2] = mapLed(led_tmp) * _palette_size;
#endif
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF4_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
                _defaulthmap[offset2] = mapLed(led_tmp);
#else
                _defaulthmap[offset2] = mapLed(led_tmp) * _palette_size;
#endif
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF3_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
                _defaulthmap[offset2] = mapLed(led_tmp);
#else
                _defaulthmap[offset2] = mapLed(led_tmp) * _palette_size;
#endif
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
        }
    }
#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY) > 0
    void calculateScrollMapping()
    {

        // int xr, yr; //,newx,newy;
        if (!_hmapscroll)
        {
            ESP_LOGE(TAG, "No more memory\n");
            return;
        }

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY) > 0
        _hmapoff = _defaulthmap;
        for (uint16_t leddisp = 0; leddisp < NUM_LEDS_PER_STRIP * NBIS2SERIALPINS * 8; leddisp++)
        {
            _hmapscroll[leddisp] = remapStatic() * _palette_size;
            _hmapoff++;
        }
#endif
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE) > 0
        uint16_t offset2 = 0;
        uint16_t val;
        _hmapoff = &val;
        for (uint16_t leddisp = 0; leddisp < NUM_LEDS_PER_STRIP; leddisp++)
        {
            uint16_t led_tmp = NUM_LEDS_PER_STRIP + leddisp;

            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {

                val = mapLed(led_tmp);
                _hmapscroll[offset2] = remapStatic() * _palette_size;
                // printf("%d %d %d\n",led_tmp,val,_hmapscroll[offset2]);
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF3_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {

                val = mapLed(led_tmp);
                _hmapscroll[offset2] = remapStatic() * _palette_size;
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF4_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {

                val = mapLed(led_tmp);
                _hmapscroll[offset2] = remapStatic() * _palette_size;
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF3_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {

                val = mapLed(led_tmp);
                _hmapscroll[offset2] = remapStatic() * _palette_size;
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF4_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {

                val = mapLed(led_tmp);
                _hmapscroll[offset2] = remapStatic() * _palette_size;
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF3_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {

                val = mapLed(led_tmp);
                _hmapscroll[offset2] = remapStatic() * _palette_size;
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF4_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {

                val = mapLed(led_tmp);
                _hmapscroll[offset2] = remapStatic() * _palette_size;
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
            led_tmp -= I2S_OFF3_MAP;
            for (uint16_t i = 0; i < NBIS2SERIALPINS; i++)
            {

                val = mapLed(led_tmp);
                _hmapscroll[offset2] = remapStatic() * _palette_size;
                led_tmp += I2S_OFF_MAP;
                offset2++;
            }
        }

#endif
    }
#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_INTERRUPT_LINE) > 0
    void resetInterruptsParameters()
    {
        for (int i = 0; i < INTERUPT_NUM_LINE_MAX; i++)
        {
            offsetsx[i] = 0;
            scalingx[i] = 1;
            if (i > 0)
                scalingy[i] = 1;
        }
    }
#endif

#ifdef USE_FASTLED
    void initled(CRGB *leds, int *Pinsq, int clock_pin, int latch_pin)
    {
        initled((uint8_t *)leds, Pinsq, clock_pin, latch_pin);
    }
       #ifdef CONFIG_IDF_TARGET_ESP32S3
         void initled(CRGB *leds, int *Pinsq, int clock_pin, int latch_pin,clock_speed clock)
    {
        _clockspeed=clock;
        initled((uint8_t *)leds, Pinsq, clock_pin, latch_pin);
    }
       #endif
#endif

    void initled(Pixel *leds, int *Pinsq, int clock_pin, int latch_pin)
    {
        initled((uint8_t *)leds, Pinsq, clock_pin, latch_pin);
    }

       #ifdef CONFIG_IDF_TARGET_ESP32S3
         void initled(Pixel *leds, int *Pinsq, int clock_pin, int latch_pin,clock_speed clock)
    {
        _clockspeed=clock;
        initled((uint8_t *)leds, Pinsq, clock_pin, latch_pin);
    }
    void initled(uint8_t *leds, int *Pinsq, int clock_pin, int latch_pin,clock_speed clock)
    {
        _clockspeed=clock;
       initled((uint8_t *)leds, Pinsq, clock_pin, latch_pin);
    }
       #endif
    void initled(uint8_t *leds, int *Pinsq, int clock_pin, int latch_pin)
    {
        this->leds = leds;
        this->saveleds = leds;
        initled(Pinsq, clock_pin, latch_pin);
    }

    void initled(int *Pinsq, int clock_pin, int latch_pin)
    {
        ESP_LOGI(TAG, "Start driver");
        driverInit = false;
        isOffsetDisplay = false;
        /*
        switch(cArr)
        {
            case ORDER_RGB:
                nb_components=3;
                p_r=0;
                p_g=1;
                p_b=2;
                break;
            case ORDER_RBG:
                nb_components=3;
                p_r=0;
                p_g=2;
                p_b=1;
                break;
            case ORDER_GRB:
                nb_components=3;
                p_r=1;
                p_g=0;
                p_b=2;
                break;
            case ORDER_GBR:
                nb_components=3;
                p_r=2;
                p_g=0;
                p_b=1;
                break;
            case ORDER_BRG:
                nb_components=3;
                p_r=1;
                p_g=2;
                p_b=0;
                break;
            case ORDER_BGR:
                nb_components=3;
                p_r=2;
                p_g=1;
                p_b=0;
                break;
            case ORDER_GRBW:
                nb_components=4;
                p_r=1;
                p_g=0;
                p_b=2;
                break;
        }
*/
        _gammab = 1;
        _gammar = 1;
        _gammag = 1;
        _gammaw = 1;
        setBrightness(255);
        startleds = 0;
        dmaBufferCount = __NB_DMA_BUFFER;
        // linewidth = NUM_LEDS_PER_STRIP;
        this->num_led_per_strip = NUM_LEDS_PER_STRIP;
        ESP_LOGD(TAG, "offset initiation");
        _offsetDisplay.offsetx = 0;
        _offsetDisplay.offsety = 0;
        _offsetDisplay.rotation = 0;
        _offsetDisplay._cos = 128;
        _offsetDisplay._sin = 0;
        _offsetDisplay.panel_width = _MAX_VALUE;
        _offsetDisplay.panel_height = _MAX_VALUE; // maximum
        _offsetDisplay.image_height = _MAX_VALUE;
        _offsetDisplay.image_width = _MAX_VALUE; // maximum
        _offsetDisplay.window_height = _MAX_VALUE;
        _offsetDisplay.window_width = _MAX_VALUE;
        _offsetDisplay._offx = 0; //_offsetDisplay.offsetx + 4 * _offsetDisplay.window_width;
        _offsetDisplay._offy = 0; // _offsetDisplay.offsety + 4 * _offsetDisplay.window_height;
        _offsetDisplay.enableLoopx = false;
        _offsetDisplay.enableLoopy = false;
        _offsetDisplay.enableRotation = false;
        _offsetDisplay.scaling = 1;
        _offsetDisplay.isOffsetDisplay = true;

        _defaultOffsetDisplay = _offsetDisplay;
        __defaultDisplayMode = WAIT;

        memset(firstPixel[0].bytes, 0, 16 * 8);
        memset(firstPixel[1].bytes, 0, 16 * 8);
        memset(firstPixel[2].bytes, 0, 16 * 8);
        #ifdef CONFIG_IDF_TARGET_ESP32S3

        firstPixel[0].bytes[NBIS2SERIALPINS] = 255;
        firstPixel[1].bytes[ NBIS2SERIALPINS] = 255;
        firstPixel[2].bytes[NBIS2SERIALPINS] = 255;
#if nb_components > 3
        firstPixel[3].bytes[NBIS2SERIALPINS] = 255;

#endif
#else
        firstPixel[0].bytes[16 + NBIS2SERIALPINS] = 255;
        firstPixel[1].bytes[16 + NBIS2SERIALPINS] = 255;
        firstPixel[2].bytes[16 + NBIS2SERIALPINS] = 255;
#if nb_components > 3
        firstPixel[3].bytes[16 + NBIS2SERIALPINS] = 255;
#endif
#endif
        // isRunOnCore=false;
        runCore = 3;
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY | I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)) > 0
        if (mapLed == NULL)
        {
            ESP_LOGD(TAG, "Using default mapping function");
            mapLed = __default__mapping;
            _offsetDisplay = _defaultOffsetDisplay;
        }

#endif
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY) > 0

        ESP_LOGD(TAG, "creating map array");
        _defaulthmap = (uint16_t *)malloc(NUM_LEDS_PER_STRIP * NBIS2SERIALPINS * 8 * 2 + 2);
        // _defaulthmap = (uint16_t *) heap_caps_malloc(NUM_LEDS_PER_STRIP * NBIS2SERIALPINS * 8 * 2 + 2,MALLOC_CAP_INTERNAL);
        if (!_defaulthmap)
        {
            Serial.printf("no memory\n");
        }
        else
        {

            ESP_LOGD(TAG, "calculate mapping");
            calculateDefaultMapping();
            // calculateOffsetDisplay(_defaultOffsetDisplay);
            // calculateMapping(_defaultOffsetDisplay);
            ESP_LOGD(TAG, " mapping done");
        }
#endif
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY) > 0
        ESP_LOGD(TAG, "create scroll mapping");
        _hmapscroll = (uint16_t *)malloc(NUM_LEDS_PER_STRIP * NBIS2SERIALPINS * 8 * 2 + 2);
        if (!_hmapscroll)
        {
            Serial.printf("no memory\n");
        }

#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_INTERRUPT_LINE) > 0
        resetInterruptsParameters();
#endif
        ESP_LOGD(TAG, "semaphore init");
        if (I2SClocklessVirtualLedDriver_waitDisp == NULL)
        {
            I2SClocklessVirtualLedDriver_waitDisp = xSemaphoreCreateCounting(10, 0);
        }
        // this->num_strips=num_strips;
        this->dmaBufferCount = dmaBufferCount;
        ESP_LOGD(TAG, "Pins initiation");
        setPins(Pinsq, clock_pin, latch_pin);
        ESP_LOGD(TAG, "I2S init");
        i2sInit();
        ESP_LOGD(TAG, "DMA initiation");
        initDMABuffers();
        ESP_LOGD(TAG, "End DMA initiation");
        driverInit = true;
        ESP_LOGI(TAG, "driver initiated");
    }

    void initled(frameBuffer *framb, int *Pinsq, int clock_pin, int latch_pin)
    {
        framebuff = framb;
        useFrame = true;
        ESP_LOGD(TAG, "Init leds with framebuffer");
        initled(framb->frames[0], Pinsq, clock_pin, latch_pin);
    }
    // private:
    volatile int dmaBufferActive = 0;
    volatile bool wait;
    displayMode __displayMode, __defaultDisplayMode;
    volatile int ledToDisplay;
    volatile int ledToDisplay_out;
    uint8_t *leds, *saveleds;

    int dmaBufferCount = 2; // we use two buffers
    volatile bool transpose = false;

    volatile int num_strips;
    volatile int num_led_per_strip;

    int brigthness;

    int i2s_base_pin_index;

    //I2SClocklessVirtualLedDriverDMABuffer **DMABuffersTransposed = NULL;
    // buffer array for the regular way
    #ifdef CONFIG_IDF_TARGET_ESP32S3
    typedef  dma_descriptor_t I2SClocklessVirtualLedDriverDMABuffer;
   #endif
    I2SClocklessVirtualLedDriverDMABuffer **DMABuffersTampon;//[__NB_DMA_BUFFER + 2];

    I2SClocklessVirtualLedDriverDMABuffer *allocateDMABuffer(int bytes)
    {
        I2SClocklessVirtualLedDriverDMABuffer *b = (I2SClocklessVirtualLedDriverDMABuffer *)heap_caps_malloc(sizeof(I2SClocklessVirtualLedDriverDMABuffer), MALLOC_CAP_DMA);
        if (!b)
        {
            ESP_LOGE(TAG, "No more memory\n");
            return NULL;
        }

        b->buffer = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_DMA);
        if (!b->buffer)
        {
            ESP_LOGE(TAG, "No more memory\n");
            return NULL;
        }
        memset(b->buffer, 0, bytes);
#ifdef CONFIG_IDF_TARGET_ESP32S3
 b->dw0.owner = DMA_DESCRIPTOR_BUFFER_OWNER_DMA;
  b->dw0.size = bytes;
                b->dw0.length = bytes;
                b->dw0.suc_eof = 1;

#else
        b->descriptor.length = bytes;
        b->descriptor.size = bytes;
        b->descriptor.owner = 1;
        b->descriptor.sosf = 1;
        b->descriptor.buf = b->buffer;
        b->descriptor.offset = 0;
        b->descriptor.empty = 0;
        b->descriptor.eof = 1;
        b->descriptor.qe.stqe_next = 0;

        #endif

        return b;
    }

    void i2sReset_DMA()
    {
        #ifndef CONFIG_IDF_TARGET_ESP32S3

        (&I2S0)->lc_conf.out_rst = 1;
        (&I2S0)->lc_conf.out_rst = 0;
        #endif
    }

    void i2sReset_FIFO()
    {
 #ifndef CONFIG_IDF_TARGET_ESP32S3
        (&I2S0)->conf.tx_fifo_reset = 1;
        (&I2S0)->conf.tx_fifo_reset = 0;
        #endif
    }
    /*
        static void IRAM_ATTR i2sStop(I2SClocklessVirtualLedDriver *cont)
        {

            // delay(1);
            esp_intr_disable(cont->_gI2SClocklessDriver_intr_handle);
            ets_delay_us(16);
            (&I2S0)->conf.tx_start = 0;
            while ((&I2S0)->conf.tx_start == 1)
            {
            }

            cont->i2sReset();

            cont->isDisplaying = false;
            // cont->leds=cont->saveleds;
            //         We have finished to display the strips

            // ets_delay_us(1000);
            if (cont->wasWaitingtofinish == true) // and cont->__displayMode==NO_WAIT
            {
                cont->wasWaitingtofinish = false;
                xSemaphoreGive(cont->I2SClocklessVirtualLedDriver_waitDisp);
            }
            if (cont->isWaiting)
            {
                // printf("on debloqu\n");
                xSemaphoreGive(cont->I2SClocklessVirtualLedDriver_sem);
            }
            // printf("hehe\n");
        }
    */
    void putdefaultlatch(uint16_t *buff)
    {
        // printf("dd%d\n",NBIS2SERIALPINS);
        uint16_t mask1 = 1 << NBIS2SERIALPINS;
        for (int i = 0; i < 24 * nb_components; i++)
        {
            #ifdef CONFIG_IDF_TARGET_ESP32S3
             buff[ i * (NUM_VIRT_PINS + 1) ]= mask1;
            #else
            buff[NUM_VIRT_PINS + i * (NUM_VIRT_PINS + 1) - 1 - 5 + DELTA_OFFSET_LATCH] = mask1; // 0x8000;
            #endif
            // buff[NUM_VIRT_PINS+i*(NUM_VIRT_PINS+1)]=0x02;
        }
    }

    void putdefaultones(uint16_t *buff)
    {

        uint16_t mas = 0xFFFF & (~(0xffff << (NBIS2SERIALPINS)));
        // printf("mas%d\n",mas);
        for (int j = 0; j < 8 * nb_components; j++)
        {
 #ifdef CONFIG_IDF_TARGET_ESP32S3
             buff[0 + j * (3 * (NUM_VIRT_PINS + 1))] = 0xFFFF;
            buff[1 + j * (3 * (NUM_VIRT_PINS + 1))] = mas;
 #else
            buff[1 + j * (3 * (NUM_VIRT_PINS + 1))] = 0xFFFF;
            buff[0 + j * (3 * (NUM_VIRT_PINS + 1))] = mas;
            #endif
            buff[3 + j * (3 * (NUM_VIRT_PINS + 1))] = mas;
            buff[2 + j * (3 * (NUM_VIRT_PINS + 1))] = mas;
            buff[5 + j * (3 * (NUM_VIRT_PINS + 1))] = mas;
            buff[4 + j * (3 * (NUM_VIRT_PINS + 1))] = mas;
            buff[7 + j * (3 * (NUM_VIRT_PINS + 1))] = mas;
            buff[6 + j * (3 * (NUM_VIRT_PINS + 1))] = mas;
        }
    }

    /*
     Transpose the pixel, as the function is static and all the variables are not static or global, we need to provide all of them.
     */

    void i2sStart(I2SClocklessVirtualLedDriverDMABuffer *startBuffer)
    {

               framesync = false;
        counti = 0;
#ifdef CONFIG_IDF_TARGET_ESP32S3




    LCD_CAM.lcd_user.lcd_start=0;
gdma_reset(dma_chan);  
LCD_CAM.lcd_user.lcd_dout = 1;        // Enable data out
    LCD_CAM.lcd_user.lcd_update = 1;      // Update registers
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
    
//    memset(startBuffer->buffer,0,WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE);
    gdma_start(dma_chan, (intptr_t)startBuffer); // Start DMA w/updated descriptor(s)
    // esp_intr_enable(dma_chan->intr);
    //vTaskDelay(1);                         // Must 'bake' a moment before...
    LCD_CAM.lcd_user.lcd_start = 1;  
     #else   
      i2sReset();
        framesync = false;
        counti = 0;

        (&I2S0)->lc_conf.val = I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN | I2S_OUT_DATA_BURST_EN;

        (&I2S0)->out_link.addr = (uint32_t) & (startBuffer->descriptor);

        (&I2S0)->out_link.start = 1;

        (&I2S0)->int_clr.val = (&I2S0)->int_raw.val;

        (&I2S0)->int_clr.val = (&I2S0)->int_raw.val;
        (&I2S0)->int_ena.val = 0;

        /*
         If we do not use the regular showpixels, then no need to activate the interupt at the end of each pixels
         */
        // if(transpose)
        (&I2S0)->int_ena.out_eof = 1;

        (&I2S0)->int_ena.out_total_eof = 1;
        esp_intr_enable(_gI2SClocklessDriver_intr_handle);

        // We start the I2S
        (&I2S0)->conf.tx_start = 1;

#endif
        // Set the mode to indicate that we've started
        isDisplaying = true;
    }
    /*
        void i2sReset()
        {
            const unsigned long lc_conf_reset_flags = I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
            (&I2S0)->lc_conf.val |= lc_conf_reset_flags;
            (&I2S0)->lc_conf.val &= ~lc_conf_reset_flags;
            const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M | I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
            (&I2S0)->conf.val |= conf_reset_flags;
            (&I2S0)->conf.val &= ~conf_reset_flags;
        }
    */
    // static void IRAM_ATTR interruptHandler(void *arg);
};

static void IRAM_ATTR i2sStop(I2SClocklessVirtualLedDriver *cont)
{
#ifdef CONFIG_IDF_TARGET_ESP32S3

//gdma_disconnect(dma_chan);
LCD_CAM.lcd_user.lcd_start=0;

        while( LCD_CAM.lcd_user.lcd_start)
        {}
        gdma_stop(dma_chan);
       // esp_intr_disable(dma_chan->intr);
#else

    // delay(1);
    esp_intr_disable(cont->_gI2SClocklessDriver_intr_handle);
    ets_delay_us(16);
    (&I2S0)->conf.tx_start = 0;
    while ((&I2S0)->conf.tx_start == 1)
    {
    }
#endif
    // cont->i2sReset();
    i2sReset();
    cont->isDisplaying = false;
    // cont->leds=cont->saveleds;
    /*
     We have finished to display the strips
     */
    // ets_delay_us(1000);
    if (cont->wasWaitingtofinish == true) // and cont->__displayMode==NO_WAIT
    {
        cont->wasWaitingtofinish = false;
        xSemaphoreGive(cont->I2SClocklessVirtualLedDriver_waitDisp);
    }
    if (cont->isWaiting)
    {
        // printf("on debloqu\n");
        xSemaphoreGive(cont->I2SClocklessVirtualLedDriver_sem);
    }
    // printf("hehe\n");
}
#ifdef CONFIG_IDF_TARGET_ESP32S3
static IRAM_ATTR bool _I2SClocklessVirtualLedDriverinterruptHandler(gdma_channel_handle_t dma_chan,
                                   gdma_event_data_t *event_data, void *user_data)
{
    // This DMA callback seems to trigger a moment before the last data has
    // issued (buffering between DMA & LCD peripheral?), so pause a moment
    // before stopping LCD data out. The ideal delay may depend on the LCD
    // clock rate...this one was determined empirically by monitoring on a
    // logic analyzer. YMMV.
    //vTaskDelay(100);
    // The LCD peripheral stops transmitting at the end of the DMA xfer, but
    // clear the lcd_start flag anyway -- we poll it in loop() to decide when
    // the transfer has finished, and the same flag is set later to trigger
    // the next transfer.
 I2SClocklessVirtualLedDriver *cont = (I2SClocklessVirtualLedDriver *)user_data;

    if (!cont->__enableDriver)
    {
        // cont->i2sStop(cont);
        i2sStop(cont);
        return true;
    }

        cont->framesync = !cont->framesync;

        // cont->ledToDisplay_in[cont->ledToDisplay_out]=cont->ledToDisplay+1;
        // cont->ledToDisplay_inbuffer[cont->ledToDisplay_out]=cont->dmaBufferActive;

        if (cont->transpose)
        {
            cont->ledToDisplay = cont->ledToDisplay + 1;
            if (cont->ledToDisplay < cont->num_led_per_strip)
            {

                loadAndTranspose(cont);

                if (cont->ledToDisplay_out == (cont->num_led_per_strip - (__NB_DMA_BUFFER))) // here it's not -1 because it takes time top have the change into account and it reread the buufer
                {
                    cont->DMABuffersTampon[(cont->dmaBufferActive) % __NB_DMA_BUFFER]->next = (cont->DMABuffersTampon[__NB_DMA_BUFFER + 1]);
                    // cont->ledToDisplay_inbufferfor[cont->ledToDisplay_out]=cont->dmaBufferActive;
                }

                cont->dmaBufferActive = (cont->dmaBufferActive + 1) % __NB_DMA_BUFFER;
            }
           cont->ledToDisplay_out = cont->ledToDisplay_out + 1;
                if( cont->ledToDisplay>=NUM_LEDS_PER_STRIP+__NB_DMA_BUFFER-1)
     {
 

                     i2sStop(cont);
     }
        }
        else
        {
            if (cont->framesync)
            {
                portBASE_TYPE HPTaskAwoken = 0;
                xSemaphoreGiveFromISR(cont->I2SClocklessVirtualLedDriver_semSync, &HPTaskAwoken);
                if (HPTaskAwoken == pdTRUE)
                    portYIELD_FROM_ISR();
            }
        }
return true;
}
#else
static void IRAM_ATTR _I2SClocklessVirtualLedDriverinterruptHandler(void *arg)
{

    // REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG( 0 )) & 0xffffffc0) | 0x3f);
    // return;
    I2SClocklessVirtualLedDriver *cont = (I2SClocklessVirtualLedDriver *)arg;

if(!cont->__enableDriver)
{
     REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
    i2sStop(cont);
     
     return;
}
    if (GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_EOF_INT_ST_S, I2S_OUT_EOF_INT_ST_S))
    {
        cont->framesync = !cont->framesync;

        // cont->ledToDisplay_in[cont->ledToDisplay_out]=cont->ledToDisplay+1;
        // cont->ledToDisplay_inbuffer[cont->ledToDisplay_out]=cont->dmaBufferActive;

        if (((I2SClocklessVirtualLedDriver *)arg)->transpose)
        {
            cont->ledToDisplay++;
            if (cont->ledToDisplay < cont->num_led_per_strip)
            {

                loadAndTranspose(cont);

                if (cont->ledToDisplay_out == (cont->num_led_per_strip - (__NB_DMA_BUFFER))) // here it's not -1 because it takes time top have the change into account and it reread the buufer
                {
                    cont->DMABuffersTampon[(cont->dmaBufferActive) % __NB_DMA_BUFFER]->descriptor.qe.stqe_next = &(cont->DMABuffersTampon[__NB_DMA_BUFFER + 1]->descriptor);
                    // cont->ledToDisplay_inbufferfor[cont->ledToDisplay_out]=cont->dmaBufferActive;
                }

                cont->dmaBufferActive = (cont->dmaBufferActive + 1) % __NB_DMA_BUFFER;
            }
            cont->ledToDisplay_out++;
        }
        else
        {
            if (cont->framesync)
            {
                portBASE_TYPE HPTaskAwoken = 0;
                xSemaphoreGiveFromISR(cont->I2SClocklessVirtualLedDriver_semSync, &HPTaskAwoken);
                if (HPTaskAwoken == pdTRUE)
                    portYIELD_FROM_ISR();
            }
        }
    }

    if (GET_PERI_REG_BITS(I2S_INT_ST_REG(I2S_DEVICE), I2S_OUT_TOTAL_EOF_INT_ST_S, I2S_OUT_TOTAL_EOF_INT_ST_S))
    {

        // cont->ledToDisplay_inbufferfor[cont->ledToDisplay_out]=9999;
        i2sStop(cont);
    }

    REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
}
#endif

static inline __attribute__((always_inline)) void IRAM_ATTR transpose16x1_noinline2(unsigned char *A, uint8_t *B)
{

    uint32_t x, y, t;
#if NBIS2SERIALPINS >= 8
    uint32_t x1, y1;
#endif
#if NBIS2SERIALPINS >= 4
    uint32_t ff;
#endif
    uint32_t aa, cc;
    uint32_t ff2;
    aa = AAA;
    cc = CCC;
#if NBIS2SERIALPINS >= 4
    ff = FFF;
#endif
    ff2 = FFF2;
    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    //******
    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    //*************

    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    //******
    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    B += 2;
    A += 16;

    y = *(unsigned int *)(A);
#if NBIS2SERIALPINS >= 4
    x = *(unsigned int *)(A + 4);
#else
    x = 0;
#endif
#if NBIS2SERIALPINS >= 8
    y1 = *(unsigned int *)(A + 8);
#if NBIS2SERIALPINS >= 12
    x1 = *(unsigned int *)(A + 12);
#else
    x1 = 0;
#endif

#endif

    // pre-transform x
#if NBIS2SERIALPINS >= 4
    t = (x ^ (x >> 7)) & aa;
    x = x ^ t ^ (t << 7);
    t = (x ^ (x >> 14)) & cc;
    x = x ^ t ^ (t << 14);
#endif
#if NBIS2SERIALPINS >= 12
    t = (x1 ^ (x1 >> 7)) & aa;
    x1 = x1 ^ t ^ (t << 7);
    t = (x1 ^ (x1 >> 14)) & cc;
    x1 = x1 ^ t ^ (t << 14);
#endif
    // pre-transform y
    t = (y ^ (y >> 7)) & aa;
    y = y ^ t ^ (t << 7);
    t = (y ^ (y >> 14)) & cc;
    y = y ^ t ^ (t << 14);

#if NBIS2SERIALPINS >= 8
    t = (y1 ^ (y1 >> 7)) & aa;
    y1 = y1 ^ t ^ (t << 7);
    t = (y1 ^ (y1 >> 14)) & cc;
    y1 = y1 ^ t ^ (t << 14);
#endif

    // final transform
#if NBIS2SERIALPINS >= 4
    t = (x & ff) | ((y >> 4) & ff2);

    y = ((x << 4) & ff) | (y & ff2);

    x = t;
#else
    x = ((y >> 4) & ff2);
    y = (y & ff2);

#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if NBIS2SERIALPINS >= 12
    t = (x1 & ff) | ((y1 >> 4) & ff2);
    y1 = ((x1 << 4) & ff) | (y1 & ff2);
    x1 = t;
#else
    x1 = ((y1 >> 4) & ff2);
    y1 = (y1 & ff2);
#endif
#endif

#if NBIS2SERIALPINS >= 8

#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
#endif

#else
#if __MAX_BRIGTHNESS >= 128
#if _BRIGHTNES_8 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_8)) = (uint16_t)((x) >> 24);
#endif
#endif
#if __MAX_BRIGTHNESS >= 64
#if _BRIGHTNES_7 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_7)) = (uint16_t)((x) >> 16);
#endif
#endif
#if __MAX_BRIGTHNESS >= 32
#if _BRIGHTNES_6 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_6)) = (uint16_t)((x) >> 8);
#endif
#endif
#if __MAX_BRIGTHNESS >= 16
#if _BRIGHTNES_5 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_5)) = (uint16_t)(x);
#endif
#endif
#if __MAX_BRIGTHNESS >= 8
#if _BRIGHTNES_4 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_4)) = (uint16_t)((y) >> 24);
#endif
#endif
#if _BRIGHTNES_3 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_3)) = (uint16_t)((y) >> 16);
#endif
#if _BRIGHTNES_2 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_2)) = (uint16_t)((y) >> 8);
#endif
#if _BRIGHTNES_1 < (8 * 48)
    *((uint16_t *)(B + _BRIGHTNES_1)) = (uint16_t)(y);
#endif

#endif
    //  B+=370;
    // A+=16;
    // }
}

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_DIRECT_CALCULATION) == 0
static inline __attribute__((always_inline)) void IRAM_ATTR loadAndTranspose(I2SClocklessVirtualLedDriver *driver)
{

#if CORE_DEBUG_LEVEL >= 5
    driver->_times[driver->ledToDisplay] = ESP.getCycleCount();
#endif
    uint8_t *ledt = driver->leds;
    uint16_t *buff = (uint16_t *)driver->DMABuffersTampon[driver->dmaBufferActive]->buffer;
#if ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE) or ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE) or ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_NONE)
    int ledtodisp = driver->ledToDisplay;
#endif
#ifndef __HARDWARE_BRIGHTNESS
    uint8_t *mapg = driver->__green_map;
    uint8_t *mapr = driver->__red_map;
    uint8_t *mapb = driver->__blue_map;
#endif
#ifdef _USE_PALETTE
    uint8_t *palette = driver->palette;
#endif
#if nb_components > 3
    uint8_t *mapw = driver->__white_map;
#endif
#if STATICCOLOR == 0
    uint8_t *r_map = driver->r_map;
    uint8_t *g_map = driver->g_map;
    uint8_t *b_map = driver->b_map;
#endif
    Lines *firstPixel = driver->firstPixel;

    uint8_t *poli_b, *poli;
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_NONE) > 0
    uint8_t *_poli;
#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_NONE) > 0

    _poli = ledt + ledtodisp * _palette_size;
#else

#if ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE) or ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)
    uint16_t _led_tmp = ledtodisp;
    // #if !((I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY) > 0)
    uint16_t led_tmp;
    // #endif
#endif

#endif
    buff += OFFSET;
    // jump en deux

    //.for()

    for (int pin74HC595 = 0; pin74HC595 < 8; pin74HC595++)
    {

#if ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE) or ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)
        led_tmp = _led_tmp;
#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_NONE) > 0
        poli = _poli;
#endif
#if (I2S_MAPPING_MODE & (I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY | I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY)) > 0
#ifdef CONFIG_IDF_TARGET_ESP32S3
        int pin = (pin74HC595 ^1) << 4;
#else
 int pin = (pin74HC595) << 4;
#endif
#else
       #ifdef CONFIG_IDF_TARGET_ESP32S3
        int pin = (pin74HC595 ) << 4;
#else
 int pin = (pin74HC595^1) << 4;
#endif
#endif
        for (int vpin = 0; vpin < NBIS2SERIALPINS; vpin++)
        {
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE
            poli = ledt + driver->mapLed(led_tmp) * _palette_size;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY_SOFTWARE
            poli = ledt + driver->remapStatic() * _palette_size;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_ALL_IN_MEMORY
            poli = ledt + *(driver->_hmapoff);
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY
            poli = ledt + *(driver->_hmapoff);
#endif

#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE
            uint16_t val;
            driver->_hmapoff = &val;
            val = driver->mapLed(led_tmp);
            poli = ledt + driver->remapStatic() * _palette_size;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_IN_MEMORY
            poli = ledt + *(driver->_hmapoff);
#endif

#ifdef _USE_PALETTE
            poli_b = palette + *(poli)*nb_components;
#else
            poli_b = poli;
#endif

#if STATICCOLOR == 1
#ifndef __HARDWARE_BRIGHTNESS
            firstPixel[p_g].bytes[pin + vpin] = mapg[*(poli_b + 1)];
            firstPixel[p_r].bytes[pin + vpin] = mapr[*(poli_b)];
            firstPixel[p_b].bytes[pin + vpin] = mapb[*(poli_b + 2)];
#else

            firstPixel[p_g].bytes[pin + vpin] = *(poli_b + 1);
            firstPixel[p_r].bytes[pin + vpin] = *(poli_b);
            firstPixel[p_b].bytes[pin + vpin] = *(poli_b + 2);

#endif

#else
#ifndef __HARDWARE_BRIGHTNESS
            firstPixel[g_map[8 * vpin]].bytes[pin + vpin] = mapg[*(poli_b + 1)];
            firstPixel[r_map[8 * vpin]].bytes[pin + vpin] = mapr[*(poli_b)];
            firstPixel[b_map[8 * vpin]].bytes[pin + vpin] = mapb[*(poli_b + 2)];
#else
            firstPixel[g_map[8 * vpin]].bytes[pin + vpin] = *(poli_b + 1);
            firstPixel[r_map[8 * vpin]].bytes[pin + vpin] = *(poli_b);
            firstPixel[b_map[8 * vpin]].bytes[pin + vpin] = *(poli_b + 2);
#endif
#endif
#if nb_components > 3
            firstPixel[3].bytes[pin + vpin] = mapw[*(poli_b + 3)];
#endif

            // pin++;
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE
            led_tmp += I2S_OFF_MAP;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE
            led_tmp += I2S_OFF_MAP;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY
            driver->_hmapoff++;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY_SOFTWARE
            driver->_hmapoff++;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_ALL_IN_MEMORY
            driver->_hmapoff++;
#endif
#if (I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_IN_MEMORY
            driver->_hmapoff++;
#endif

#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_NONE) > 0
            poli += I2S_OFF;
#endif
        }
#if ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE) or ((I2S_MAPPING_MODE & 0xFFF) == I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)
        _led_tmp += NUM_LEDS_PER_STRIP;
#endif
#if (I2S_MAPPING_MODE & I2S_MAPPING_MODE_OPTION_NONE) > 0
        _poli += NUM_LEDS_PER_STRIP * _palette_size;
#endif
    }

    transpose16x1_noinline2(firstPixel[0].bytes, (uint8_t *)(buff));

    transpose16x1_noinline2(firstPixel[1].bytes, (uint8_t *)(buff + 192));
    transpose16x1_noinline2(firstPixel[2].bytes, (uint8_t *)(buff + 384));
#if nb_components > 3
    transpose16x1_noinline2(firstPixel[3].bytes, (uint8_t *)(buff + 576));
#endif

#if CORE_DEBUG_LEVEL >= 5
    driver->_times[driver->ledToDisplay] = ESP.getCycleCount() - driver->_times[driver->ledToDisplay];
#endif
}

#else // function instead of reading memory
static inline __attribute__((always_inline)) void IRAM_ATTR loadAndTranspose(I2SClocklessVirtualLedDriver *driver)
{
#if CORE_DEBUG_LEVEL >= 5
    driver->_times[driver->ledToDisplay] = ESP.getCycleCount();
#endif
    // int led_tmp;
    // uint8_t *ledt = driver->leds;
    uint16_t *buff = (uint16_t *)driver->DMABuffersTampon[driver->dmaBufferActive]->buffer;
    int ledtodisp = driver->ledToDisplay;
#ifndef __HARDWARE_BRIGHTNESS
    uint8_t *mapg = driver->__green_map;
    uint8_t *mapr = driver->__red_map;
    uint8_t *mapb = driver->__blue_map;
#endif
    Lines *firstPixel = driver->firstPixel;
#if STATICCOLOR == 0
    uint8_t *r_map = driver->r_map;
    uint8_t *g_map = driver->g_map;
    uint8_t *b_map = driver->b_map;
#endif
#ifdef _USE_PALETTE
    uint8_t *palette = driver->palette;
#endif
    buff += OFFSET;

    for (int pin74HC595 = 0; pin74HC595 < 8; pin74HC595++)
    {

        int pin = (pin74HC595 ^ 1) << 4;
        int vPin = pin74HC595 << 4;
        for (int pinEsp32 = 0; pinEsp32 < NBIS2SERIALPINS; pinEsp32++)
        {
#ifdef _USE_PALETTE
            Pixel p = *(Pixel *)(palette + driver->pixelCalc(ledtodisp, pinEsp32, vPin) * nb_components);
#else
            Pixel p = driver->pixelCalc(ledtodisp, pinEsp32, vPin);
#endif
#if STATICCOLOR == 1
#ifndef __HARDWARE_BRIGHTNESS
            firstPixel[p_g].bytes[pin + pinEsp32] = mapg[p.raw[1]];
            firstPixel[p_r].bytes[pin + pinEsp32] = mapr[p.raw[0]];
            firstPixel[p_b].bytes[pin + pinEsp32] = mapb[p.raw[2]];
#else
            firstPixel[p_g].bytes[pin + pinEsp32] = p.raw[1];
            firstPixel[p_r].bytes[pin + pinEsp32] = p.raw[0];
            firstPixel[p_b].bytes[pin + pinEsp32] = p.raw[2];
#endif

#else
#ifndef __HARDWARE_BRIGHTNESS
            firstPixel[g_map[8 * pinEsp32]].bytes[pin + pinEsp32] = mapg[p.raw[1]];
            firstPixel[r_map[8 * pinEsp32]].bytes[pin + pinEsp32] = mapr[p.raw[0]];
            firstPixel[b_map[8 * pinEsp32]].bytes[pin + pinEsp32] = mapb[p.raw[2]];
#else
            firstPixel[g_map[8 * pinEsp32]].bytes[pin + pinEsp32] = p.raw[1];
            firstPixel[r_map[8 * pinEsp32]].bytes[pin + pinEsp32] = p.raw[0];
            firstPixel[b_map[8 * pinEsp32]].bytes[pin + pinEsp32] = p.raw[2];
#endif
#endif
        }
    }

    transpose16x1_noinline2(firstPixel[0].bytes, (uint8_t *)(buff));

    transpose16x1_noinline2(firstPixel[1].bytes, (uint8_t *)(buff + 192));
    transpose16x1_noinline2(firstPixel[2].bytes, (uint8_t *)(buff + 384));
#if nb_components > 3
    transpose16x1_noinline2(firstPixel[3].bytes, (uint8_t *)(buff + 576));
#endif
#if CORE_DEBUG_LEVEL >= 5
    driver->_times[driver->ledToDisplay] = ESP.getCycleCount() - driver->_times[driver->ledToDisplay];
#endif
}

#endif

static void showPixelsTask(void *pvParameters)
{
     I2SClocklessVirtualLedDriver *cont = (I2SClocklessVirtualLedDriver *)pvParameters;
    #ifndef CONFIG_IDF_TARGET_ESP32S3
   
    if (cont->_gI2SClocklessDriver_intr_handle != NULL)
    {
        esp_intr_free(cont->_gI2SClocklessDriver_intr_handle);
    }
    ESP_LOGV(TAG, "setting interupt handler");
    esp_err_t e = esp_intr_alloc(interruptSource, ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM, &_I2SClocklessVirtualLedDriverinterruptHandler, cont, &cont->_gI2SClocklessDriver_intr_handle);
    if (e != ESP_OK)
    {
        ESP_LOGE(TAG, "Impossible to create interupt allocation");
        return;
    }
    ESP_LOGV(TAG, "interupt handler set on core %d", xPortGetCoreID());

    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        cont->___showPixels();
    }
    #else
    for (;;)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        cont->___showPixels();
    }
    #endif

}

} // namespace I2S
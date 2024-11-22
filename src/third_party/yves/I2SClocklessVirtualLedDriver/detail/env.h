
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
#include "../helper.h"
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
}  // namespace fl

#endif
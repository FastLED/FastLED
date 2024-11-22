

#include "esp_heap_caps.h"
#include "freertos/semphr.h"
#include <stdio.h> // ok include

#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
#include "esp_private/periph_ctrl.h"
#else
#include "driver/periph_ctrl.h"
#endif

#include <soc/gdma_channel.h>
// #include "esp_private/periph_ctrl.h"
// #include "gdma_priv.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/gpio_ll.h"
#include <esp_private/gdma.h>
#include <hal/dma_types.h>
#include <hal/gdma_types.h>
#include <hal/gpio_hal.h>
#include <soc/lcd_cam_struct.h>
#include <stdbool.h>

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
#define CLOCK_DIV_A 4
#define CLOCK_DIV_B 1
#endif

namespace fl {
typedef struct gdma_pair_t gdma_pair_t;

struct gdma_channel_t {
    gdma_pair_t *pair;                  // which pair the channel belongs to
    intr_handle_t intr;                 // per-channel interrupt handle
    portMUX_TYPE spinlock;              // channel level spinlock
    gdma_channel_direction_t direction; // channel direction
    int periph_id; // Peripheral instance ID, indicates which peripheral is
                   // connected to this GDMA channel
    size_t int_mem_alignment; // alignment for memory in internal memory
    size_t ext_mem_alignment; // alignment for memory in external memory
    esp_err_t (*del)(
        gdma_channel_t
            *channel); // channel deletion function, it's polymorphic, see
                       // `gdma_del_tx_channel` or `gdma_del_rx_channel`
    struct {
        uint32_t start_stop_by_etm : 1; // whether the channel is
                                        // started/stopped by ETM
    } flags;
};

typedef struct {
    int div_num;
    int div_a;
    int div_b;
} clock_speed;

clock_speed clock_1123KHZ = {4, 20, 9};
clock_speed clock_1111KHZ = {4, 2, 1};
clock_speed clock_1000KHZ = {5, 1, 0};
clock_speed clock_800KHZ = {6, 4, 1};

} // namespace fl

#define WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE (576 * 2)

static uint8_t signalsID[16] = {
    LCD_DATA_OUT0_IDX,  LCD_DATA_OUT1_IDX,  LCD_DATA_OUT2_IDX,
    LCD_DATA_OUT3_IDX,  LCD_DATA_OUT4_IDX,  LCD_DATA_OUT5_IDX,
    LCD_DATA_OUT6_IDX,  LCD_DATA_OUT7_IDX,  LCD_DATA_OUT8_IDX,
    LCD_DATA_OUT9_IDX,  LCD_DATA_OUT10_IDX, LCD_DATA_OUT11_IDX,
    LCD_DATA_OUT12_IDX, LCD_DATA_OUT13_IDX, LCD_DATA_OUT14_IDX,
    LCD_DATA_OUT15_IDX,
};
static gdma_channel_handle_t dma_chan;

static void IRAM_ATTR i2sReset() {
    gdma_reset(dma_chan);
    LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
}
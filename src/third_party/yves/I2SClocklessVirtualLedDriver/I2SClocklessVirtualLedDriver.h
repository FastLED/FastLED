
/*

 */

#pragma once

#include "detail/env.h"

#include "___pixeltypes.h"



#include "framebuffer.h"
#include "detail/transpose.h"

namespace fl {

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
} // namespace fl


#include "detail/driver.h"
#include "detail/after_driver.h"
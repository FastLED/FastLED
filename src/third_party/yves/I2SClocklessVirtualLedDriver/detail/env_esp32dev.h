#pragma once

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


static void IRAM_ATTR i2sReset()
{
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
}
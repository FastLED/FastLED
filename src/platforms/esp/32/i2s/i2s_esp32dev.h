#pragma once
#include "fl/stdint.h"

#include "fl/namespace.h"



#ifdef __cplusplus
extern "C" {
#endif

#include "platforms/esp/esp_version.h"

#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/io_mux_reg.h"
#include "soc/soc.h"

#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR >= 5
#include "esp_private/periph_ctrl.h"
#else
#include "driver/periph_ctrl.h"
#endif

#include "esp_system.h" // Load ESP_IDF_VERSION_MAJOR if exists
#include "rom/lldesc.h"
// ESP_IDF_VERSION_MAJOR is defined in ESP-IDF v3.3 or later
#if defined(ESP_IDF_VERSION_MAJOR) && ESP_IDF_VERSION_MAJOR > 3
#include "esp_intr_alloc.h"
#else
#include "esp_intr.h"
#endif
#include "platforms/esp/32/esp_log_control.h"  // Control ESP logging before including esp_log.h
#include "esp_log.h"

// esp include for F_CPU
#include "esp32-hal.h"

#ifndef F_CPU_MHZ
#define F_CPU_MHZ (F_CPU / 1000000L)
#endif

#ifdef __cplusplus
}
#endif


// TODO: this is in like 2 places. Consolidate.
// override default NUM_DMA_BUFFERS if FASTLED_ESP32_I2S_NUM_DMA_BUFFERS
// is defined and has a valid value
#if FASTLED_ESP32_I2S_NUM_DMA_BUFFERS > 2
#if FASTLED_ESP32_I2S_NUM_DMA_BUFFERS > 16
#error invalid value for FASTLED_ESP32_I2S_NUM_DMA_BUFFERS
#endif
#define NUM_DMA_BUFFERS FASTLED_ESP32_I2S_NUM_DMA_BUFFERS
// for counting DMA buffers currently in use
#else
#define NUM_DMA_BUFFERS 2
#endif


#define NUM_COLOR_CHANNELS 3


FASTLED_NAMESPACE_BEGIN


struct DMABuffer {
    lldesc_t descriptor;
    uint8_t *buffer;
};
extern int gCntBuffer;
extern int gCurBuffer;
extern bool gDoneFilling;
extern uint8_t gPixelRow[NUM_COLOR_CHANNELS][32];
extern uint8_t gPixelBits[NUM_COLOR_CHANNELS][8][4];
extern DMABuffer *dmaBuffers[NUM_DMA_BUFFERS];;

// typedef for a void function pointer
typedef void (*void_func_t)(void);
void i2s_set_fill_buffer_callback(void_func_t callback);

int pgcd(int smallest, int precision, int a, int b, int c);

/** Compute pules/bit patterns
 *
 *  This is Yves Bazin's mad code for computing the pulse pattern
 *  and clock timing given the target signal given by T1, T2, and
 *  T3. In general, these parameters are interpreted as follows:
 *
 *  a "1" bit is encoded by setting the pin HIGH to T1+T2 ns, then LOW for T3 ns
 *  a "0" bit is encoded by setting the pin HIGH to T1 ns, then LOW for T2+T3 ns
 *
 */
void i2s_define_bit_patterns(int T1, int T2, int T3);
bool i2s_is_initialized();
void i2s_init(int i2s_device);
void i2s_clear_dma_buffer(uint32_t *buf);  // warning, this function assumes length.
void i2s_start();
void i2s_reset();
void i2s_reset_dma();
void i2s_reset_fifo();
void i2s_stop();
void i2s_begin();
void i2s_wait();
void i2s_setup_pin(int pin, int offset);
void i2s_transpose_and_encode(int channel, uint32_t has_data_mask, volatile uint32_t *buf);

FASTLED_NAMESPACE_END

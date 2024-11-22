
#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_IDF_TARGET_ESP32S3
#include "env_s3.h"
#else
#include "env_esp32dev.h"
#endif

#include "../helper.h"
#include "math.h"

#define I2S_DEVICE 0

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

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
#define I2S_OFF2                                                               \
    ((I2S_OFF * NBIS2SERIALPINS - NUM_LEDS_PER_STRIP * _palette_size))
#define I2S_OFF3                                                               \
    ((I2S_OFF * NBIS2SERIALPINS + NUM_LEDS_PER_STRIP * _palette_size))
#define I2S_OFF4                                                               \
    ((I2S_OFF * NBIS2SERIALPINS - 3 * NUM_LEDS_PER_STRIP * _palette_size))
#define I2S_OFF_MAP (((NUM_VIRT_PINS + 1) * NUM_LEDS_PER_STRIP))
#define I2S_OFF2_MAP ((I2S_OFF_MAP * NBIS2SERIALPINS - NUM_LEDS_PER_STRIP))
#define I2S_OFF3_MAP ((I2S_OFF_MAP * NBIS2SERIALPINS + NUM_LEDS_PER_STRIP))
#define I2S_OFF4_MAP ((I2S_OFF_MAP * NBIS2SERIALPINS - 3 * NUM_LEDS_PER_STRIP))
#define BUFFOFF ((NBIS2SERIALPINS * 8) - 1)
#define AAA (0x00AA00AAL)
#define CCC (0x0000CCCCL)
#define FFF (0xF0F0F0F0L)
#define FFF2 (0xF0F0F0FL)

// this below is only the the ws281X if you need more time then you are able to
// extend the size of the buffer
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

#define _NB_BIT                                                                \
    (_DMA_EXTENSTION * 2 + (NUM_VIRT_PINS + 1) * nb_components * 8 * 3)
#define _BUFFER_TIMING ((_NB_BIT / 19.2) - 4)
#define _BASE_BUFFER_TIMING                                                    \
    (((NUM_VIRT_PINS + 1) * nb_components * 8 * 3 / 19.2) - 4)

#define I2S_DRIVER_MODE_OPTION_VIRTUAL_PIN 0x100
#define I2S_DRIVER_MODE_OPTION_CLOCKLESS 0x200
#define I2S_DRIVER_MODE_OPTION_CLOCKBASED 0x400
#define I2S_MAPPING_MODE_OPTION_NONE 0x1
#define I2S_MAPPING_MODE_OPTION_DIRECT_CALCULATION 0x2
#define I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY 0x4
#define I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE 0x8
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE 0x20
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY 0x10
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_ALL_IN_MEMORY                   \
    (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY |                        \
     I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY)
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY_SOFTWARE              \
    (I2S_MAPPING_MODE_OPTION_MAPPING_IN_MEMORY |                               \
     I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE)
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_SOFTWARE               \
    (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE |                         \
     I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)
#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_SOFTWARE_IN_MEMORY              \
    (I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_IN_MEMORY |                        \
     I2S_MAPPING_MODE_OPTION_MAPPING_SOFTWARE)

#define I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING                                 \
    I2S_MAPPING_MODE_OPTION_SCROLL_MAPPING_ALL_IN_MEMORY
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
#define WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE (576 * 2)
#else
#define WS2812_DMA_DESCRIPTOR_BUFFER_MAX_SIZE                                  \
    ((NUM_VIRT_PINS + 1) * nb_components * 8 * 3 * 2 + _DMA_EXTENSTION * 4)
#endif

// Note - Unused and can be removed
#define __delay (((NUM_LEDS_PER_STRIP * 125 * 8 * nb_components) / 100000) + 1)

#define _MAX_VALUE 5000

namespace fl {

class I2SClocklessVirtualLedDriver;
struct OffsetDisplay {
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

typedef struct {
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
static IRAM_ATTR bool
_I2SClocklessVirtualLedDriverinterruptHandler(gdma_channel_handle_t dma_chan,
                                              gdma_event_data_t *event_data,
                                              void *user_data);
#else
static void IRAM_ATTR _I2SClocklessVirtualLedDriverinterruptHandler(void *arg);
#endif
static void IRAM_ATTR transpose16x1_noinline2(unsigned char *A, uint8_t *B);

static void IRAM_ATTR loadAndTranspose(I2SClocklessVirtualLedDriver *driver);
static TaskHandle_t I2SClocklessVirtualLedDriver_dispTaskHandle = 0;
static TaskHandle_t I2SClocklessVirtualLedDriver_returnTaskHandle = 0;
static void showPixelsTask(void *pvParameters);
int interruptSource;

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

__OffsetDisplay _internalOffsetDisplay;

inline uint16_t __default__mapping(uint16_t pos) { return pos; }

typedef union {
    uint8_t bytes[16 * 8];
    uint32_t shorts[16 * 2];
} Lines;

} // namespace fl

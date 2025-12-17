

/*

 */

#pragma once

// This I2S parallel mode driver only works on ESP32 and ESP32-S2
// ESP32-S3, ESP32-P4: Use LCD_CAM peripheral instead (see lcd_driver_i80.h and bulk_lcd_i80.h)
// ESP32-C3, C2, C5, C6, H2: Have completely different I2S peripheral architecture (no parallel mode)
//
// Technical note: ESP32-S3 and ESP32-P4 removed parallel LCD mode from I2S peripheral and moved it to
// dedicated LCD_CAM peripheral with different register structure and API. The register-level
// i2s_dev_t struct fields (conf, conf2, sample_rate_conf, clkm_conf, etc.) are incompatible
// with ESP32-S3/P4. FastLED provides LCD_CAM-based drivers for these chips that are more efficient.
#if !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32P4) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C5) && !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2)

#include "esp_heap_caps.h"
#include "soc/soc.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/io_mux_reg.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "rom/lldesc.h"
#include "fl/str.h"
#include "fl/stl/math.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "fl/str.h"
#include "fl/stl/cstring.h"
#include <rom/ets_sys.h>
#include "esp32-hal-log.h"
#include "ledtypes.h"

// ESP-IDF v5.x compatibility: esp_rom_gpio.h provides gpio routing functions
#if __has_include("esp_idf_version.h")
    #include "esp_idf_version.h"
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        #include "esp_rom_gpio.h"
    #endif
#endif
#ifndef NUMSTRIPS
#define NUMSTRIPS 16
#endif

#ifndef MAX_BRIGHTNESS
#define MAX_BRIGHTNESS 255
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

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#ifndef HARDWARESPRITES
#define HARDWARESPRITES 0
#endif
#ifndef NUM_LEDS_PER_STRIP
#define NUM_LEDS_PER_STRIP 50
#endif

#if HARDWARESPRITES == 1
#include "hardwareSprite.h"
#endif
#define I2S_BASE_CLK (80000000L)
typedef union
{
    uint8_t bytes[16];
    uint32_t shorts[8];
    uint32_t raw[2];
} Lines;

struct OffsetDisplay
{
    int offsetx;
    int offsety;
    int panel_height;
    int panel_width;
};
static const char *TAG = "I2SClockBasedLedDriver";
static void IRAM_ATTR _I2SClockBasedLedDriverinterruptHandler(void *arg);
static void IRAM_ATTR transpose16x1_noinline2(unsigned char *A, uint8_t *B);
static void IRAM_ATTR loadAndTranspose(uint8_t *ledt, int led_per_strip, int num_stripst, OffsetDisplay offdisp, uint8_t *buffer, int ledtodisp, uint8_t *mapg, uint8_t *mapr, uint8_t *mapb, uint8_t *mapw, int nbcomponents, int pg, int pr, int pb,uint16_t brightness);

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

inline int MOD(int a, int b)
{
    /* if (b == 1)
    {
        if (a < 0)
            return -a;
        else
            return a;
    }*/
    if (a < 0)
    {
        if (-a % b == 0)
            return 0;
        else
            return b - (-a) % b;
    }
    else
        return a % b;
}

struct LedTiming
{

    //led timing
    uint32_t T0;
    uint32_t T1;
    uint32_t T2;

    //compileled
    uint8_t f1;
    uint8_t f2;
    uint8_t f3;
};

class I2SClockBasedLedDriver
{

    struct I2SClockBasedLedDriverDMABuffer
    {
        lldesc_t descriptor;
        uint8_t *buffer;
    };

    // ESP32-S2 only has I2S0, not I2S1 (ESP32-S3 is excluded by outer platform guard)
    #if defined(CONFIG_IDF_TARGET_ESP32S2) || defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6) || defined(CONFIG_IDF_TARGET_ESP32H2)
    const int deviceBaseIndex[1] = {I2S0O_DATA_OUT0_IDX};
    const int deviceClockIndex[1] = {I2S0O_BCK_OUT_IDX};
    const int deviceWordSelectIndex[1] = {I2S0O_WS_OUT_IDX};
    const periph_module_t deviceModule[1] = {PERIPH_I2S0_MODULE};
    #else
    // ESP32 (original) has both I2S0 and I2S1
    const int deviceBaseIndex[2] = {I2S0O_DATA_OUT0_IDX, I2S1O_DATA_OUT0_IDX};
    const int deviceClockIndex[2] = {I2S0O_BCK_OUT_IDX, I2S1O_BCK_OUT_IDX};
    const int deviceWordSelectIndex[2] = {I2S0O_WS_OUT_IDX, I2S1O_WS_OUT_IDX};
    const periph_module_t deviceModule[2] = {PERIPH_I2S0_MODULE, PERIPH_I2S1_MODULE};
    #endif

public:
    i2s_dev_t *i2s;
    uint8_t __green_map[256];
    uint8_t __blue_map[256];
    uint8_t __red_map[256];
    uint8_t __white_map[256];
    uint16_t _brightness;
    
    float _gammar, _gammab, _gammag, _gammaw;
    intr_handle_t _gI2SClocklessDriver_intr_handle;
    volatile xSemaphoreHandle I2SClockBasedLedDriver_sem = NULL;
    volatile xSemaphoreHandle I2SClockBasedLedDriver_semSync = NULL;
    volatile xSemaphoreHandle I2SClockBasedLedDriver_semDisp = NULL;
    volatile int dmaBufferActive = 0;
    volatile bool wait;
    displayMode __displayMode;
    volatile int ledToDisplay;
    OffsetDisplay _offsetDisplay, _defaultOffsetDisplay;
    // volatile int oo=0;
    uint8_t *leds;
    int startleds;
    int linewidth;
    int dmaBufferCount = 2; //we use two buffers
    volatile bool transpose = false;

    volatile int num_strips;
    volatile int num_led_per_strip;
    //int clock_pin;
    int p_r, p_g, p_b;
    int i2s_base_pin_index;
    int nb_components;
    /*
     This flag is used when using the NO_WAIT modeÒÒ
     */
    volatile bool isDisplaying = false;
    volatile bool isWaiting = true;
    volatile bool framesync = false;
    volatile int counti;

    I2SClockBasedLedDriver(){};
    void setPins(int *Pins,int clock_pin)
    {

        for (int i = 0; i < num_strips; i++)
        {
#if __has_include("esp_idf_version.h") && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
            // ESP-IDF v5.x: Use new GPIO API
            gpio_reset_pin((gpio_num_t)Pins[i]);
            gpio_set_direction((gpio_num_t)Pins[i], GPIO_MODE_OUTPUT);
            esp_rom_gpio_connect_out_signal(Pins[i], deviceBaseIndex[I2S_DEVICE] + i + 8, false, false);
#else
            // ESP-IDF v3.x/v4.x: Use legacy GPIO API
            PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[Pins[i]], PIN_FUNC_GPIO);
            gpio_set_direction((gpio_num_t)Pins[i], (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
            gpio_matrix_out(Pins[i], deviceBaseIndex[I2S_DEVICE] + i + 8, false, false);
#endif
        }
#if __has_include("esp_idf_version.h") && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        // ESP-IDF v5.x: Use new GPIO API
        esp_rom_gpio_connect_out_signal(clock_pin, deviceClockIndex[I2S_DEVICE], false, false);
#else
        // ESP-IDF v3.x/v4.x: Use legacy GPIO API
        gpio_matrix_out(clock_pin, deviceClockIndex[I2S_DEVICE], false, false);
#endif
    }

    //Corrected = 255 * (Image/255)^(1/2.2).

    void setGlobalBrightness(uint16_t brightness)
    {
        _brightness = brightness % MAX_BRIGHTNESS;
        float tmp;
        for (int i = 0; i < 256; i++)
        {
            tmp = fl::powf((float)i / 255, 1 / _gammag);
            __green_map[i] = (uint8_t)(tmp * brightness);
            tmp = fl::powf((float)i / 255, 1 / _gammag);
            __blue_map[i] = (uint8_t)(tmp * brightness);
            tmp = fl::powf((float)i / 255, 1 / _gammag);
            __red_map[i] = (uint8_t)(tmp * brightness);
            tmp = fl::powf((float)i / 255, 1 / _gammag);
            __white_map[i] = (uint8_t)(tmp * brightness);
        }
    }

    void setGamma(float gammar, float gammab, float gammag, float gammaw)
    {
        _gammag = gammag;
        _gammar = gammar;
        _gammaw = gammaw;
        _gammab = gammab;
        setGlobalBrightness(_brightness);
    }

    void setGamma(float gammar, float gammab, float gammag)
    {
        _gammag = gammag;
        _gammar = gammar;
        _gammab = gammab;
        setGlobalBrightness(_brightness);
    }

    void i2sInit(int clockMHz )
    {
        int interruptSource;
        int cA=clockMHz;
        int cN=(uint8_t)80/cA;
        int cB=80%clockMHz;
        if (I2S_DEVICE == 0)
        {
            i2s = &I2S0;
            periph_module_enable(PERIPH_I2S0_MODULE);
            interruptSource = ETS_I2S0_INTR_SOURCE;
            i2s_base_pin_index = I2S0O_DATA_OUT0_IDX;
        }
        #if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32C5) && !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2)
        // Only ESP32 (original) has I2S1 (ESP32-S3 excluded by outer platform guard)
        else
        {
            i2s = &I2S1;
            periph_module_enable(PERIPH_I2S1_MODULE);
            interruptSource = ETS_I2S1_INTR_SOURCE;
            i2s_base_pin_index = I2S1O_DATA_OUT0_IDX;
        }
        #endif

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

        // ESP32-S2 and newer chips have a different register structure without clka_en
        #if !defined(CONFIG_IDF_TARGET_ESP32S2) && !defined(CONFIG_IDF_TARGET_ESP32C2) && !defined(CONFIG_IDF_TARGET_ESP32C3) && !defined(CONFIG_IDF_TARGET_ESP32C5) && !defined(CONFIG_IDF_TARGET_ESP32C6) && !defined(CONFIG_IDF_TARGET_ESP32H2)
        i2s->clkm_conf.clka_en = 0;
        #endif

        i2s->clkm_conf.clkm_div_a = cA;    // CLOCK_DIVIDER_A;
        i2s->clkm_conf.clkm_div_b = cB;    //CLOCK_DIVIDER_B;
        i2s->clkm_conf.clkm_div_num = cN; //CLOCK_DIVIDER_N;
        i2s->fifo_conf.val = 0;
        i2s->fifo_conf.tx_fifo_mod_force_en = 1;
        i2s->fifo_conf.tx_fifo_mod = 1;  // 16-bit single channel data
        i2s->fifo_conf.tx_data_num = 32; //32; // fifo length
        i2s->fifo_conf.dscr_en = 1;      // fifo will use dma
        i2s->sample_rate_conf.tx_bck_div_num = 1;
        i2s->conf1.val = 0;
        i2s->conf1.tx_stop_en = 0;
        i2s->conf1.tx_pcm_bypass = 1;

        i2s->conf_chan.val = 0;
        i2s->conf_chan.tx_chan_mod = 1; // Mono mode, with tx_msb_right = 1, everything goes to right-channel

        i2s->timing.val = 0;
        i2s->int_ena.val = 0;
        /*
        // -- Allocate i2s interrupt
        SET_PERI_REG_BITS(I2S_INT_ENA_REG(I2S_DEVICE), I2S_OUT_EOF_INT_ENA_V,1, I2S_OUT_EOF_INT_ENA_S);
        SET_PERI_REG_BITS(I2S_INT_ENA_REG(I2S_DEVICE), I2S_OUT_TOTAL_EOF_INT_ENA_V, 1, I2S_OUT_TOTAL_EOF_INT_ENA_S);
        SET_PERI_REG_BITS(I2S_INT_ENA_REG(I2S_DEVICE), I2S_OUT_TOTAL_EOF_INT_ENA_V, 1, I2S_OUT_TOTAL_EOF_INT_ENA_S);
        */
        esp_err_t e = esp_intr_alloc(interruptSource, ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM, &_I2SClockBasedLedDriverinterruptHandler, this, &_gI2SClocklessDriver_intr_handle);

        // -- Create a semaphore to block execution until all the controllers are done

        if (I2SClockBasedLedDriver_sem == NULL)
        {
            I2SClockBasedLedDriver_sem = xSemaphoreCreateBinary();
        }

        if (I2SClockBasedLedDriver_semSync == NULL)
        {
            I2SClockBasedLedDriver_semSync = xSemaphoreCreateBinary();
        }
        if (I2SClockBasedLedDriver_semDisp == NULL)
        {
            I2SClockBasedLedDriver_semDisp = xSemaphoreCreateBinary();
        }
    }

    void initDMABuffers()
    {
        DMABuffersTampon[0] = allocateDMABuffer(NUMBER_OF_BLOCK * 8 * 2 ); //the buffers for the
        DMABuffersTampon[1] = allocateDMABuffer(NUMBER_OF_BLOCK * 8 * 2 );
        DMABuffersTampon[2] = allocateDMABuffer(START_FRAME_SIZE * 8 * 2 );
        DMABuffersTampon[3] = allocateDMABuffer((NUM_LEDS_PER_STRIP * 2 ));
        fl::memset(DMABuffersTampon[3]->buffer,END_FRAME * 255,(NUM_LEDS_PER_STRIP * 2 ));
         fl::memset(DMABuffersTampon[2]->buffer,0,(START_FRAME_SIZE * 8 * 2));

        //putdefaultones((uint16_t *)DMABuffersTampon[0]->buffer);
        //putdefaultones((uint16_t *)DMABuffersTampon[1]->buffer);

#ifdef FULL_DMA_BUFFER
        /*
         We do create n+2 buffers
         the first buffer is to be sure that everything is 0
         the last one is to put back the I2S at 0 the last bufffer is longer because when using the loop display mode the time between two frames needs to be longh enough.
         */
        DMABuffersTransposed = (I2SClockBasedLedDriverDMABuffer **)malloc(sizeof(I2SClockBasedLedDriverDMABuffer *) * (num_led_per_strip + 2));
        for (int i = 0; i < num_led_per_strip + 2; i++)
        {
            if (i>0 and i < num_led_per_strip + 1)
                DMABuffersTransposed[i] = allocateDMABuffer(NUMBER_OF_BLOCK * 8 * 2 );
            else
            {
                DMABuffersTransposed[i] = allocateDMABuffer(NUM_LEDS_PER_STRIP * 2 );
                fl::memset(DMABuffersTransposed[i]->buffer,255,NUM_LEDS_PER_STRIP*2);
             }
            if (i==0)
                DMABuffersTransposed[i] = allocateDMABuffer(START_FRAME_SIZE * 8 * 2 * 16 );
            if (i < num_led_per_strip)
                DMABuffersTransposed[i]->descriptor.eof = 0;
            if (i)
            {
                DMABuffersTransposed[i - 1]->descriptor.qe.stqe_next = &(DMABuffersTransposed[i]->descriptor);
                if (i < num_led_per_strip + 1)
                {
                   // putdefaultones((uint16_t *)DMABuffersTransposed[i]->buffer);
                }
            }
        }
#endif
    }

#ifdef FULL_DMA_BUFFER

    void stopDisplayLoop()
    {
        DMABuffersTransposed[num_led_per_strip + 1]->descriptor.qe.stqe_next = 0;
    }

    void showPixelsFromBuffer()
    {
        showPixelsFromBuffer(NO_WAIT);
    }

    void showPixelsFromBuffer(displayMode dispmode)
    {
        /*
         We cannot launch twice when in loopmode
         */
        if (__displayMode == LOOP && isDisplaying)
        {
            ESP_LOGE(TAG, "The loop mode is activated execute stopDisplayLoop() first");
            return;
        }
        /*
         We wait for the display to be stopped before launching a new one
         */
        if (__displayMode == NO_WAIT && isDisplaying == true)
            xSemaphoreTake(I2SClockBasedLedDriver_semDisp, portMAX_DELAY);
        __displayMode = dispmode;
        isWaiting = false;
        if (dispmode == LOOP or dispmode == LOOP_INTERUPT)
        {
            DMABuffersTransposed[num_led_per_strip + 1]->descriptor.qe.stqe_next = &(DMABuffersTransposed[0]->descriptor);
        }
        transpose = false;
        i2sStart(DMABuffersTransposed[0]);

        if (dispmode == WAIT)
        {
            isWaiting = true;
            xSemaphoreTake(I2SClockBasedLedDriver_sem, portMAX_DELAY);
        }
    }

    void showPixelsFirstTranspose(OffsetDisplay offdisp)
    {
        _offsetDisplay = offdisp;
        showPixelsFirstTranspose();
        _offsetDisplay = _defaultOffsetDisplay;
    }
    
    void showPixelsFirstTranspose(OffsetDisplay offdisp,uint8_t * temp_leds)
    {
        _offsetDisplay = offdisp;
        showPixelsFirstTranspose(temp_leds);
        _offsetDisplay = _defaultOffsetDisplay;
    }

    void showPixelsFirstTranspose(uint8_t *new_leds)
    {
        uint8_t *tmp_leds;
        tmp_leds = leds;
        leds = new_leds;
        showPixelsFirstTranspose();
        leds = tmp_leds;
    }
    void showPixelsFirstTranspose()
    {
        showPixelsFirstTranpose(NO_WAIT);
    }
    void showPixelsFirstTranpose(displayMode dispmode)
    {
        if (leds == NULL)
        {
            printf("no leds buffer defined");
            return;
        }
        transposeAll();
        showPixelsFromBuffer(dispmode);
    }

    void transposeAll()
    {
        ledToDisplay = 0;
        //Lines secondPixel[NUMBER_OF_BLOCK];
        for (int j = 0; j <  NUM_LEDS_PER_STRIP; j++)
        {
            /*uint8_t *poli = leds + ledToDisplay * NUMBER_OF_BLOCK;
            for (int i = 0; i < NUMSTRIPS; i++)
            {

                secondPixel[p_g].bytes[i] = __green_map[*(poli + 1)];
                secondPixel[p_r].bytes[i] = __red_map[*(poli + 0)];
                secondPixel[p_b].bytes[i] = __blue_map[*(poli + 2)];
                if (nb_components > 3)
                    secondPixel[3].bytes[i] = __white_map[*(poli + 3)];
                //#endif
                poli += num_led_per_strip * nb_components;
            }
            ledToDisplay++;
            transpose16x1_noinline2(secondPixel[0].bytes, (uint16_t *)DMABuffersTransposed[j + 1]->buffer);
            transpose16x1_noinline2(secondPixel[1].bytes, (uint16_t *)DMABuffersTransposed[j + 1]->buffer + 3 * 8);
            transpose16x1_noinline2(secondPixel[2].bytes, (uint16_t *)DMABuffersTransposed[j + 1]->buffer + 2 * 3 * 8);
            if (nb_components > 3)
                transpose16x1_noinline2(secondPixel[3].bytes, (uint16_t *)DMABuffersTransposed[j + 1]->buffer + 3 * 3 * 8);
                */
        loadAndTranspose(leds, num_led_per_strip, num_strips, _offsetDisplay, DMABuffersTransposed[j + 1]->buffer, ledToDisplay, __green_map, __red_map, __blue_map, __white_map, nb_components, p_g, p_r, p_b,_brightness);
        ledToDisplay++;
        }

       
    }

    void setPixelinBuffer(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
    {
        uint32_t stripNumber = pos / num_led_per_strip;
        uint32_t posOnStrip = pos % num_led_per_strip;

        uint16_t mask = ~(1 << stripNumber);

        uint8_t colors[3];
        colors[p_g] = __green_map[green];
        colors[p_r] = __red_map[red];
        colors[p_b] = __blue_map[blue];
        uint16_t *B = (uint16_t *)DMABuffersTransposed[posOnStrip + 1]->buffer;
        // printf("nb c:%d\n",nb_components);
        uint8_t y = colors[0];
        *((uint16_t *)(B)) = (*((uint16_t *)(B)) & mask) | ((uint16_t)((y & 128) >> 7) << stripNumber);
        *((uint16_t *)(B + 5)) = (*((uint16_t *)(B + 5)) & mask) | ((uint16_t)((y & 64) >> 6) << stripNumber);
        *((uint16_t *)(B + 6)) = (*((uint16_t *)(B + 6)) & mask) | ((uint16_t)((y & 32) >> 5) << stripNumber);
        *((uint16_t *)(B + 11)) = (*((uint16_t *)(B + 11)) & mask) | ((uint16_t)((y & 16) >> 4) << stripNumber);
        *((uint16_t *)(B + 12)) = (*((uint16_t *)(B + 12)) & mask) | ((uint16_t)((y & 8) >> 3) << stripNumber);
        *((uint16_t *)(B + 17)) = (*((uint16_t *)(B + 17)) & mask) | ((uint16_t)((y & 4) >> 2) << stripNumber);
        *((uint16_t *)(B + 18)) = (*((uint16_t *)(B + 18)) & mask) | ((uint16_t)((y & 2) >> 1) << stripNumber);
        *((uint16_t *)(B + 23)) = (*((uint16_t *)(B + 23)) & mask) | ((uint16_t)(y & 1) << stripNumber);

        B += 3 * 8;
        y = colors[1];
        *((uint16_t *)(B)) = (*((uint16_t *)(B)) & mask) | ((uint16_t)((y & 128) >> 7) << stripNumber);
        *((uint16_t *)(B + 5)) = (*((uint16_t *)(B + 5)) & mask) | ((uint16_t)((y & 64) >> 6) << stripNumber);
        *((uint16_t *)(B + 6)) = (*((uint16_t *)(B + 6)) & mask) | ((uint16_t)((y & 32) >> 5) << stripNumber);
        *((uint16_t *)(B + 11)) = (*((uint16_t *)(B + 11)) & mask) | ((uint16_t)((y & 16) >> 4) << stripNumber);
        *((uint16_t *)(B + 12)) = (*((uint16_t *)(B + 12)) & mask) | ((uint16_t)((y & 8) >> 3) << stripNumber);
        *((uint16_t *)(B + 17)) = (*((uint16_t *)(B + 17)) & mask) | ((uint16_t)((y & 4) >> 2) << stripNumber);
        *((uint16_t *)(B + 18)) = (*((uint16_t *)(B + 18)) & mask) | ((uint16_t)((y & 2) >> 1) << stripNumber);
        *((uint16_t *)(B + 23)) = (*((uint16_t *)(B + 23)) & mask) | ((uint16_t)(y & 1) << stripNumber);

        B += 3 * 8;
        y = colors[2];
        *((uint16_t *)(B)) = (*((uint16_t *)(B)) & mask) | ((uint16_t)((y & 128) >> 7) << stripNumber);
        *((uint16_t *)(B + 5)) = (*((uint16_t *)(B + 5)) & mask) | ((uint16_t)((y & 64) >> 6) << stripNumber);
        *((uint16_t *)(B + 6)) = (*((uint16_t *)(B + 6)) & mask) | ((uint16_t)((y & 32) >> 5) << stripNumber);
        *((uint16_t *)(B + 11)) = (*((uint16_t *)(B + 11)) & mask) | ((uint16_t)((y & 16) >> 4) << stripNumber);
        *((uint16_t *)(B + 12)) = (*((uint16_t *)(B + 12)) & mask) | ((uint16_t)((y & 8) >> 3) << stripNumber);
        *((uint16_t *)(B + 17)) = (*((uint16_t *)(B + 17)) & mask) | ((uint16_t)((y & 4) >> 2) << stripNumber);
        *((uint16_t *)(B + 18)) = (*((uint16_t *)(B + 18)) & mask) | ((uint16_t)((y & 2) >> 1) << stripNumber);
        *((uint16_t *)(B + 23)) = (*((uint16_t *)(B + 23)) & mask) | ((uint16_t)(y & 1) << stripNumber);
        if (nb_components > 3)
        {
            B += 3 * 8;
            y = __white_map[white];
            *((uint16_t *)(B)) = (*((uint16_t *)(B)) & mask) | ((uint16_t)((y & 128) >> 7) << stripNumber);
            *((uint16_t *)(B + 5)) = (*((uint16_t *)(B + 5)) & mask) | ((uint16_t)((y & 64) >> 6) << stripNumber);
            *((uint16_t *)(B + 6)) = (*((uint16_t *)(B + 6)) & mask) | ((uint16_t)((y & 32) >> 5) << stripNumber);
            *((uint16_t *)(B + 11)) = (*((uint16_t *)(B + 11)) & mask) | ((uint16_t)((y & 16) >> 4) << stripNumber);
            *((uint16_t *)(B + 12)) = (*((uint16_t *)(B + 12)) & mask) | ((uint16_t)((y & 8) >> 3) << stripNumber);
            *((uint16_t *)(B + 17)) = (*((uint16_t *)(B + 17)) & mask) | ((uint16_t)((y & 4) >> 2) << stripNumber);
            *((uint16_t *)(B + 18)) = (*((uint16_t *)(B + 18)) & mask) | ((uint16_t)((y & 2) >> 1) << stripNumber);
            *((uint16_t *)(B + 23)) = (*((uint16_t *)(B + 23)) & mask) | ((uint16_t)(y & 1) << stripNumber);
        }
    }

    void setPixelinBuffer(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue)
    {
        uint8_t W = 0;
        if (nb_components > 3)
        {
            W = MIN(red, green);
            W = MIN(W, blue);
            red = red - W;
            green = green - W;
            blue = blue - W;
        }

        setPixelinBuffer(pos, red, green, blue, W);
    }

    void initled(int *Pinsq, int num_strips, int num_led_per_strip, colorarrangment cArr)
    {
        initled(NULL, Pinsq, num_strips, num_led_per_strip, cArr);
    }
    void waitSync()
    {
        xSemaphoreTake(I2SClockBasedLedDriver_semSync, portMAX_DELAY);
    }
#endif
//here remplacer par un uint32t !!!!!
    void setPixel(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue, uint8_t white)
    {
        uint8_t *offset = leds + (pos << 2); //faster than doing * 4
        *(offset) = red;
        *(++offset) = green;
        *(++offset) = blue;
        *(++offset) = white;
    }

    void setPixel(uint32_t pos, uint8_t red, uint8_t green, uint8_t blue)
    {

        if (nb_components == 3)
        {
            uint8_t *offset = leds + (pos << 1) + pos;
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

    /* hardware scroll

*/

    OffsetDisplay getDefaultOffset()
    {
        return _defaultOffsetDisplay;
    }

    void showPixels(uint8_t *new_leds, OffsetDisplay offdisp)
    {
        _offsetDisplay = offdisp;
        showPixels(new_leds);
        _offsetDisplay = _defaultOffsetDisplay;
    }

    void showPixels(OffsetDisplay offdisp)
    {
        _offsetDisplay = offdisp;
        showPixels();
        _offsetDisplay = _defaultOffsetDisplay;
    }

    /*
Show pixel circular
*/
    /*
    void showPixels(uint8_t *new_leds)
    {
         _offsetDisplay=offdisp
        showPixels(new_leds);
        startleds=0;
    }*/

    /*
    void showPixels(int offsett)
    {
        startleds=offsett;
        showPixels();
        startleds=0;
    }
    */

    /*
Show pixels classiques
*/
    void showPixels(uint8_t *newleds)
    {
        uint8_t *tmp_leds;
        tmp_leds = leds;
        leds = newleds;
        showPixels();
        leds = tmp_leds;
    }

    void showPixels()
    {

        #if HARDWARESPRITES == 1
        fl::memset(target, 0, num_led_per_strip * num_strips  * 2);
        for (int i = 0; i < 8; i++)
        {
            sprites[i].reorder(_offsetDisplay.panel_width, _offsetDisplay.panel_height);
        }
#endif
        if (leds == NULL)
        {
            ESP_LOGE(TAG, "no leds buffer defined");
            return;
        }
        ledToDisplay = 0;
        transpose = true;
        DMABuffersTampon[0]->descriptor.qe.stqe_next = &(DMABuffersTampon[1]->descriptor);
        DMABuffersTampon[1]->descriptor.qe.stqe_next = &(DMABuffersTampon[0]->descriptor);
        DMABuffersTampon[2]->descriptor.qe.stqe_next = &(DMABuffersTampon[0]->descriptor);
        DMABuffersTampon[3]->descriptor.qe.stqe_next = 0;
        dmaBufferActive = 0;
        loadAndTranspose(leds, num_led_per_strip, num_strips, _offsetDisplay, DMABuffersTampon[0]->buffer, ledToDisplay, __green_map, __red_map, __blue_map, __white_map, nb_components, p_g, p_r, p_b,_brightness);

        dmaBufferActive = 1;
        i2sStart(DMABuffersTampon[2]);

        isWaiting = true;
        xSemaphoreTake(I2SClockBasedLedDriver_sem, portMAX_DELAY);
    }

    void setIndvBrigtness(Pixel *ledt, IndvBrightness b)
    {
        for(int i=0;i<NUM_LEDS_PER_STRIP*NUMSTRIPS;i++)
        {
           ledt[i]=b;
        }
    }

    void setIndvBrigtness(IndvBrightness b)
    {
        Pixel *ledt;
        ledt=(Pixel *)leds;
        for(int i=0;i<NUM_LEDS_PER_STRIP*NUMSTRIPS;i++)
        {
           ledt[i]=b;
        }
    }

    void initled(uint8_t *leds, int *Pinsq, int clock_pin,int num_strips, int num_led_per_strip,  int clockMHz=4)
    {


        _gammab = 1;
        _gammar = 1;
        _gammag = 1;
        _gammaw = 1;
        startleds = 0;
#if HARDWARESPRITES == 1
        //Serial.println(NUM_LEDS_PER_STRIP * NBIS2SERIALPINS * 8);
        target = (uint16_t *)malloc(num_led_per_strip * num_strips * 2 + 2);
#endif
        setGlobalBrightness(255);
        
        dmaBufferCount = 2;
        this->leds = leds;
        this->num_led_per_strip = num_led_per_strip;
        _offsetDisplay.offsetx = 0;
        _offsetDisplay.offsety = 0;
        _offsetDisplay.panel_width = num_led_per_strip;
        _offsetDisplay.panel_height = 9999;
        _defaultOffsetDisplay = _offsetDisplay;
        linewidth = num_led_per_strip;
        this->num_strips = num_strips;
        this->dmaBufferCount = dmaBufferCount;
        setPins(Pinsq,clock_pin);
        i2sInit(clockMHz);
        initDMABuffers();
        //setIndvBrigtness(31);
    }

    //private:

    // intr_handle_t I2SClockBasedLedDriver_intr_handle;// = NULL;
    //    xSemaphoreHandle I2SClockBasedLedDriver_sem = NULL;
    //   xSemaphoreHandle I2SClockBasedLedDriver_semSync = NULL;
    //   xSemaphoreHandle I2SClockBasedLedDriver_semDisp= NULL;
    //buffer array for the transposed leds
    I2SClockBasedLedDriverDMABuffer **DMABuffersTransposed = NULL;
    //buffer array for the regular way
    I2SClockBasedLedDriverDMABuffer *DMABuffersTampon[4];

    I2SClockBasedLedDriverDMABuffer *allocateDMABuffer(int bytes)
    {
        I2SClockBasedLedDriverDMABuffer *b = (I2SClockBasedLedDriverDMABuffer *)heap_caps_malloc(sizeof(I2SClockBasedLedDriverDMABuffer), MALLOC_CAP_DMA);
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
        fl::memset(b->buffer, 0, bytes);

        b->descriptor.length = bytes;
        b->descriptor.size = bytes;
        b->descriptor.owner = 1;
        b->descriptor.sosf = 1;
        b->descriptor.buf = b->buffer;
        b->descriptor.offset = 0;
        b->descriptor.empty = 0;
        b->descriptor.eof = 1;
        b->descriptor.qe.stqe_next = 0;

        return b;
    }

    void i2sReset_DMA()
    {

        (&I2S0)->lc_conf.out_rst = 1;
        (&I2S0)->lc_conf.out_rst = 0;
    }

    void i2sReset_FIFO()
    {

        (&I2S0)->conf.tx_fifo_reset = 1;
        (&I2S0)->conf.tx_fifo_reset = 0;
    }

    void IRAM_ATTR i2sStop()
    {

        ets_delay_us(16);

        xSemaphoreGive(I2SClockBasedLedDriver_semDisp);
        esp_intr_disable(_gI2SClocklessDriver_intr_handle);
        i2sReset();

        (&I2S0)->conf.tx_start = 0;
        isDisplaying = false;
        /*
         We have finished to display the strips
         */

        //xSemaphoreGive(I2SClockBasedLedDriver_semDisp);
    }

    void putdefaultones(uint16_t *buffer)
    {
        /*order to push the data to the pins
         0:D7
         1:1
         2:1
         3:0
         4:0
         5:D6
         6:D5
         7:1
         8:1
         9:0
         10:0
         11:D4
         12:D3
         13:1
         14:1
         15:0
         16:0
         17:D2
         18:D1
         19:1
         20:1
         21:0
         22:0
         23:D0
         */
        for (int i = 0; i < nb_components * 8 / 2; i++)
        {
            buffer[i * 6 + 1] = 0xffff;
            buffer[i * 6 + 2] = 0xffff;
        }
    }

    /*
     Transpose the pixel, as the function is static and all the variables are not static or global, we need to provide all of them.
     */

    //    void transpose16x1_noinline2(uint8_t y,uint16_t *B,uint16_t mask,uint16_t mask2,int stripNumber) {
    //
    //        *((uint16_t*)(B)) =   (*((uint16_t*)(B))& mask) | ((uint16_t)((y &   128)>>7) <<stripNumber);
    //        *((uint16_t*)(B+5)) =   (*((uint16_t*)(B+5))& mask) | ((uint16_t)((y & 64)>>6) <<stripNumber);
    //        *((uint16_t*)(B+6)) =   (*((uint16_t*)(B+6))& mask) | ((uint16_t)((y & 32)>>5) <<stripNumber);
    //        *((uint16_t*)(B+11)) =   (*((uint16_t*)(B+11))& mask) | ((uint16_t)((y& 16)>>4)<<stripNumber);
    //        *((uint16_t*)(B+12)) =   (*((uint16_t*)(B+12))& mask) | ((uint16_t)((y& 8)>>3) <<stripNumber);
    //        *((uint16_t*)(B+17)) =   (*((uint16_t*)(B+17))& mask) | ((uint16_t)((y& 4)>>2) <<stripNumber);
    //        *((uint16_t*)(B+18)) =   (*((uint16_t*)(B+18))& mask) | ((uint16_t)((y& 2)>>1) <<stripNumber);
    //        *((uint16_t*)(B+23)) =   (*((uint16_t*)(B+23))& mask) | ((uint16_t)(y & 1) <<stripNumber);
    //
    //    }

    void i2sStart(I2SClockBasedLedDriverDMABuffer *startBuffer)
    {

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
        //if(transpose)
        (&I2S0)->int_ena.out_eof = 1;

        (&I2S0)->int_ena.out_total_eof = 1;
        esp_intr_enable(_gI2SClocklessDriver_intr_handle);

        //We start the I2S
        (&I2S0)->conf.tx_start = 1;

        //Set the mode to indicate that we've started
        isDisplaying = true;
    }

    void IRAM_ATTR i2sReset()
    {
        const unsigned long lc_conf_reset_flags = I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
        (&I2S0)->lc_conf.val |= lc_conf_reset_flags;
        (&I2S0)->lc_conf.val &= ~lc_conf_reset_flags;
        const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M | I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
        (&I2S0)->conf.val |= conf_reset_flags;
        (&I2S0)->conf.val &= ~conf_reset_flags;
    }

    // static void IRAM_ATTR interruptHandler(void *arg);
};
static void IRAM_ATTR _I2SClockBasedLedDriverinterruptHandler(void *arg)
{
#ifdef DO_NOT_USE_INTERUPT
    REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
    return;
#else
    I2SClockBasedLedDriver *cont = (I2SClockBasedLedDriver *)arg;

    if ((READ_PERI_REG(I2S_INT_ST_REG(I2S_DEVICE)) >> I2S_OUT_EOF_INT_ST_S) & I2S_OUT_EOF_INT_ST_V)
    {
        cont->framesync = !cont->framesync;

       if (((I2SClockBasedLedDriver *)arg)->transpose)
        {
            cont->ledToDisplay++;
            if (cont->ledToDisplay < NUM_LEDS_PER_STRIP)
            {
                loadAndTranspose(cont->leds, cont->num_led_per_strip, cont->num_strips, cont->_offsetDisplay, cont->DMABuffersTampon[cont->dmaBufferActive]->buffer, cont->ledToDisplay, cont->__green_map, cont->__red_map, cont->__blue_map, cont->__white_map, cont->nb_components, cont->p_g, cont->p_r, cont->p_b,cont->_brightness);
                if (cont->ledToDisplay == cont->num_led_per_strip - OFFSET_LED) //here it's not -1 because it takes time top have the change into account and it reread the buufer
                {
                    cont->DMABuffersTampon[cont->dmaBufferActive]->descriptor.qe.stqe_next = &(cont->DMABuffersTampon[3]->descriptor);
                }
                cont->dmaBufferActive = (cont->dmaBufferActive + 1) % 2;
            }
        }
        else
        {
            if (cont->framesync)
            {
                portBASE_TYPE HPTaskAwoken = 0;
                xSemaphoreGiveFromISR(cont->I2SClockBasedLedDriver_semSync, &HPTaskAwoken);
                if (HPTaskAwoken == pdTRUE)
                   portYIELD_FROM_ISR();
            }
        }
    }

    if ((READ_PERI_REG(I2S_INT_ST_REG(I2S_DEVICE)) >> I2S_OUT_TOTAL_EOF_INT_ST_S) & I2S_OUT_TOTAL_EOF_INT_ST_V)
    {

        //        portBASE_TYPE HPTaskAwoken = 0;
        //            xSemaphoreGiveFromISR(((I2SClockBasedLedDriver *)arg)->I2SClockBasedLedDriver_semDisp, &HPTaskAwoken);
        //            if(HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
        ((I2SClockBasedLedDriver *)arg)->i2sStop();
        if (cont->isWaiting)
        {
            portBASE_TYPE HPTaskAwoken = 0;
            xSemaphoreGiveFromISR(cont->I2SClockBasedLedDriver_sem, &HPTaskAwoken);
            if (HPTaskAwoken == pdTRUE)
                portYIELD_FROM_ISR();
        }
    }
    REG_WRITE(I2S_INT_CLR_REG(0), (REG_READ(I2S_INT_RAW_REG(0)) & 0xffffffc0) | 0x3f);
#endif
}

static void IRAM_ATTR transpose16x1_noinline2(unsigned char *A, uint8_t *B)
{

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

    *((uint16_t *)(B+2)) = (uint16_t)(((x & 0xff000000) >> 8 | ((x1 & 0xff000000))) >> 16);
    *((uint16_t *)(B)) = (uint16_t)(((x & 0xff0000) >> 16 | ((x1 & 0xff0000) >> 8)));
    *((uint16_t *)(B + 6)) = (uint16_t)(((x & 0xff00) | ((x1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 4)) = (uint16_t)((x & 0xff) | ((x1 & 0xff) << 8));
    *((uint16_t *)(B + 10)) = (uint16_t)(((y & 0xff000000) >> 8 | ((y1 & 0xff000000))) >> 16);
    *((uint16_t *)(B + 8)) = (uint16_t)(((y & 0xff0000) | ((y1 & 0xff0000) << 8)) >> 16);
    *((uint16_t *)(B + 14)) = (uint16_t)(((y & 0xff00) | ((y1 & 0xff00) << 8)) >> 8);
    *((uint16_t *)(B + 12)) = (uint16_t)((y & 0xff) | ((y1 & 0xff) << 8));
}

static void IRAM_ATTR loadAndTranspose(uint8_t *ledt, int led_per_strip, int num_stripst, OffsetDisplay offdisp, uint8_t *buffer, int ledtodisp, uint8_t *mapg, uint8_t *mapr, uint8_t *mapb, uint8_t *mapw, int nbcomponents, int pg, int pr, int pb,uint16_t brightness)
{
    Lines secondPixel[NUMBER_OF_BLOCK];
    //uint8_t *poli=ledt+ledtodisp*NUMBER_OF_BLOCK;
            uint8_t p2,p1;
            uint32_t f;
    uint32_t offp, offi, offsetled;
    uint8_t _g, _r, _b;
    int x, y, X, Y, deltaY;
    deltaY = led_per_strip / offdisp.panel_width; //calculation of height per strip
    //printf("li%d:\n",line_width);
    y = ledtodisp / offdisp.panel_width;
    x = ledtodisp % offdisp.panel_width;
    Y = y;
    X = x;
#if SNAKEPATTERN == 1

    //move in y
    if (MOD(offdisp.offsety + 2 * y, offdisp.panel_height) % 2 == 0)
    {
        y = MOD(offdisp.offsety + y, offdisp.panel_height);
    }
    else
    {
        y = MOD(offdisp.offsety + y, offdisp.panel_height);
        x = offdisp.panel_width - x - 1;
    }

    //move in x
    if (y % 2 == 0)
    {
        //x=ledtodisp%offdisp.panel_width;
        // off=((x+startleds)%line_width)+y*line_width;
        offi = MOD(x + offdisp.offsetx, offdisp.panel_width) + y * offdisp.panel_width;
#if ALTERNATEPATTERN == 1
        offp = MOD(x - offdisp.offsetx, offdisp.panel_width) + y * offdisp.panel_width;
#else
        offp = offi;
#endif
    }
    else
    {
        //x=ledtodisp%offdisp.panel_width;
        //off=(line_width-x-startleds+1)%line_width+y*line_width;
        offi = MOD(x - offdisp.offsetx, offdisp.panel_width) + y * offdisp.panel_width;
#if ALTERNATEPATTERN == 1
        offp = MOD(x + offdisp.offsetx, offdisp.panel_width) + y * offdisp.panel_width;
#else
        offp = offi;
#endif
    }

#else
    //x = ledtodisp % offdisp.panel_width;
    offi = ((x + offdisp.offsetx) % offdisp.panel_width) + y * offdisp.panel_width;
#if ALTERNATEPATTERN == 1
    offp = MOD(x - offdisp.offsetx, offdisp.panel_width) + y * offdisp.panel_width;
#else
    offp = offi;
#endif
#endif

    // uint8_t *poli = ledt + ((ledtodisp+startleds)%led_per_strip) * nbcomponents;
    offi = offi * NUMBER_OF_BLOCK;
    offp = offp * NUMBER_OF_BLOCK;
    uint8_t *poli = ledt + offi;
    offsetled = ledtodisp;
    for (int i = 0; i < num_stripst; i++)
    {

        if (poli >= ledt + (uint32_t)offdisp.panel_width * offdisp.panel_height * NUMBER_OF_BLOCK)
        {
            //Serial.printf("%d %d %d\n",i,ledtodisp,((uint32_t)poli - (uint32_t)offdisp.panel_width * offdisp.panel_height * nbcomponents-(uint32_t)ledt))/3;
            poli = poli - (uint32_t)offdisp.panel_width * offdisp.panel_height * NUMBER_OF_BLOCK;
        }
        else
        {
            if (poli < ledt)
            {
                //Serial.printf("neg %d %d %d\n",i,ledtodisp,(ledt-poli)/3);
                poli += (uint32_t)offdisp.panel_width * offdisp.panel_height * NUMBER_OF_BLOCK;
            }
        }
 
 
#if HARDWARESPRITES == 1
        //res re=RE(offsetled);
        if (target[offsetled] == 0)
        {
            _g = *(poli + 1);
            _r = *(poli);
            _b = *(poli + 2);
        }
        else
        {
            _g = _spritesleds[target[offsetled] - 1 + 1];
            _r = _spritesleds[target[offsetled] - 1];
            _b = _spritesleds[target[offsetled] - 1 + 2];
        }
#else
        _g = *(poli + 1);
        _r = *(poli);
        _b = *(poli + 2);
#endif

/*
        secondPixel[pg].bytes[i] = mapg[_g];
        secondPixel[pr].bytes[i] = mapr[_r];
        secondPixel[pb].bytes[i] = mapb[_b];
        if (nbcomponents > 3)
            secondPixel[3].bytes[i] = mapw[*(poli + 3)];
*/
    //for (int i = 0; i < NUMSTRIPS; i++)
    //{
#if NUMBER_OF_BLOCK >=1
        secondPixel[BA0].bytes[i]=*(poli);
#endif
#if NUMBER_OF_BLOCK >=2
    #if DATA_SIZE==1
        secondPixel[BA1].bytes[i]=mapr[*(poli+1)];
    #else
            secondPixel[BA1].bytes[i]=*(poli+1);
     #endif

#endif
#if NUMBER_OF_BLOCK >=3
    #if DATA_SIZE == 1
            secondPixel[BA2].bytes[i]=mapg[*(poli+2)];
    #else
             f=(*((uint16_t *)(poli+2)))*brightness/MAX_BRIGHTNESS;
             p1=f>>8;
             p2=f&255;
            secondPixel[BA2].bytes[i]=p2;
    #endif
#endif
#if NUMBER_OF_BLOCK >=4
    #if DATA_SIZE == 1
        secondPixel[BA3].bytes[i]=mapb[*(poli+3)];
    #else
         secondPixel[BA3].bytes[i]=p1;
    #endif
#endif
#if NUMBER_OF_BLOCK >=5
    #if DATA_SIZE == 2
             f=(*((uint16_t *)(poli+4)))*brightness/MAX_BRIGHTNESS;
             p1=f>>8;
             p2=f&255;
        secondPixel[BA4].bytes[i]=p2;
    #endif
#endif
#if NUMBER_OF_BLOCK >=6
        #if DATA_SIZE == 2
             secondPixel[BA5].bytes[i]=p1;
        #endif
#endif
#if NUMBER_OF_BLOCK >=7
    #if DATA_SIZE == 2
             f=(*((uint16_t *)(poli+6)))*brightness/MAX_BRIGHTNESS;
             p1=f>>8;
             p2=f&255;
             secondPixel[BA6].bytes[i]=p2;
    #endif
#endif
#if NUMBER_OF_BLOCK >=8
    #if DATA_SIZE == 2
        secondPixel[BA7].bytes[i]=p1;
    #endif
#endif
#if NUMBER_OF_BLOCK >=9
        secondPixel[BA8].bytes[i]=*(poli+8);
#endif
#if NUMBER_OF_BLOCK >=10
        secondPixel[BA9].bytes[i]=*(poli+9);
#endif
#if NUMBER_OF_BLOCK >=11
        secondPixel[BA10].bytes[i]=*(poli+10);
#endif
#if NUMBER_OF_BLOCK >=12
        secondPixel[BA11].bytes[i]=*(poli+11);
#endif
#if NUMBER_OF_BLOCK >=13
        secondPixel[BA12].bytes[i]=*(poli+12);
#endif
#if NUMBER_OF_BLOCK >=14
        secondPixel[BA13].bytes[i]=*(poli+13);
#endif
#if NUMBER_OF_BLOCK >=15
        secondPixel[BA14].bytes[i]=*(poli+14);
#endif
#if NUMBER_OF_BLOCK >=16
        secondPixel[BA15].bytes[i]=*(poli+15);
#endif
#if NUMBER_OF_BLOCK >=17
        secondPixel[BA16].bytes[i]=*(poli+16);
#endif
#if NUMBER_OF_BLOCK >=18
        secondPixel[BA17].bytes[i]=*(poli+17);
#endif
#if NUMBER_OF_BLOCK >=19
        secondPixel[BA18].bytes[i]=*(poli+18);
#endif
#if NUMBER_OF_BLOCK >=20
        secondPixel[BA19].bytes[i]=*(poli+19);
#endif

        poli += led_per_strip * NUMBER_OF_BLOCK;
    

        // }
#if HARDWARESPRITES == 1
        offsetled += led_per_strip;
#endif
        Y += deltaY;

        if (i % 2 == 0)
        {
            poli = poli - offi + offp;
        }
        else
        {
            poli = poli - offp + offi;
        }
    }
#if NUMBER_OF_BLOCK >=1
        transpose16x1_noinline2(secondPixel[0].bytes, (uint8_t *)(buffer+16*0));
#endif
#if NUMBER_OF_BLOCK >=2
        transpose16x1_noinline2(secondPixel[1].bytes, (uint8_t *)(buffer+16));
#endif
#if NUMBER_OF_BLOCK >=3
        transpose16x1_noinline2(secondPixel[2].bytes, (uint8_t *)(buffer+16*2));
#endif
#if NUMBER_OF_BLOCK >=4
        transpose16x1_noinline2(secondPixel[3].bytes, (uint8_t *)(buffer+16*3));
#endif
#if NUMBER_OF_BLOCK >=5
        transpose16x1_noinline2(secondPixel[4].bytes, (uint8_t *)(buffer+16*4));
#endif
#if NUMBER_OF_BLOCK >=6
        transpose16x1_noinline2(secondPixel[5].bytes, (uint8_t *)(buffer+16*5));
#endif
#if NUMBER_OF_BLOCK >=7
        transpose16x1_noinline2(secondPixel[6].bytes, (uint8_t *)(buffer+16*6));
#endif
#if NUMBER_OF_BLOCK >=8
        transpose16x1_noinline2(secondPixel[7].bytes, (uint8_t *)(buffer+16*7));
#endif
#if NUMBER_OF_BLOCK >=9
        transpose16x1_noinline2(secondPixel[8].bytes, (uint8_t *)(buffer+16*8));
#endif
#if NUMBER_OF_BLOCK >=10
        transpose16x1_noinline2(secondPixel[9].bytes, (uint8_t *)(buffer+16*9));
#endif
#if NUMBER_OF_BLOCK >=11
        transpose16x1_noinline2(secondPixel[10].bytes, (uint8_t *)(buffer+16*10));
#endif
#if NUMBER_OF_BLOCK >=12
        transpose16x1_noinline2(secondPixel[11].bytes, (uint8_t *)(buffer+16*11));
#endif
#if NUMBER_OF_BLOCK >=13
        transpose16x1_noinline2(secondPixel[12].bytes, (uint8_t *)(buffer+16*12));
#endif
#if NUMBER_OF_BLOCK >=14
        transpose16x1_noinline2(secondPixel[13].bytes, (uint8_t *)(buffer+16*13));
#endif
#if NUMBER_OF_BLOCK >=15
        transpose16x1_noinline2(secondPixel[14].bytes, (uint8_t *)(buffer+16*14));
#endif
#if NUMBER_OF_BLOCK >=16
        transpose16x1_noinline2(secondPixel[15].bytes, (uint8_t *)(buffer+16*15));
#endif
#if NUMBER_OF_BLOCK >=17
        transpose16x1_noinline2(secondPixel[16].bytes, (uint8_t *)(buffer+16*16));
#endif
#if NUMBER_OF_BLOCK >=18
        transpose16x1_noinline2(secondPixel[17].bytes, (uint8_t *)(buffer+16*17));
#endif
#if NUMBER_OF_BLOCK >=19
        transpose16x1_noinline2(secondPixel[18].bytes, (uint8_t *)(buffer+16*18));
#endif
#if NUMBER_OF_BLOCK >=20
        transpose16x1_noinline2(secondPixel[19].bytes, (uint8_t *)(buffer+16*19));
#endif
}

#endif // !defined(CONFIG_IDF_TARGET_ESP32S3) && !defined(CONFIG_IDF_TARGET_ESP32C3) && ...

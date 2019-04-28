/*
 *
 *
 */
/*
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

FASTLED_NAMESPACE_BEGIN

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_heap_caps.h"
#include "soc/soc.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/io_mux_reg.h"
#include "driver/gpio.h"
#include "driver/periph_ctrl.h"
#include "rom/lldesc.h"
#include "esp_intr.h"
#include "esp_log.h"

#ifdef __cplusplus
}
#endif

__attribute__ ((always_inline)) inline static uint32_t __clock_cycles() {
  uint32_t cyc;
  __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc));
  return cyc;
}

#define FASTLED_HAS_CLOCKLESS 1
#define NUM_COLOR_CHANNELS 3

// -- Choose which I2S device to use
#ifndef I2S_DEVICE
#define I2S_DEVICE 0
#endif

// -- Max number of controllers we can support
#ifndef FASTLED_I2S_MAX_CONTROLLERS
#define FASTLED_I2S_MAX_CONTROLLERS 24
#endif

// -- I2S clock
#define I2S_BASE_CLK (800000000L)

// -- Convert ESP32 cycles back into nanoseconds
#define ESPCLKS_TO_NS(_CLKS) (((long)(_CLKS) * 1000L) / F_CPU_MHZ)

// -- I2S bit encoding
//    For now, this stuff is hard-coded
#define FASTLED_I2S_CLOCK_DIVIDER   25     //   10  // 80MHz --> 8MHz
#define FASTLED_I2S_NS_PER_PULSE   312.5   //  125  // == 125ns per cycle

// -- Array of all controllers
static CLEDController * gControllers[FASTLED_I2S_MAX_CONTROLLERS];
static int gNumControllers = 0;
static int gNumStarted = 0;

// -- Global semaphore for the whole show process
//    Semaphore is not given until all data has been sent
static xSemaphoreHandle gTX_sem = NULL;

// -- I2S global configuration stuff
static bool gInitialized = false;

static intr_handle_t gI2S_intr_handle = NULL;

static i2s_dev_t * i2s;          // A pointer to the memory-mapped structure: I2S0 or I2S1
static int i2s_base_pin_index;   // I2S goes to these pins until we remap them using the GPIO matrix

// --- I2S DMA buffers
struct DMABuffer {
    lldesc_t descriptor;
    uint8_t * buffer;
};

#define NUM_DMA_BUFFERS 2
static DMABuffer * dmaBuffers[NUM_DMA_BUFFERS];

// -- Bit patterns
//    We configure the I2S data clock so that each pulse is
//    125ns. Depending on the kind of LED we compute a pattern of
//    pulses that match the timing. For example, a "1" bit for the
//    WS2812 consists of 700-900ns high, followed by 300-500ns
//    low. Using 125ns per pulse, we can send a "1" bit using this
//    pattern: 1111111000 (a total of 10 bits, or 1250ns)
//
//    For now, we require all strips to be the same chipset, so these
//    are global variables.

static int      gPulsesPerBit = 0;
static uint32_t gOneBit[10] = {0,0,0,0,0,0,0,0,0,0};
static uint32_t gZeroBit[10]  = {0,0,0,0,0,0,0,0,0,0};

// -- Counters to track progress
static int gCurBuffer = 0;
static bool gDoneFilling = false;

// -- Temp buffers for pixels and bits being formatted for DMA
static uint8_t gPixelRow[NUM_COLOR_CHANNELS][32];
static uint8_t gPixelBits[NUM_COLOR_CHANNELS][8][4];


template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER>
{
    // -- The index of this controller in the global gControllers array
    int            m_index;

    // -- Store the GPIO pin
    gpio_num_t     mPin;

    // -- This instantiation forces a check on the pin choice
    FastPin<DATA_PIN> mFastPin;

    // -- Save the pixel controller
    PixelController<RGB_ORDER> * mPixels;

public:

    void init()
    {
        i2sInit();
        
        // -- Allocate space to save the pixel controller
        //    during parallel output
        mPixels = (PixelController<RGB_ORDER> *) malloc(sizeof(PixelController<RGB_ORDER>));

        gControllers[gNumControllers] = this;
        m_index = gNumControllers;
        gNumControllers++;

        // -- Set up the pin We have to do two things: configure the
        //    actual GPIO pin, and route the output from the default
        //    pin (determined by the I2S device) to the pin we
        //    want. We compute the default pin using the index of this
        //    controller in the array. This order is crucial because
        //    the bits must go into the DMA buffer in the same order.
        mPin = gpio_num_t(DATA_PIN);

        PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[DATA_PIN], PIN_FUNC_GPIO);
        gpio_set_direction(mPin, (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
        pinMode(mPin,OUTPUT);
        gpio_matrix_out(mPin, i2s_base_pin_index + m_index, false, false);
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:

    static void initBitPatterns()
    {
        // Precompute the bit patterns based on the I2S sample rate
        uint32_t T1ns = ESPCLKS_TO_NS(T1);
        uint32_t T2ns = ESPCLKS_TO_NS(T2);
        uint32_t T3ns = ESPCLKS_TO_NS(T3);

        Serial.print("T1 = "); Serial.print(T1); Serial.print(" ns "); Serial.println(T1ns);
        Serial.print("T2 = "); Serial.print(T2); Serial.print(" ns "); Serial.println(T2ns);
        Serial.print("T3 = "); Serial.print(T3); Serial.print(" ns "); Serial.println(T3ns);

        gPulsesPerBit = (T1ns + T2ns + T3ns)/FASTLED_I2S_NS_PER_PULSE;

        Serial.print("Pulses per bit: "); Serial.println(gPulsesPerBit);

        int ones_for_one  = ((T1ns + T2ns - 1)/FASTLED_I2S_NS_PER_PULSE) + 1;
        Serial.print("One bit:  target "); 
        Serial.print(T1ns+T2ns); Serial.print("ns --- "); 
        Serial.print(ones_for_one); Serial.print(" 1 bits");
        Serial.print(" = "); Serial.print(ones_for_one * FASTLED_I2S_NS_PER_PULSE); Serial.println("ns");
        int i = 0;
        while ( i < ones_for_one ) {
            gOneBit[i] = 0xFFFFFF00;
            i++;
        }
        while ( i < gPulsesPerBit ) {
            gOneBit[i] = 0x00000000;
            i++;
        }

        int ones_for_zero = ((T1ns - 1)/FASTLED_I2S_NS_PER_PULSE) + 1;
        Serial.print("Zero bit:  target "); 
        Serial.print(T1ns); Serial.print("ns --- "); 
        Serial.print(ones_for_zero); Serial.print(" 1 bits");
        Serial.print(" = "); Serial.print(ones_for_zero * FASTLED_I2S_NS_PER_PULSE); Serial.println("ns");
        i = 0;
        while ( i < ones_for_zero ) {
            gZeroBit[i] = 0xFFFFFF00;
            i++;
        }
        while ( i < gPulsesPerBit ) {
            gZeroBit[i] = 0x00000000;
            i++;
        }

        memset(gPixelRow, 0, NUM_COLOR_CHANNELS * 32);
        memset(gPixelBits, 0, NUM_COLOR_CHANNELS * 32);
    }

    static DMABuffer * allocateDMABuffer(int bytes)
    {
        DMABuffer * b = (DMABuffer *)heap_caps_malloc(sizeof(DMABuffer), MALLOC_CAP_DMA);

        b->buffer = (uint8_t *)heap_caps_malloc(bytes, MALLOC_CAP_DMA);
        memset(b->buffer, 0, bytes);

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

    static void i2sInit()
    {
        // -- Only need to do this once
        if (gInitialized) return;

        // -- Construct the bit patterns for ones and zeros
        initBitPatterns();

        // -- Choose whether to use I2S device 0 or device 1
        //    Set up the various device-specific parameters
        int interruptSource;
        if (I2S_DEVICE == 0) {
            i2s = &I2S0;
            periph_module_enable(PERIPH_I2S0_MODULE);
            interruptSource = ETS_I2S0_INTR_SOURCE;
            i2s_base_pin_index = I2S0O_DATA_OUT0_IDX;
        } else {
            i2s = &I2S1;
            periph_module_enable(PERIPH_I2S1_MODULE);
            interruptSource = ETS_I2S1_INTR_SOURCE;
            i2s_base_pin_index = I2S1O_DATA_OUT0_IDX;
        }

        // -- Reset everything
        i2sReset();
        i2sReset_DMA();
        i2sReset_FIFO();

        // -- Main configuration 
        i2s->conf.tx_msb_right = 1;
        i2s->conf.tx_mono = 0;
        i2s->conf.tx_short_sync = 0;
        i2s->conf.tx_msb_shift = 0;
        i2s->conf.tx_right_first = 1; // 0;//1;
        i2s->conf.tx_slave_mod = 0;

        // -- Set parallel mode
        i2s->conf2.val = 0;
        i2s->conf2.lcd_en = 1;
        i2s->conf2.lcd_tx_wrx2_en = 0; // 0 for 16 or 32 parallel output
        i2s->conf2.lcd_tx_sdx2_en = 0; // HN

        // -- Set up the clock rate and sampling
        i2s->sample_rate_conf.val = 0;
        i2s->sample_rate_conf.tx_bits_mod = 32; // Number of parallel bits/pins
        i2s->sample_rate_conf.tx_bck_div_num = 1;
        i2s->clkm_conf.val = 0;
        i2s->clkm_conf.clka_en = 0;

        // -- Data clock is computed as Base/(div_num + (div_b/div_a))
        //    Base is 80Mhz, so 80/(10 + 0/1) = 8Mhz
        //    One cycle is 125ns
        i2s->clkm_conf.clkm_div_a = 1;
        i2s->clkm_conf.clkm_div_b = 0;
        i2s->clkm_conf.clkm_div_num = FASTLED_I2S_CLOCK_DIVIDER;
    
        i2s->fifo_conf.val = 0;
        i2s->fifo_conf.tx_fifo_mod_force_en = 1;
        i2s->fifo_conf.tx_fifo_mod = 3;  // 32-bit single channel data
        i2s->fifo_conf.tx_data_num = 32; // fifo length
        i2s->fifo_conf.dscr_en = 1;      // fifo will use dma

        i2s->conf1.val = 0;
        i2s->conf1.tx_stop_en = 0;
        i2s->conf1.tx_pcm_bypass = 1;

        i2s->conf_chan.val = 0;
        i2s->conf_chan.tx_chan_mod = 1; // Mono mode, with tx_msb_right = 1, everything goes to right-channel

        i2s->timing.val = 0;

        // -- Allocate two DMA buffers
        dmaBuffers[0] = allocateDMABuffer(32 * NUM_COLOR_CHANNELS * gPulsesPerBit);
        dmaBuffers[1] = allocateDMABuffer(32 * NUM_COLOR_CHANNELS * gPulsesPerBit);

        // -- Arrange them as a circularly linked list
        dmaBuffers[0]->descriptor.qe.stqe_next = &(dmaBuffers[1]->descriptor);
        dmaBuffers[1]->descriptor.qe.stqe_next = &(dmaBuffers[0]->descriptor);

        // -- Allocate i2s interrupt
        SET_PERI_REG_BITS(I2S_INT_ENA_REG(I2S_DEVICE), I2S_OUT_EOF_INT_ENA_V, 1, I2S_OUT_EOF_INT_ENA_S);
        esp_err_t e = esp_intr_alloc(interruptSource, 0, // ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3,
                                     &interruptHandler, 0, &gI2S_intr_handle);

        // -- Create a semaphore to block execution until all the controllers are done
        if (gTX_sem == NULL) {
            gTX_sem = xSemaphoreCreateBinary();
            xSemaphoreGive(gTX_sem);
        }
                
        // Serial.println("Init I2S");
        gInitialized = true;
    }

    // -- Show pixels
    //    This is the main entry point for the controller.
    virtual void showPixels(PixelController<RGB_ORDER> & pixels)
    {
        if (gNumStarted == 0) {
            // -- First controller: make sure everything is set up
            xSemaphoreTake(gTX_sem, portMAX_DELAY);
        }

        // -- Initialize the local state, save a pointer to the pixel
        //    data. We need to make a copy because pixels is a local
        //    variable in the calling function, and this data structure
        //    needs to outlive this call to showPixels.
        (*mPixels) = pixels;

        // -- Keep track of the number of strips we've seen
        gNumStarted++;

        // Serial.print("Show pixels ");
        // Serial.println(gNumStarted);

        // -- The last call to showPixels is the one responsible for doing
        //    all of the actual work
        if (gNumStarted == gNumControllers) {
            gCurBuffer = 0;
            gDoneFilling = false;

            // -- Prefill both buffers
            fillBuffer();
            fillBuffer();

            i2sStart();

            // -- Wait here while the rest of the data is sent. The interrupt handler
            //    will keep refilling the RMT buffers until it is all sent; then it
            //    gives the semaphore back.
            xSemaphoreTake(gTX_sem, portMAX_DELAY);
            xSemaphoreGive(gTX_sem);

            i2sStop();

            // -- Reset the counters
            gNumStarted = 0;
        }
    }

    // -- Custom interrupt handler
    static IRAM_ATTR void interruptHandler(void *arg)
    {
        if (i2s->int_st.out_eof) {
            i2s->int_clr.val = i2s->int_raw.val;

            if ( ! gDoneFilling) {
                fillBuffer();
            } else {
                portBASE_TYPE HPTaskAwoken = 0;
                xSemaphoreGiveFromISR(gTX_sem, &HPTaskAwoken);
                if(HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
            }
        }
    }

    static void fillBuffer()
    {
        volatile uint32_t * buf = (uint32_t *) dmaBuffers[gCurBuffer]->buffer;
        gCurBuffer = (gCurBuffer + 1) % NUM_DMA_BUFFERS;

        // -- Get the requested pixel from each controller. Store the
        //    data for each color channel in a separate array.
        uint32_t has_data_mask = 0;
        for (int i = 0; i < gNumControllers; i++) {
            // -- Store the pixels in reverse controller order starting at index 23
            //    This causes the bits to come out in the right position after we
            //    transpose them.
            int bit_index = 23-i;
            ClocklessController * pController = static_cast<ClocklessController*>(gControllers[i]);
            if (pController->mPixels->has(1)) {
                gPixelRow[0][bit_index] = pController->mPixels->loadAndScale0();
                gPixelRow[1][bit_index] = pController->mPixels->loadAndScale1();
                gPixelRow[2][bit_index] = pController->mPixels->loadAndScale2();
                pController->mPixels->advanceData();
                pController->mPixels->stepDithering();

                // -- Record that this controller still has data to send
                has_data_mask |= (1 << (i+8));
            }
        }

        if (has_data_mask == 0) {
            gDoneFilling = true;
            return;
        }

        // -- Transpose and encode the pixel data for the DMA buffer
        int buf_index = 0;
        for (int channel = 0; channel < NUM_COLOR_CHANNELS; channel++) {

            // -- Tranpose each array: all the bit 7's, then all the bit 6's, ...
            transpose32(gPixelRow[channel], gPixelBits[channel][0] );

            //Serial.print("Channel: "); Serial.print(channel); Serial.print(" ");
            for (int bitnum = 0; bitnum < 8; bitnum++) {
                uint8_t * row = (uint8_t *) (gPixelBits[channel][bitnum]);
                uint32_t bit = (row[0] << 24) | (row[1] << 16) | (row[2] << 8) | row[3];

                /*
                Serial.print(bitnum); Serial.print(": ");
                uint32_t bt = bit;
                for (int k = 0; k < 32; k++) {
                    if (bt & 0x80000000) Serial.print("1");
                    else Serial.print("0");
                    bt = bt << 1;
                }
                Serial.println();
                */

                for (int pulse_num = 0; pulse_num < gPulsesPerBit; pulse_num++) {
                    buf[buf_index++] = has_data_mask & ( (bit & gOneBit[pulse_num]) | (~bit & gZeroBit[pulse_num]) );
                    //if (buf[buf_index-1] & 0x100) Serial.print("1");
                    //else Serial.print("0");
                }
            }
        }
    }

    static void transpose32(uint8_t * pixels, uint8_t * bits)
    {
        transpose8rS32(& pixels[0],  1, 4, & bits[0]);
        transpose8rS32(& pixels[8],  1, 4, & bits[1]);
        transpose8rS32(& pixels[16], 1, 4, & bits[2]);
        //transpose8rS32(& pixels[24], 1, 4, & bits[3]);
    }

    static void transpose8rS32(uint8_t * A, int m, int n, uint8_t * B) 
    {
        uint32_t x, y, t;

        // Load the array and pack it into x and y.

        x = (A[0]<<24)   | (A[m]<<16)   | (A[2*m]<<8) | A[3*m];
        y = (A[4*m]<<24) | (A[5*m]<<16) | (A[6*m]<<8) | A[7*m];

        t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
        t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);

        t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);
        t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

        t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
        y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
        x = t;

        B[0]=x>>24;    B[n]=x>>16;    B[2*n]=x>>8;  B[3*n]=x;
        B[4*n]=y>>24;  B[5*n]=y>>16;  B[6*n]=y>>8;  B[7*n]=y;
    }

    /** Start I2S transmission
     */
    static void i2sStart()
    {
        // esp_intr_disable(gI2S_intr_handle);
        // Serial.println("I2S start");
        i2sReset();
        //Serial.println(dmaBuffers[0]->sampleCount());
        i2s->lc_conf.val=I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN | I2S_OUT_DATA_BURST_EN;
        i2s->out_link.addr = (uint32_t) & (dmaBuffers[0]->descriptor);
        i2s->out_link.start = 1;
        ////vTaskDelay(5);
        i2s->int_clr.val = i2s->int_raw.val;
        // //vTaskDelay(5);
        i2s->int_ena.out_dscr_err = 1;
        //enable interrupt
        ////vTaskDelay(5);
        esp_intr_enable(gI2S_intr_handle);
        // //vTaskDelay(5);
        i2s->int_ena.val = 0;
        i2s->int_ena.out_eof = 1;

        //start transmission
        i2s->conf.tx_start = 1;
    }

    static void i2sReset()
    {
        // Serial.println("I2S reset");
        const unsigned long lc_conf_reset_flags = I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
        i2s->lc_conf.val |= lc_conf_reset_flags;
        i2s->lc_conf.val &= ~lc_conf_reset_flags;

        const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M | I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
        i2s->conf.val |= conf_reset_flags;
        i2s->conf.val &= ~conf_reset_flags;
    }

    static void i2sReset_DMA()
    {
        i2s->lc_conf.in_rst=1; i2s->lc_conf.in_rst=0;
        i2s->lc_conf.out_rst=1; i2s->lc_conf.out_rst=0;
    }

    static void i2sReset_FIFO()
    {
        i2s->conf.rx_fifo_reset=1; i2s->conf.rx_fifo_reset=0;
        i2s->conf.tx_fifo_reset=1; i2s->conf.tx_fifo_reset=0;
    }

    static void i2sStop()
    {
        // Serial.println("I2S stop");
        esp_intr_disable(gI2S_intr_handle);
        i2sReset();
        i2s->conf.rx_start = 0;
        i2s->conf.tx_start = 0;
    }
};

FASTLED_NAMESPACE_END

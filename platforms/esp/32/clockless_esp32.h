/*
 * ESP32 driver for clockless LED chips
 *
 * There are two different drivers available for the ESP32: RMT and I2S.
 *
 * The RMT driver is the default. It uses the remote control
 * peripheral (RMT) to drive up to 8 strips in parallel, queueing any
 * additional strips, which are output as RMT channels become
 * available. The reason it is the default is that it can handle any
 * mix of strips with different chips, different timing, different
 * lengths, etc.
 *
 * The I2S implementation can drive up to 24 strips in parallel, but
 * with the following limitation: all the strips must have the same
 * timing (i.e., they must all use the same chip).
 *
 * To enable the I2S driver, add the following line *before* including
 * FastLED.h (no other changes are necessary):
 *
 * #define FASTLED_ESP32_I2S true
 *
 * === DETAILS ===
 *
 * RMT Integration into FastLED ClocklessController
 * Copyright (c) 2018 Samuel Z. Guyer
 * Copyright (c) 2017 Thomas Basler
 * Copyright (c) 2017 Martin F. Falatic
 *
 * ESP32 support is provided using the RMT peripheral device -- a unit
 * on the chip designed specifically for generating (and receiving)
 * precisely-timed digital signals. Nominally for use in infrared
 * remote controls, we use it to generate the signals for clockless
 * LED strips. The main advantage of using the RMT device is that,
 * once programmed, it generates the signal asynchronously, allowing
 * the CPU to continue executing other code. It is also not vulnerable
 * to interrupts or other timing problems that could disrupt the signal.
 *
 * The implementation strategy is borrowed from previous work and from
 * the RMT support built into the ESP32 IDF. The RMT device has 8
 * channels, which can be programmed independently to send sequences
 * of high/low bits. Memory for each channel is limited, however, so
 * in order to send a long sequence of bits, we need to continuously
 * refill the buffer until all the data is sent. To do this, we fill
 * half the buffer and then set an interrupt to go off when that half
 * is sent. Then we refill that half while the second half is being
 * sent. This strategy effectively overlaps computation (by the CPU)
 * and communication (by the RMT).
 *
 * Since the RMT device only has 8 channels, we need a strategy to
 * allow more than 8 LED controllers. Our driver assigns controllers
 * to channels on the fly, queuing up controllers as necessary until a
 * channel is free. The main showPixels routine just fires off the
 * first 8 controllers; the interrupt handler starts new controllers
 * asynchronously as previous ones finish. So, for example, it can
 * send the data for 8 controllers simultaneously, but 16 controllers
 * would take approximately twice as much time.
 *
 * There is a #define that allows a program to control the total
 * number of channels that the driver is allowed to use. It defaults
 * to 8 -- use all the channels. Setting it to 1, for example, results
 * in fully serial output:
 *
 *     #define FASTLED_RMT_MAX_CHANNELS 1
 *
 * OTHER RMT APPLICATIONS
 *
 * The default FastLED driver takes over control of the RMT interrupt
 * handler, making it hard to use the RMT device for other
 * (non-FastLED) purposes. You can change it's behavior to use the ESP
 * core driver instead, allowing other RMT applications to
 * co-exist. To switch to this mode, add the following directive
 * before you include FastLED.h:
 *
 *      #define FASTLED_RMT_BUILTIN_DRIVER
 *
 * There may be a performance penalty for using this mode. We need to
 * compute the RMT signal for the entire LED strip ahead of time,
 * rather than overlapping it with communication. We also need a large
 * buffer to hold the signal specification. Each bit of pixel data is
 * represented by a 32-bit pulse specification, so it is a 32X blow-up
 * in memory use.
 *
 *
 * Based on public domain code created 19 Nov 2016 by Chris Osborn <fozztexx@fozztexx.com>
 * http://insentricity.com *
 *
 * I2S Integration
 * Copyright (c) 2019 Yves Bazin
 * Copyright (c) 2019 Samuel Z. Guyer
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

#include "esp_intr.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "driver/periph_ctrl.h"

#ifdef FASTLED_ESP32_I2S
#include "esp_heap_caps.h"
#include "soc/soc.h"
#include "soc/gpio_sig_map.h"
#include "soc/i2s_reg.h"
#include "soc/i2s_struct.h"
#include "soc/io_mux_reg.h"
#include "rom/lldesc.h"
#else
#include "esp32-hal.h"
#include "driver/rmt.h"
#include "freertos/semphr.h"
#include "soc/rmt_struct.h"
#endif

#ifdef __cplusplus
}
#endif

__attribute__ ((always_inline)) inline static uint32_t __clock_cycles() {
    uint32_t cyc;
    __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc));
    return cyc;
}


// === Common stuff ==========================================

#define FASTLED_HAS_CLOCKLESS 1
#define NUM_COLOR_CHANNELS 3

// -- Max number of controllers we can support
#ifndef FASTLED_ESP32_MAX_CONTROLLERS
#define FASTLED_ESP32_MAX_CONTROLLERS 24
#endif

// -- Convert ESP32 cycles back into nanoseconds
#define ESPCLKS_TO_NS(_CLKS) (((long)(_CLKS) * 1000L) / F_CPU_MHZ)

// -- Array of all controllers
static CLEDController * gControllers[FASTLED_ESP32_MAX_CONTROLLERS];
static int gNumControllers = 0;
static int gNumStarted = 0;

// -- Global semaphore for the whole show process
//    Semaphore is not given until all data has been sent
static xSemaphoreHandle gTX_sem = NULL;

// -- One-time initialiation (both I2S and RMT)
static bool gInitialized = false;

// === I2S specific stuff ====================================

// -- Choose which I2S device to use
#ifndef I2S_DEVICE
#define I2S_DEVICE 0
#endif

// -- I2S clock
#define I2S_BASE_CLK (80000000L)
#define I2S_MAX_CLK (20000000L) //more tha a certain speed and the I2s looses some bits
#define I2S_MAX_PULSE_PER_BIT 20 //put it higher to get more accuracy but it could decrease the refresh rate without real improvement

static intr_handle_t gI2S_intr_handle = NULL;

static i2s_dev_t * i2s;          // A pointer to the memory-mapped structure: I2S0 or I2S1
static int i2s_base_pin_index;   // I2S goes to these pins until we remap them using the GPIO matrix

// --- I2S DMA buffers
struct DMABuffer {
    lldesc_t descriptor;
    uint8_t * buffer;
};

// -- We use two DMA buffers: one is being sent while we fill the other
#define NUM_DMA_BUFFERS 2
static DMABuffer * dmaBuffers[NUM_DMA_BUFFERS];

// -- Counters to track progress
static int gCurBuffer = 0;
static bool gDoneFilling = false;

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
static uint32_t gOneBit[40] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static uint32_t gZeroBit[40]  = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

static int ones_for_one;
static int ones_for_zero;
// -- Temp buffers for pixels and bits being formatted for DMA
static uint8_t gPixelRow[NUM_COLOR_CHANNELS][32];
static uint8_t gPixelBits[NUM_COLOR_CHANNELS][8][4];
static int CLOCK_DIVIDER_N;
static int CLOCK_DIVIDER_A;
static int CLOCK_DIVIDER_B;

#endif

// === RMT specific stuff ====================================

// -- Configuration constants
#define DIVIDER             2 /* 4, 8 still seem to work, but timings become marginal */
#define MAX_PULSES         32 /* A channel has a 64 "pulse" buffer - we use half per pass */

// -- Convert nanoseconds into RMT cycles
#define F_CPU_RMT       (  80000000L)
#define NS_PER_SEC      (1000000000L)
#define CYCLES_PER_SEC  (F_CPU_RMT/DIVIDER)
#define NS_PER_CYCLE    ( NS_PER_SEC / CYCLES_PER_SEC )
#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )

// -- Convert ESP32 cycles to RMT cycles
#define TO_RMT_CYCLES(_CLKS) NS_TO_CYCLES(ESPCLKS_TO_NS(_CLKS))    

// -- Number of cycles to signal the strip to latch
#define RMT_RESET_DURATION NS_TO_CYCLES(50000)

// -- Core or custom driver
#ifndef FASTLED_RMT_BUILTIN_DRIVER
#define FASTLED_RMT_BUILTIN_DRIVER false
#endif

// -- Number of RMT channels to use (up to 8)
//    Redefine this value to 1 to force serial output
#ifndef FASTLED_RMT_MAX_CHANNELS
#define FASTLED_RMT_MAX_CHANNELS 8
#endif

// -- Current set of active controllers, indexed by the RMT
//    channel assigned to them.
static CLEDController * gOnChannel[FASTLED_RMT_MAX_CHANNELS];

static int gNumDone = 0;
static int gNext = 0;

static intr_handle_t gRMT_intr_handle = NULL;

// =====================================================================

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER>
{
    // -- Store the GPIO pin
    gpio_num_t     mPin;
    
    // -- This instantiation forces a check on the pin choice
    FastPin<DATA_PIN> mFastPin;
    
    // -- Save the pixel controller
    PixelController<RGB_ORDER> * mPixels;

    // -- RMT has 8 channels, numbered 0 to 7
    rmt_channel_t  mRMT_channel;
    
    // -- Timing values for zero and one bits, derived from T1, T2, and T3
    rmt_item32_t   mZero;
    rmt_item32_t   mOne;

    // -- State information for keeping track of where we are in the pixel data
    uint8_t *      mPixelData = NULL;
    int            mSize = 0;
    int            mCurByte;
    uint16_t       mCurPulse;

    // -- Buffer to hold all of the pulses. For the version that uses
    //    the RMT driver built into the ESP core.
    rmt_item32_t * mBuffer;
    uint16_t       mBufferSize;

public:

    void init()
    {
        // -- Allocate space to save the pixel controller
        //    during parallel output
        mPixels = (PixelController<RGB_ORDER> *) malloc(sizeof(PixelController<RGB_ORDER>));
        
        gControllers[gNumControllers] = this;
        int my_index = gNumControllers;
        gNumControllers++;

        // -- Store the pin
        mPin = gpio_num_t(DATA_PIN);

        if (FASTLED_ESP32_I2S) {
            // -- One-time initialization of I2S system
            i2sInit();
        
            // -- Set up the pin We have to do two things: configure the
            //    actual GPIO pin, and route the output from the default
            //    pin (determined by the I2S device) to the pin we
            //    want. We compute the default pin using the index of this
            //    controller in the array. This order is crucial because
            //    the bits must go into the DMA buffer in the same order.
        
            PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[DATA_PIN], PIN_FUNC_GPIO);
            gpio_set_direction(mPin, (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
            pinMode(mPin,OUTPUT);
            gpio_matrix_out(mPin, i2s_base_pin_index + my_index, false, false);
        } else {
            // -- One-time initialization of RMT system
            rmtInit();

            // -- RMT: Precompute rmt items corresponding to a zero bit and a one bit
            //         according to the timing values given in the template instantiation
            // T1H
            mOne.level0 = 1;
            mOne.duration0 = TO_RMT_CYCLES(T1+T2);
            // T1L
            mOne.level1 = 0;
            mOne.duration1 = TO_RMT_CYCLES(T3);

            // T0H
            mZero.level0 = 1;
            mZero.duration0 = TO_RMT_CYCLES(T1);
            // T0L
            mZero.level1 = 0;
            mZero.duration1 = TO_RMT_CYCLES(T2 + T3);
        }
    }
    
    virtual uint16_t getMaxRefreshRate() const { return 400; }
    
protected:
   
    // ----------------------------------------------------------------------
    //  RMT Initialization
    // ----------------------------------------------------------------------

    static void rmtInit()
    {
        // -- Only need to do this once
        if (gInitialized) return;

        for (int i = 0; i < FASTLED_ESP32_MAX_CHANNELS; i++) {
            gOnChannel[i] = NULL;

            // -- RMT configuration for transmission
            rmt_config_t rmt_tx;
            rmt_tx.channel = rmt_channel_t(i);
            rmt_tx.rmt_mode = RMT_MODE_TX;
            rmt_tx.gpio_num = mPin;  // The particular pin will be assigned later
            rmt_tx.mem_block_num = 1;
            rmt_tx.clk_div = DIVIDER;
            rmt_tx.tx_config.loop_en = false;
            rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
            rmt_tx.tx_config.carrier_en = false;
            rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
            rmt_tx.tx_config.idle_output_en = true;
                
            // -- Apply the configuration
            rmt_config(&rmt_tx);

            if (FASTLED_RMT_BUILTIN_DRIVER) {
                rmt_driver_install(rmt_channel_t(i), 0, 0);
            } else {
                // -- Set up the RMT to send 1/2 of the pulse buffer and then
                //    generate an interrupt. When we get this interrupt we
                //    fill the other half in preparation (kind of like double-buffering)
                rmt_set_tx_thr_intr_en(rmt_channel_t(i), true, MAX_PULSES);
            }
        }

        // -- Create a semaphore to block execution until all the controllers are done
        if (gTX_sem == NULL) {
            gTX_sem = xSemaphoreCreateBinary();
            xSemaphoreGive(gTX_sem);
        }
                
        if ( ! FASTLED_RMT_BUILTIN_DRIVER) {
            // -- Allocate the interrupt if we have not done so yet. This
            //    interrupt handler must work for all different kinds of
            //    strips, so it delegates to the refill function for each
            //    specific instantiation of ClocklessController.
            if (gRMT_intr_handle == NULL)
                esp_intr_alloc(ETS_RMT_INTR_SOURCE, 0, rmtInterruptHandler, 0, &gRMT_intr_handle);
        }

        gInitialized = true;
    }

    // ----------------------------------------------------------------------
    //  I2S Initialization
    // ----------------------------------------------------------------------
    
    static int pgcd(int smallest,int precision,int a,int b,int c)
    {
        int pgc_=1;
        for( int i=smallest;i>0;i--) 
        {
            
            if( a%i<=precision && b%i<=precision && c%i<=precision)
            {
                pgc_=i;
                break;
            }
        }
        return pgc_;
    }
    
    static void initBitPatterns()
    {
        // Precompute the bit patterns based on the I2S sample rate
        uint32_t T1ns = ESPCLKS_TO_NS(T1);
        uint32_t T2ns = ESPCLKS_TO_NS(T2);
        uint32_t T3ns = ESPCLKS_TO_NS(T3);
        
        Serial.print("T1 = "); Serial.print(T1); Serial.print(" ns "); Serial.println(T1ns);
        Serial.print("T2 = "); Serial.print(T2); Serial.print(" ns "); Serial.println(T2ns);
        Serial.print("T3 = "); Serial.print(T3); Serial.print(" ns "); Serial.println(T3ns);
        
        /*
         We calculate the best pcgd to the timing
         ie
         WS2811 77 77 154 => 1  1 2 => nb pulses= 4
         WS2812 60 150 90 => 2 5 3 => nb pulses=10
         */
        int smallest=0;
        if (T1>T2)
            smallest=T2;
        else
            smallest=T1;
        if(smallest>T3)
            smallest=T3;
        double freq=(double)1/(double)(T1ns + T2ns + T3ns);
        Serial.printf("chipset frequency:%f Khz\n", 1000000L*freq);
        // Serial.printf("smallest %d\n",smallest);
        int pgc_=1;
        int precision=0;
        pgc_=pgcd(smallest,precision,T1,T2,T3);
        //Serial.printf("%f\n",I2S_MAX_CLK/(1000000000L*freq));
        while(pgc_==1 ||  (T1/pgc_ +T2/pgc_ +T3/pgc_)>I2S_MAX_PULSE_PER_BIT) //while(pgc_==1 ||  (T1/pgc_ +T2/pgc_ +T3/pgc_)>I2S_MAX_CLK/(1000000000L*freq))
        {
            precision++;
            pgc_=pgcd(smallest,precision,T1,T2,T3);
            //Serial.printf("%d %d\n",pgc_,(a+b+c)/pgc_);
        }
        pgc_=pgcd(smallest,precision,T1,T2,T3);
        Serial.printf("pgcd %d precision:%d\n",pgc_,precision);
        Serial.printf("nb pulse per bit:%d\n",T1/pgc_ +T2/pgc_ +T3/pgc_);
        gPulsesPerBit=(int)T1/pgc_ +(int)T2/pgc_ +(int)T3/pgc_;
        /*
         we calculate the duration of one pulse nd htre base frequency of the led
         ie WS2812B F=1/(250+625+375)=800kHz or 1250ns
         as we need 10 pulses each pulse is 125ns => frequency 800Khz*10=8MHz
         WS2811 T=320+320+641=1281ns qnd we need 4 pulses => pulse duration 320.25ns =>frequency 3.1225605Mhz
         
         */

        freq=1000000000L*freq*gPulsesPerBit;
        Serial.printf("needed frequency (nbpiulse per bit)*(chispset frequency):%f Mhz\n",freq/1000000);
        
        /*
         we do calculate the needed N a and b
         as f=basefred/(N+b/a);
         as a is max 63 the precision for the decimal is 1/63 
         */
        
        CLOCK_DIVIDER_N=(int)((double)I2S_BASE_CLK/freq);
        double v=I2S_BASE_CLK/freq-CLOCK_DIVIDER_N;

        double prec=(double)1/63;
        int a=1;
        int b=0;
        CLOCK_DIVIDER_A=1;
        CLOCK_DIVIDER_B=0;
        for(a=1;a<64;a++)
        {
            for(b=0;b<a;b++)
            {
                //printf("%d %d %f %f %f\n",b,a,v,(double)v*(double)a,fabsf(v-(double)b/a));
                if(fabsf(v-(double)b/a) <= prec/2)
                    break;
            }
            if(fabsf(v-(double)b/a) ==0)
            {
                CLOCK_DIVIDER_A=a;
                CLOCK_DIVIDER_B=b;
                break;
            }
            if(fabsf(v-(double)b/a) < prec/2)
            {
                if (fabsf(v-(double)b/a) <fabsf(v-(double)CLOCK_DIVIDER_B/CLOCK_DIVIDER_A))
                {
                    CLOCK_DIVIDER_A=a;
                    CLOCK_DIVIDER_B=b;
                }
                
            }
        }
        //top take care of an issue with double 0.9999999999
        if(CLOCK_DIVIDER_A==CLOCK_DIVIDER_B)
        {
            CLOCK_DIVIDER_A=1;
            CLOCK_DIVIDER_B=0;
            CLOCK_DIVIDER_N++;
        }
        
        //printf("%d %d %f %f %d\n",CLOCK_DIVIDER_B,CLOCK_DIVIDER_A,(double)CLOCK_DIVIDER_B/CLOCK_DIVIDER_A,v,CLOCK_DIVIDER_N);
        //Serial.printf("freq %f %f\n",freq,I2S_BASE_CLK/(CLOCK_DIVIDER_N+(double)CLOCK_DIVIDER_B/CLOCK_DIVIDER_A));
        freq=1/(CLOCK_DIVIDER_N+(double)CLOCK_DIVIDER_B/CLOCK_DIVIDER_A);
        freq=freq*I2S_BASE_CLK;
        Serial.printf("calculted for i2s frequency:%f Mhz N:%d B:%d A:%d\n",freq/1000000,CLOCK_DIVIDER_N,CLOCK_DIVIDER_B,CLOCK_DIVIDER_A);
        double pulseduration=1000000000/freq;
        Serial.printf("Pulse duration: %f ns\n",pulseduration);
        // gPulsesPerBit = (T1ns + T2ns + T3ns)/FASTLED_I2S_NS_PER_PULSE;
        
        //Serial.print("Pulses per bit: "); Serial.println(gPulsesPerBit);
        
        //int ones_for_one  = ((T1ns + T2ns - 1)/FASTLED_I2S_NS_PER_PULSE) + 1;
        ones_for_one  = T1/pgc_ +T2/pgc_;
        //Serial.print("One bit:  target ");
        //Serial.print(T1ns+T2ns); Serial.print("ns --- ");
        //Serial.print(ones_for_one); Serial.print(" 1 bits");
        //Serial.print(" = "); Serial.print(ones_for_one * FASTLED_I2S_NS_PER_PULSE); Serial.println("ns");
        Serial.printf("one bit : target %d  ns --- %d  pulses 1 bit = %f ns\n",T1ns+T2ns,ones_for_one ,ones_for_one*pulseduration);
        
        int i = 0;
        while ( i < ones_for_one ) {
            gOneBit[i] = 0xFFFFFF00;
            i++;
        }
        while ( i < gPulsesPerBit ) {
            gOneBit[i] = 0x00000000;
            i++;
        }
        
        //int ones_for_zero = ((T1ns - 1)/FASTLED_I2S_NS_PER_PULSE) + 1;
        ones_for_zero =T1/pgc_  ;
        // Serial.print("Zero bit:  target ");
        // Serial.print(T1ns); Serial.print("ns --- ");
        //Serial.print(ones_for_zero); Serial.print(" 1 bits");
        //Serial.print(" = "); Serial.print(ones_for_zero * FASTLED_I2S_NS_PER_PULSE); Serial.println("ns");
        Serial.printf("Zero bit : target %d ns --- %d pulses  1 bit =   %f ns\n",T1ns,ones_for_zero ,ones_for_zero*pulseduration);
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
        i2s->clkm_conf.clkm_div_a = CLOCK_DIVIDER_A;
        i2s->clkm_conf.clkm_div_b = CLOCK_DIVIDER_B;
        i2s->clkm_conf.clkm_div_num = CLOCK_DIVIDER_N;
        
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
                                     &i2sInterruptHandler, 0, &gI2S_intr_handle);
        
        // -- Create a semaphore to block execution until all the controllers are done
        if (gTX_sem == NULL) {
            gTX_sem = xSemaphoreCreateBinary();
            xSemaphoreGive(gTX_sem);
        }
        
        // Serial.println("Init I2S");
        gInitialized = true;
    }
    
    // -- Clear the I2S DMA buffer 
    //    Yves' trick: fill in all the bits we know ahead of time;
    //    i.e., the parts of the zero-bit and one-bit signals where
    //    they are *both* high or *both* low.
    static void empty( uint32_t *buf)
    {
        for(int i=0;i<8*NUM_COLOR_CHANNELS;i++)
        {
            int offset=gPulsesPerBit*i;
            for(int j=0;j<ones_for_zero;j++)
                buf[offset+j]=0xffffffff;
            
            for(int j=ones_for_one;j<gPulsesPerBit;j++)
                buf[offset+j]=0;
        }
    }
    
    // ----------------------------------------------------------------------
    //  Common entry point
    // ----------------------------------------------------------------------

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
        
        if (FASTLED_RMT_BUILTIN_DRIVER)
            convertAllPixelData(pixels);

        // -- Keep track of the number of strips we've seen
        gNumStarted++;

        // Serial.print("Show pixels ");
        // Serial.println(gNumStarted);
        
        // -- The last call to showPixels is the one responsible for doing
        //    all of the actual work
        if (gNumStarted == gNumControllers) {
            if (FASTLED_ESP32_I2S) {
                empty((uint32_t*)dmaBuffers[0]->buffer);
                empty((uint32_t*)dmaBuffers[1]->buffer);
                gCurBuffer = 0;
                gDoneFilling = false;
            
                // -- Prefill both buffers
                fillBuffer();
                fillBuffer();
            
                i2sStart();
            } else {
                // -- First, fill all the available channels
                gNext = 0;
                int channel = 0;
                while (channel < FASTLED_RMT_MAX_CHANNELS && gNext < gNumControllers) {
                    startNext(channel);
                    channel++;
                }
            }
                
            
            // -- Wait here while the rest of the data is sent. The interrupt handler
            //    will keep refilling the RMT buffers until it is all sent; then it
            //    gives the semaphore back.
            xSemaphoreTake(gTX_sem, portMAX_DELAY);
            xSemaphoreGive(gTX_sem);
            
            if (FASTLED_ESP32_I2S)
                i2sStop();
            
            // -- Reset the counters
            gNumStarted = 0;
            gNumDone = 0;
            gNext = 0;
        }
    }
    
    // ----------------------------------------------------------------------
    //  RMT Methods
    // ----------------------------------------------------------------------

    // -- Convert all pixels to RMT pulses
    //    This function is only used when the user chooses to use the
    //    built-in RMT driver, which needs all of the RMT pulses
    //    up-front.
    virtual void convertAllPixelData(PixelController<RGB_ORDER> & pixels)
    {
        // -- Compute the pulse values for the whole strip at once.
        //    Requires a large buffer
        mBufferSize = pixels.size() * 3 * 8;

        if (mBuffer == NULL) {
            mBuffer = (rmt_item32_t *) calloc( mBufferSize, sizeof(rmt_item32_t));
        }

        // -- Cycle through the R,G, and B values in the right order,
        //    storing the pulses in the big buffer
        mCurPulse = 0;
        int cur = 0;
        uint32_t byteval;
        while (pixels.has(1)) {
            byteval = pixels.loadAndScale0();
            convertByte(byteval);
            byteval = pixels.loadAndScale1();
            convertByte(byteval);
            byteval = pixels.loadAndScale2();
            convertByte(byteval);
            pixels.advanceData();
            pixels.stepDithering();
        }

        mBuffer[mCurPulse-1].duration1 = RMT_RESET_DURATION;
        assert(mCurPulse == mBufferSize);
    }

    void convertByte(uint32_t byteval)
    {
        // -- Write one byte's worth of RMT pulses to the big buffer
        byteval <<= 24;
        for (register uint32_t j = 0; j < 8; j++) {
            mBuffer[mCurPulse] = (byteval & 0x80000000L) ? mOne : mZero;
            byteval <<= 1;
            mCurPulse++;
        }
    }

    // -- Start up the next controller
    //    This method is static so that it can dispatch to the
    //    appropriate startOnChannel method of the given controller.
    static void startNext(int channel)
    {
        if (gNext < gNumControllers) {
            ClocklessController * pController = static_cast<ClocklessController*>(gControllers[gNext]);
            pController->startOnChannel(channel);
            gNext++;
        }
    }

    // -- Start this controller on the given channel
    //    This function just initiates the RMT write; it does not wait
    //    for it to finish.
    void startOnChannel(int channel)
    {
        // -- Assign this channel and configure the RMT
        mRMT_channel = rmt_channel_t(channel);

        // -- Store a reference to this controller, so we can get it
        //    inside the interrupt handler
        gOnChannel[channel] = this;

        // -- Assign the pin to this channel
        rmt_set_pin(mRMT_channel, RMT_MODE_TX, mPin);

        if (FASTLED_RMT_BUILTIN_DRIVER) {
            // -- Use the built-in RMT driver to send all the data in one shot
            rmt_register_tx_end_callback(doneOnChannel, 0);
            rmt_write_items(mRMT_channel, mBuffer, mBufferSize, false);
        } else {
            // -- Use our custom driver to send the data incrementally

            // -- Turn on the interrupts
            rmt_set_tx_intr_en(mRMT_channel, true);
        
            // -- Initialize the counters that keep track of where we are in
            //    the pixel data.
            mCurPulse = 0;
            mCurByte = 0;

            // -- Fill both halves of the buffer
            fillHalfRMTBuffer();
            fillHalfRMTBuffer();

            // -- Turn on the interrupts
            rmt_set_tx_intr_en(mRMT_channel, true);
            
            // -- Start the RMT TX operation
            rmt_tx_start(mRMT_channel, true);
        }
    }

    // -- A controller is done 
    //    This function is called when a controller finishes writing
    //    its data. It is called either by the custom interrupt
    //    handler (below), or as a callback from the built-in
    //    interrupt handler. It is static because we don't know which
    //    controller is done until we look it up.
    static void doneOnChannel(rmt_channel_t channel, void * arg)
    {
        ClocklessController * controller = static_cast<ClocklessController*>(gOnChannel[channel]);
        portBASE_TYPE HPTaskAwoken = 0;

        // -- Turn off output on the pin
        gpio_matrix_out(controller->mPin, 0x100, 0, 0);

        gOnChannel[channel] = NULL;
        gNumDone++;

        if (gNumDone == gNumControllers) {
            // -- If this is the last controller, signal that we are all done
            xSemaphoreGiveFromISR(gTX_sem, &HPTaskAwoken);
            if(HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
        } else {
            // -- Otherwise, if there are still controllers waiting, then
            //    start the next one on this channel
            if (gNext < gNumControllers)
                startNext(channel);
        }
    }
    
    // -- Custom interrupt handler
    //    This interrupt handler handles two cases: a controller is
    //    done writing its data, or a controller needs to fill the
    //    next half of the RMT buffer with data.
    static IRAM_ATTR void rmtInterruptHandler(void *arg)
    {
        // -- The basic structure of this code is borrowed from the
        //    interrupt handler in esp-idf/components/driver/rmt.c
        uint32_t intr_st = RMT.int_st.val;
        uint8_t channel;

        for (channel = 0; channel < FASTLED_RMT_MAX_CHANNELS; channel++) {
            int tx_done_bit = channel * 3;
            int tx_next_bit = channel + 24;

            if (gOnChannel[channel] != NULL) {

                // -- More to send on this channel
                if (intr_st & BIT(tx_next_bit)) {
                    RMT.int_clr.val |= BIT(tx_next_bit);
                    
                    // -- Refill the half of the buffer that we just finished,
                    //    allowing the other half to proceed.
                    ClocklessController * controller = static_cast<ClocklessController*>(gOnChannel[channel]);
                    controller->fillHalfRMTBuffer();
                } else {
                    // -- Transmission is complete on this channel
                    if (intr_st & BIT(tx_done_bit)) {
                        RMT.int_clr.val |= BIT(tx_done_bit);
                        doneOnChannel(rmt_channel_t(channel), 0);
                    }
                }
            }
        }
    }

    // -- Fill the RMT buffer
    //    This function fills the next 32 slots in the RMT write
    //    buffer with pixel data. It also handles the case where the
    //    pixel data is exhausted, so we need to fill the RMT buffer
    //    with zeros to signal that it's done.
    void fillHalfRMTBuffer()
    {
        uint32_t one_val = mOne.val;
        uint32_t zero_val = mZero.val;

        // -- Convert (up to) 32 bits of the raw pixel data into
        //    into RMT pulses that encode the zeros and ones.
        int pulses = 0;
        uint32_t byteval;
        while (pulses < 32 && mCurByte < mSize) {
            // -- Get one byte
            byteval = mPixelData[mCurByte++];
            byteval <<= 24;
            // Shift bits out, MSB first, setting RMTMEM.chan[n].data32[x] to the 
            // rmt_item32_t value corresponding to the buffered bit value
            for (register uint32_t j = 0; j < 8; j++) {
                uint32_t val = (byteval & 0x80000000L) ? one_val : zero_val;
                RMTMEM.chan[mRMT_channel].data32[mCurPulse].val = val;
                byteval <<= 1;
                mCurPulse++;
            }
            pulses += 8;
        }

        // -- When we reach the end of the pixel data, fill the rest of the
        //    RMT buffer with 0's, which signals to the device that we're done.
        if (mCurByte == mSize) {
            while (pulses < 32) {
                RMTMEM.chan[mRMT_channel].data32[mCurPulse].val = 0;
                mCurPulse++;
                pulses++;
            }
        }
        
        // -- When we have filled the back half the buffer, reset the position to the first half
        if (mCurPulse >= MAX_PULSES*2)
            mCurPulse = 0;
    }

    // ----------------------------------------------------------------------
    //  I2S Methods
    // ----------------------------------------------------------------------

    // -- Custom interrupt handler
    static IRAM_ATTR void i2sInterruptHandler(void *arg)
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
                
               /* for (int pulse_num = 0; pulse_num < gPulsesPerBit; pulse_num++) {
                    buf[buf_index++] = has_data_mask & ( (bit & gOneBit[pulse_num]) | (~bit & gZeroBit[pulse_num]) );*/
                //when the loop is too big  => issues in timing hence i only fill the the 1
                for(int pulse_num=ones_for_zero;pulse_num<ones_for_one;pulse_num++) {
                    buf[bitnum*gPulsesPerBit+channel*8*gPulsesPerBit+pulse_num] = has_data_mask & bit;
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

/*
 * Integration into FastLED ClocklessController 2017 Thomas Basler
 *
 * Modifications Copyright (c) 2017 Martin F. Falatic
 *
 * Modifications Copyright (c) 2018 Samuel Z. Guyer
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
 * channels, which can be programmed independently with sequences of
 * high/low bits. Memory for each channel is limited, however, so in
 * order to send a long sequence of bits, we need to continuously
 * refill the buffer until all the data is sent. To do this, we fill
 * half the buffer and then set an interrupt to go off when that half
 * is sent. Then we refill that half while the second half is being
 * sent. This strategy effectively overlaps computation (by the CPU)
 * and communication (by the RMT).
 *
 * PARALLEL vs SERIAL
 *
 * By default, this driver sends the data for all LED strips in
 * parallel. We get parallelism essentially for free because the RMT
 * is an independent processing unit. It only interrupts the CPU when
 * it needs more data to send, and the CPU is fast enough to keep all
 * 8 channels filled.
 *
 * However, there may be cases where you want serial output -- that
 * is, you want to send the data for each strip before moving on to
 * the next one. The performance will be much lower, limiting the
 * framerate. To force serial output, add this directive before you
 * include FastLED.h:
 *
 *      #define FASTLED_RMT_SERIAL_OUTPUT
 *
 * OTHER RMT APPLICATIONS
 *
 * The default FastLED driver takes over control of the RMT
 * interrupts, making it hard to use the RMT device for other
 * (non-FastLED) purposes. You can change it's behavior to use the ESP
 * core driver instead, allowing other RMT applications to
 * co-exist. To switch to this mode, add the following directive
 * before you include FastLED.h:
 *
 *      #define FASTLED_RMT_CORE_DRIVER
 *
 * There is a performance penalty for using this mode. We need to
 * compute the RMT signal for the entire LED strip ahead of time,
 * rather than overlapping it with communication. We also need a large
 * buffer to hold the signal specification. Each bit of pixel data is
 * represented by a 32-bit pulse specification, so it is a 32X blow-up
 * in memory use.
 *
 * This driver assigns channels to LED strips sequentially starting at
 * zero. So, for other RMT applications make sure to choose a channel
 * at the higher end to avoid collisions.
 *
 * Based on public domain code created 19 Nov 2016 by Chris Osborn <fozztexx@fozztexx.com>
 * http://insentricity.com *
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

#include "esp32-hal.h"
#include "esp_intr.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "driver/periph_ctrl.h"
#include "freertos/semphr.h"
#include "soc/rmt_struct.h"

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

// -- Configuration constants
#define DIVIDER             2 /* 4, 8 still seem to work, but timings become marginal */
#define MAX_PULSES         32 /* A channel has a 64 "pulse" buffer - we use half per pass */

// -- Convert ESP32 cycles back into nanoseconds
#define ESPCLKS_TO_NS(_CLKS) (((long)(_CLKS) * 1000L) / F_CPU_MHZ)

// -- Convert nanoseconds into RMT cycles
#define F_CPU_RMT       (  80000000L)
#define NS_PER_SEC      (1000000000L)
#define CYCLES_PER_SEC  (F_CPU_RMT/DIVIDER)
#define NS_PER_CYCLE    ( NS_PER_SEC / CYCLES_PER_SEC )
#define NS_TO_CYCLES(n) ( (n) / NS_PER_CYCLE )

// -- Convert ESP32 cycles to RMT cycles
#define TO_RMT_CYCLES(_CLKS) NS_TO_CYCLES(ESPCLKS_TO_NS(_CLKS))    

// -- Number of cycles to reset the strip
#define RMT_RESET_DURATION NS_TO_CYCLES(50000)

// -- Parallel or serial outut
#ifndef FASTLED_RMT_SERIAL_OUTPUT
#define FASTLED_RMT_SERIAL_OUTPUT false
#endif

// -- Core or custom driver
#ifndef FASTLED_RMT_CORE_DRIVER
#define FASTLED_RMT_CORE_DRIVER false
#endif

// -- Global counter of channels used
//    Each FastLED.addLeds uses the next consecutive channel
static uint8_t gNextChannel;

// -- Global information for the interrupt handler
//    Information is indexed by the RMT channel, so we can get it 
//    when we are in the interrupt handler.
static CLEDController * gControllers[8];

typedef void (*RefillDispatcher_t)(uint8_t);
static RefillDispatcher_t gRefillFunctions[8];

static intr_handle_t gRMT_intr_handle;

// -- Parallelize the output This works because most of the work of
//    pumping out the bits is handled by the RMT peripheral, which we
//    keep filled by responding to interrupts. All we need to do is
//    detect when all of the channels have finished.

// -- Global semaphore for the whole show process
//    Only used in parallel output, to signal when all controllers are done
static xSemaphoreHandle gTX_sem = NULL;

// -- Globals to keep track of how many controllers have started and
//    how many have finished
static int gNumControllers = 0;
static int gNumShowing = 0;
static int gNumDone = 0;

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER>
{
    // -- RMT has 8 channels, numbered 0 to 7
    rmt_channel_t mRMT_channel;

    // -- Semaphore to signal when show() is done
    //    Per-controller, so only needed for serial output
    xSemaphoreHandle mTX_sem = NULL;

    // -- Timing values for zero and one bits
    rmt_item32_t mZero;
    rmt_item32_t mOne;

    // -- State information for keeping track of where we are in the pixel data
    PixelController<RGB_ORDER> * mPixels = NULL;
    void * mPixelSpace = NULL;
    uint8_t mRGB_channel;
    uint16_t mCurPulse;
    CMinWait<WAIT_TIME> mWait;

    // -- Buffer to hold all of the pulses. For the version that uses
    //    the RMT driver built into the ESP core.
    rmt_item32_t * mBuffer;
    uint16_t mBufferSize;

public:

    virtual void init()
    {
        // -- Precompute rmt items corresponding to a zero bit and a one bit
        //    according to the timing values given in the template instantiation
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

        // -- First time though: initialize the globals
        if (gNextChannel == 0) {
            for (int i = 0; i < 8; i++) {
                gControllers[8] = 0;
                gRefillFunctions[8] = 0;
            }
        }

        // -- Sequentially assign RMT channels -- at most 8
        mRMT_channel =  (rmt_channel_t) gNextChannel++;
        if (mRMT_channel > 7) {
            assert("Only 8 RMT Channels are allowed");
        }

        gNumControllers++;

        // -- Save this controller object, indexed by the RMT channel
        //    This allows us to get the pointer inside the interrupt handler
        gControllers[mRMT_channel] = this;
        gRefillFunctions[mRMT_channel] = &refillDispatcher;

        ESP_LOGI("fastled", "RMT Channel Init: %d", mRMT_channel);

        // -- RMT configuration for transmission
        rmt_config_t rmt_tx;
        rmt_tx.channel = mRMT_channel;
        rmt_tx.rmt_mode = RMT_MODE_TX;
        rmt_tx.gpio_num = gpio_num_t(DATA_PIN);
        rmt_tx.mem_block_num = 1;
        rmt_tx.clk_div = DIVIDER;
        rmt_tx.tx_config.loop_en = false;
        rmt_tx.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
        rmt_tx.tx_config.carrier_en = false;
        rmt_tx.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
        rmt_tx.tx_config.idle_output_en = true;
        
        // -- Apply the configuration
        rmt_config(&rmt_tx);

        // -- Allocate space for a cope of the pixels
        // mPixelSpace = malloc(sizeof(PixelController<RGB_ORDER>));

        if (FASTLED_RMT_CORE_DRIVER) {
            // -- Use the built-in RMT driver. The only reason to choose
            //    this option is if you have other parts of your code that
            //    are using the RMT peripheral, and you want them to
            //    co-exist with FastLED.
            rmt_driver_install(mRMT_channel, 0, 0);
        } else {
            // -- Use the custom RMT driver implemented here, which computes
            //    pulses on demand to reduce memory requirements and latency.

            // -- Set up the RMT to send 1/2 of the pulse buffer and then
            //    generate an interrupt. When we get this interrupt we
            //    fill the other half in preparation (kind of like double-buffering)
            rmt_set_tx_thr_intr_en(mRMT_channel, true, MAX_PULSES);

            // -- Turn on the interrupts
            rmt_set_tx_intr_en(mRMT_channel, true);

            // -- Semaphore to signal completion of each show()
            //    Only needed for serial output
            mTX_sem = xSemaphoreCreateBinary();
            xSemaphoreGive(mTX_sem);
        }
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:

    virtual void showPixels(PixelController<RGB_ORDER> & pixels)
    {
        mWait.wait();

        gNumShowing++;

        if (FASTLED_RMT_CORE_DRIVER) {
            // === Built-in RMT driver ===

            //    Fill a big buffer with all of the pixel data
            mBufferSize = pixels.size() * 3 * 8;
            computeAllRMTItems(pixels);

            // -- Serial or parallel
            bool wait_done;

            if (FASTLED_RMT_SERIAL_OUTPUT) {
                wait_done = true;
            } else {
                // -- Parallel: only wait on the last channel
                wait_done = (gNumShowing == gNumControllers);
            }

            // -- Send it all at once using the built-in RMT driver
            rmt_write_items(mRMT_channel, mBuffer, mBufferSize, wait_done);

        } else {
            // === Custom RMT driver ===

            if (FASTLED_RMT_SERIAL_OUTPUT) {
                // -- Local semaphore just for this controller
                xSemaphoreTake(mTX_sem, portMAX_DELAY);
            } else {
                // -- Create a global semaphore that signals when all the
                //    controllers are done
                if (gTX_sem == NULL) {
                    gTX_sem = xSemaphoreCreateBinary();
                    xSemaphoreGive(gTX_sem);
                }
                if (gNumShowing == 1) {
                    xSemaphoreTake(gTX_sem, portMAX_DELAY);
                }
            }

            // -- Initialize the local state, save a pointer to the pixel
            //    data. We need to make a copy because pixels is a local
            //    variable in the calling function, and this data structure
            //    needs to outlive this call to showPixels.
            // mPixels = new (mPixelSpace) PixelController<RGB_ORDER>(pixels);
            if (mPixels != NULL) 
                delete mPixels;
            mPixels = new PixelController<RGB_ORDER>(pixels);
            mCurPulse = 0;
            mRGB_channel = 0;

            // -- Fill both halves of the buffer
            fillHalfRMTBuffer();
            fillHalfRMTBuffer();

            // -- Allocate the interrupt if we have not done so yet. This
            //    interrupt handler must work for all different kinds of
            //    strips, so it delegates to the refill function for each
            //    specific instantiation of ClocklessController.
            if (gRMT_intr_handle == NULL)
                esp_intr_alloc(ETS_RMT_INTR_SOURCE, 0, interruptHandler, 0, &gRMT_intr_handle);

            // -- Turn on the interrupts
            rmt_set_tx_intr_en(mRMT_channel, true);

            // -- Start the RMT TX operation
            rmt_tx_start(mRMT_channel, true);

            if (FASTLED_RMT_SERIAL_OUTPUT) {
                // -- Block until this controller is done
                //    All of the data transmission happens while we wait here
                xSemaphoreTake(mTX_sem, portMAX_DELAY);
                xSemaphoreGive(mTX_sem);
        
                // -- Turn off the interrupts
                rmt_set_tx_intr_en(mRMT_channel, false);
            } else {
                // -- If this is the last controller, then this is the place to
                //    wait for all the data to be sent.
                if (gNumShowing == gNumControllers) {
                    xSemaphoreTake(gTX_sem, portMAX_DELAY);
                    xSemaphoreGive(gTX_sem);
                }
            }
        }

        // -- All controllers are done: reset the counters
        if (gNumShowing == gNumControllers) {
            gNumDone = 0;
            gNumShowing = 0;
        }

        mWait.mark();
    }

    static IRAM_ATTR void interruptHandler(void *arg)
    {
        // -- The basic structure of this code is borrowed from the
        //    interrupt handler in esp-idf/components/driver/rmt.c
        uint32_t intr_st = RMT.int_st.val;
        uint8_t channel;
        portBASE_TYPE HPTaskAwoken = 0;

        for (channel = 0; channel < 8; channel++) {
            int tx_done_bit = channel * 3;
            int tx_next_bit = channel + 24;

            if (intr_st & BIT(tx_done_bit)) {
                // -- Transmission is complete on this channel
                RMT.int_clr.val |= BIT(tx_done_bit);
                gNumDone++;

                if (FASTLED_RMT_SERIAL_OUTPUT) {
                    // -- Serial mode: unblock the call to showPixels for this strip
                    ClocklessController * controller = static_cast<ClocklessController*>(gControllers[channel]);
                    xSemaphoreGiveFromISR(controller->mTX_sem, &HPTaskAwoken);
                } else {
                    // -- Parallel mode: unblock the global semaphore when all strips are done
                    if (gNumDone == gNumControllers)
                        xSemaphoreGiveFromISR(gTX_sem, &HPTaskAwoken);
                }

                if(HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
            }

            if (intr_st & BIT(tx_next_bit)) {
                // -- More to send on this channel: call the appropriate refill function
                //    Note that we refill the half of the buffer that we just finished,
                //    allowing the other half to proceed.
                RMT.int_clr.val |= BIT(tx_next_bit);
                (gRefillFunctions[channel])(channel);
            }
        }
    }

    /* Refill the RMT buffer
     * We need this dispatch function because there will be one for each instantiation of this template
     * class -- in particular, one for each possible RGB_ORDER. We need to dispatch to the correct one
     * so that fillHalfRMTBuffer will use the right ordering for this strip.
     */
    static IRAM_ATTR void refillDispatcher(uint8_t channel)
    {
        ClocklessController * controller = static_cast<ClocklessController*>(gControllers[channel]);
        controller->fillHalfRMTBuffer();
    }

    IRAM_ATTR void fillHalfRMTBuffer()
    {
        // -- Fill half of the RMT pulse buffer

        //    The buffer holds 64 total pulse items, so this loop converts
        //    as many pixels as can fit in half of the buffer (MAX_PULSES =
        //    32 items). In our case, each pixel consists of three bytes,
        //    each bit turns into one pulse item -- 24 items per pixel. So,
        //    each half of the buffer can hold 1 and 1/3 of a pixel.

        //    The member variable mCurPulse keeps track of which of the 64
        //    items we are writing. During the first call to this method it
        //    fills 0-31; in the second call it fills 32-63, and then wraps
        //    back around to zero.

        //    When we run out of pixel data, just fill the remaining items
        //    with zero pulses.

        uint16_t pulse_count = 0; // Ranges from 0-31 (half a buffer)
        uint32_t byteval = 0;
        uint32_t one_val = mOne.val;
        uint32_t zero_val = mZero.val;
        bool done_strip = false;

        while (pulse_count < MAX_PULSES) {
            if (! mPixels->has(1)) {
                done_strip = true;
                break;
            }

            // -- Cycle through the R,G, and B values in the right order
            switch (mRGB_channel) {
            case 0:
                byteval = mPixels->loadAndScale0();
                mRGB_channel = 1;
                break;
            case 1:
                byteval = mPixels->loadAndScale1();
                mRGB_channel = 2;
                break;
            case 2:
                byteval = mPixels->loadAndScale2();
                mPixels->advanceData();
                mPixels->stepDithering();
                mRGB_channel = 0;
                break;
            default:
                break;
            }

            byteval <<= 24;
            // Shift bits out, MSB first, setting RMTMEM.chan[n].data32[x] to the 
            // rmt_item32_t value corresponding to the buffered bit value
            for (register uint32_t j = 0; j < 8; j++) {
                uint32_t val = (byteval & 0x80000000L) ? one_val : zero_val;
                RMTMEM.chan[mRMT_channel].data32[mCurPulse].val = val;
                byteval <<= 1;
                mCurPulse++;
                pulse_count++;
            }
        }
        
        // -- At the end, stretch out the last pulse to signal to the strip
        //    that we're done
        if (done_strip) {
            RMTMEM.chan[mRMT_channel].data32[mCurPulse-1].duration1 = RMT_RESET_DURATION;

            // -- And fill the remaining items with zero pulses. The zero values triggers
            //    the tx_done interrupt.
            while (pulse_count < MAX_PULSES) {
                RMTMEM.chan[mRMT_channel].data32[mCurPulse].val = 0;
                mCurPulse++;
                pulse_count++;
            }
        }

        // -- When we have filled the back half the buffer, reset the position to the first half
        if (mCurPulse >= MAX_PULSES*2)
            mCurPulse = 0;
    }
    
    void computeAllRMTItems(PixelController<RGB_ORDER> & pixels)
    {
        // -- Compute the pulse values for the whole strip at once.
        //    Requires a large buffer

        // TODO: need a specific number here
        if (mBuffer == NULL) {
            mBuffer = (rmt_item32_t *) calloc( mBufferSize, sizeof(rmt_item32_t));
        }

        mCurPulse = 0;
        mRGB_channel = 0;
        uint32_t byteval = 0;
        while (pixels.has(1)) {
            // -- Cycle through the R,G, and B values in the right order
            switch (mRGB_channel) {
            case 0:
                byteval = pixels.loadAndScale0();
                mRGB_channel = 1;
                break;
            case 1:
                byteval = pixels.loadAndScale1();
                mRGB_channel = 2;
                break;
            case 2:
                byteval = pixels.loadAndScale2();
                pixels.advanceData();
                pixels.stepDithering();
                mRGB_channel = 0;
                break;
            default:
                break;
            }

            byteval <<= 24;
            // Shift bits out, MSB first, setting RMTMEM.chan[n].data32[x] to the 
            // rmt_item32_t value corresponding to the buffered bit value
            for (register uint32_t j = 0; j < 8; j++) {
                mBuffer[mCurPulse] = (byteval & 0x80000000L) ? mOne : mZero;
                byteval <<= 1;
                mCurPulse++;
            }
        }

        mBuffer[mCurPulse-1].duration1 = RMT_RESET_DURATION;
        assert(mCurPulse == mBufferSize);
    }
};

FASTLED_NAMESPACE_END

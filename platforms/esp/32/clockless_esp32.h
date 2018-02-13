/*
 * Integration into FastLED ClocklessController 2017 Thomas Basler
 *
 * Modifications Copyright (c) 2017 Martin F. Falatic
 * and Samuel Z. Guyer
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

// -- Global information for the interrupt handler
static void * gControllers[8];
static intr_handle_t gRMT_intr_handle;
static uint8_t gNext_channel;


template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER>
{
    // -- RMT has 8 channels, numbered 0 to 7
    rmt_channel_t mRMT_channel;

    // -- Semaphore to signal when show() is done
    xSemaphoreHandle mTX_sem = NULL;

    // -- Timing values for zero and one bits
    rmt_item32_t mZero;
    rmt_item32_t mOne;

    // -- State information for keeping track of where we are in the pixel data
    PixelController<RGB_ORDER> * mPixels  = NULL;
    uint8_t mRGB_channel;
    uint16_t mCurPulse;
    CMinWait<WAIT_TIME> mWait;

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

        // -- Sequentially assign RMT channels -- at most 8
        mRMT_channel =  (rmt_channel_t) gNext_channel++;
        if (mRMT_channel > 7) {
            assert("Only 8 RMT Channels are allowed");
        }

        // -- Save this controller object, indexed by the RMT channel
        //    This allows us to get the pointer inside the interrupt handler
        gControllers[mRMT_channel] = this;

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

        // -- Set up the RMT to send 1/2 of the pulse buffer and then
        //    generate an interrupt. When we get this interrupt we
        //    fill the other half in preparation (kind of like double-buffering)
        rmt_set_tx_thr_intr_en(mRMT_channel, true, MAX_PULSES);

        // -- Semaphore to signal completion of each show()
        mTX_sem = xSemaphoreCreateBinary();
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:

    virtual void showPixels(PixelController<RGB_ORDER> & pixels)
    {
        mWait.wait();

        // -- Initialize the local state, save a pointer to the pixel data
        mPixels = &pixels;
        mCurPulse = 0;
        mRGB_channel = 0;

        // -- Fill both halves of the buffer
        fillHalfRMTBuffer();
        fillHalfRMTBuffer();

        // -- Allocate the interrupt if we have not done so yet
        if (gRMT_intr_handle == NULL)
            esp_intr_alloc(ETS_RMT_INTR_SOURCE, 0, handleInterrupt, 0, &gRMT_intr_handle);

        // -- Turn on the interrupts
        rmt_set_tx_intr_en(mRMT_channel, true);

        // -- Start the RMT TX operation
        rmt_tx_start(mRMT_channel, true);

        // -- Block until done
        //    All of the data transmission happens while we wait here
        xSemaphoreTake(mTX_sem, portMAX_DELAY);

        // -- Turn off the interrupts
        rmt_set_tx_intr_en(mRMT_channel, false);

        mWait.mark();
    }

    static void handleInterrupt(void *arg)
    {
        // -- The basic structure of this code is borrowed from the
        //    interrupt handler in esp-idf/components/driver/rmt.c
        uint32_t intr_st = RMT.int_st.val;
        uint32_t i = 0;
        uint8_t channel;
        portBASE_TYPE HPTaskAwoken = 0;

        // -- Loop over all the bits in the interrupt status word; the particular
        //    bit set indicates both the meaning and the RMT channel to which it applies
        for(i = 0; i < 32; i++) {
            if(i < 24) {
                // -- The low 24 bits consist of 3 bits per channel that indicate that
                //    when a tx/rx operation completes
                if(intr_st & BIT(i)) {
                    channel = i / 3;
                    if (i % 3 == 0) {
                        // -- Transmission is complete, signal the semaphore that show() is finished
                        ClocklessController * controller = static_cast<ClocklessController*>(gControllers[channel]);
                        xSemaphoreGiveFromISR(controller->mTX_sem, &HPTaskAwoken);
                        if(HPTaskAwoken == pdTRUE) portYIELD_FROM_ISR();
                    }
                    RMT.int_clr.val = BIT(i);
                }
            } else {
                // -- The high 8 bits signal that the current 1/2 buffer has been sent, so we should
                //    fill the next half and continue.
                if(intr_st & (BIT(i))) {
                    channel = i - 24;
                    ClocklessController * controller = static_cast<ClocklessController*>(gControllers[channel]);
                    controller->fillHalfRMTBuffer();
                    RMT.int_clr.val = BIT(i);
                }
            }
        }
    }

    void fillHalfRMTBuffer()
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
        uint16_t pulse_count = 0;
        uint32_t byteval = 0;
        while (mPixels->has(1) && pulse_count < MAX_PULSES) {
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
                uint32_t val = (byteval & 0x80000000L) ? mOne.val : mZero.val;
                RMTMEM.chan[mRMT_channel].data32[mCurPulse].val = val;
                byteval <<= 1;
                mCurPulse++;
                pulse_count++;
            }
        }
        
        // -- At the end, stretch out the last pulse to signal to the strip
        //    that we're done
        if ( ! mPixels->has(1)) {
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
};

FASTLED_NAMESPACE_END

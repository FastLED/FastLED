/*
 * Integration into FastLED ClocklessController 2017 Thomas Basler
 *
 * Modifications Copyright (c) 2017 Martin F. Falatic
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

#define FASTLED_HAS_CLOCKLESS 1

// -- Configuration constants
#define DIVIDER             4 /* 8 still seems to work, but timings become marginal */
#define MAX_PULSES         32 /* A channel has a 64 "pulse" buffer - we use half per pass */
#define RMT_DURATION_NS  12.5 /* minimum time of a single RMT duration based on clock ns */

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

static uint8_t rmt_channels_used = 0;

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER>
{
    rmt_item32_t mZero;
    rmt_item32_t mOne;

    rmt_channel_t mRMT_channel;
    xSemaphoreHandle mTX_sem = NULL;
    intr_handle_t mRMT_intr_handle = NULL;
    
    PixelController<RGB_ORDER> *local_pixels  = NULL;
    uint8_t mRGB_channel;
    uint16_t mCurPulse;

public:

    virtual void init()
    {
	// TRS = 50000;

	// -- Precompute rmt items corresponding to a zero bit and a one bit
	//    according to the timing values given in the template instantiation
	// T1H
	mOne.level0 = 1;
	mOne.duration0 = TO_RMT_CYCLES(T1+T2); // 900
	// T1L
	mOne.level1 = 0;
	mOne.duration1 = TO_RMT_CYCLES(T3); // 600

	// T0H
	mZero.level0 = 1;
	mZero.duration0 = TO_RMT_CYCLES(T1); // 400
	// T0L
	mZero.level1 = 0;
	mZero.duration1 = TO_RMT_CYCLES(T2 + T3); // 900

	// -- Sequentially assign RMT channels -- at most 8
	mRMT_channel =  (rmt_channel_t) rmt_channels_used++;
	if (mRMT_channel > 7) {
	    assert("Only 8 RMT Channels are allowed");
	}

	ESP_LOGI("fastled", "RMT Channel Init: %d", mRMT_channel);

	// -- RMT set up magic
	DPORT_SET_PERI_REG_MASK(DPORT_PERIP_CLK_EN_REG, DPORT_RMT_CLK_EN);
	DPORT_CLEAR_PERI_REG_MASK(DPORT_PERIP_RST_EN_REG, DPORT_RMT_RST);

	rmt_set_pin(static_cast<rmt_channel_t>(mRMT_channel),
		    RMT_MODE_TX,
		    static_cast<gpio_num_t>(DATA_PIN));

	RMT.apb_conf.fifo_mask = 1;  //enable memory access, instead of FIFO mode.
	RMT.apb_conf.mem_tx_wrap_en = 1; //wrap around when hitting end of buffer
	
	RMT.conf_ch[mRMT_channel].conf0.div_cnt = DIVIDER;
	RMT.conf_ch[mRMT_channel].conf0.mem_size = 1;
	RMT.conf_ch[mRMT_channel].conf0.carrier_en = 0;
	RMT.conf_ch[mRMT_channel].conf0.carrier_out_lv = 1;
	RMT.conf_ch[mRMT_channel].conf0.mem_pd = 0;
	RMT.conf_ch[mRMT_channel].conf1.rx_en = 0;
	RMT.conf_ch[mRMT_channel].conf1.mem_owner = 0;
	RMT.conf_ch[mRMT_channel].conf1.tx_conti_mode = 0;    //loop back mode.
	RMT.conf_ch[mRMT_channel].conf1.ref_always_on = 1;    // use apb clock: 80M
	RMT.conf_ch[mRMT_channel].conf1.idle_out_en = 1;
	RMT.conf_ch[mRMT_channel].conf1.idle_out_lv = 0;
		
	RMT.tx_lim_ch[mRMT_channel].limit = MAX_PULSES;
	
	RMT.int_ena.val |= BIT(24 + mRMT_channel); // set ch*_tx_thr_event
	RMT.int_ena.val |= BIT(mRMT_channel * 3); // set ch*_tx_end
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:

    virtual void showPixels(PixelController<RGB_ORDER> & pixels)
    {
	esp_intr_alloc(ETS_RMT_INTR_SOURCE, 0, handleInterrupt, this, &mRMT_intr_handle);

	// -- Initialize the local state, save a pointer to the pixel data
	local_pixels = &pixels;
	mCurPulse = 0;
	mRGB_channel = 0;
		
	// -- Fill both halves of the buffer
	copyToRmtBlock_half();
	copyToRmtBlock_half();

	mTX_sem = xSemaphoreCreateBinary();

	// -- Start the RMT TX operationb
	RMT.conf_ch[mRMT_channel].conf1.mem_rd_rst = 1;
	RMT.conf_ch[mRMT_channel].conf1.tx_start = 1;

	// -- Block until done
	xSemaphoreTake(mTX_sem, portMAX_DELAY);

	// -- When we get here, all of the data has been sent
	vSemaphoreDelete(mTX_sem);
	mTX_sem = NULL;

	esp_intr_free(mRMT_intr_handle);
    }

    static void handleInterrupt(void *arg)
    {
	ClocklessController* c = static_cast<ClocklessController*>(arg);
	rmt_channel_t rmt_channel = c->mRMT_channel;

	portBASE_TYPE xHigherPriorityTaskWoken  = 0;

	if (RMT.int_st.val & BIT(24 + rmt_channel)) { // check if ch*_tx_thr_event is set
	    // -- Interrupt is telling us the RMT is ready for the next set of pulses
	    c->copyToRmtBlock_half();
	    RMT.int_clr.val |= BIT(24 + rmt_channel); // set ch*_tx_thr_event
	}
	else if ((RMT.int_st.val & BIT(rmt_channel * 3)) && c->mTX_sem) { // check if ch*_tx_end is set
	    // -- Interrupt is telling us the RMT is done -- release the semaphore
	    xSemaphoreGiveFromISR(c->mTX_sem, &xHigherPriorityTaskWoken);
	    RMT.int_clr.val |= BIT(rmt_channel * 3); // set ch*_tx_end

	    if (xHigherPriorityTaskWoken == pdTRUE) {
		portYIELD_FROM_ISR();
	    }
	}
    }

    void copyToRmtBlock_half()
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
	while (local_pixels->has(1) && pulse_count < MAX_PULSES) {
	    // -- Cycle through the R,G, and B values in the right order
	    switch (mRGB_channel) {
	    case 0:
		byteval = local_pixels->loadAndScale0();
		mRGB_channel = 1;
		break;
	    case 1:
		byteval = local_pixels->loadAndScale1();
		mRGB_channel = 2;
		break;
	    case 2:
		byteval = local_pixels->loadAndScale2();
		local_pixels->advanceData();
		local_pixels->stepDithering();
		mRGB_channel = 0;
		break;
	    default:
		break;
	    }

	    byteval <<= 24;
	    // Shift bits out, MSB first, setting RMTMEM.chan[n].data32[x] to the rmt_item32_t value corresponding to the buffered bit value
	    for (register uint32_t j = 0; j < 8; j++) {
		uint32_t val = (byteval & 0x80000000L) ? mOne.val : mZero.val;
		RMTMEM.chan[mRMT_channel].data32[mCurPulse].val = val;
		byteval <<= 1;
		mCurPulse++;
		pulse_count++;
	    }
	}
	
	// -- Fill the remaining items with zero pulses
	while (pulse_count < MAX_PULSES) {
	    RMTMEM.chan[mRMT_channel].data32[mCurPulse].val = 0;
	    mCurPulse++;
	    pulse_count++;
	}

	// -- When we have filled the back half the buffer, reset the position to the first half
	if (mCurPulse >= MAX_PULSES*2)
	    mCurPulse = 0;
    }
};

FASTLED_NAMESPACE_END

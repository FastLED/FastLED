#pragma once

FASTLED_NAMESPACE_BEGIN

#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
extern uint32_t _frame_cnt;
extern uint32_t _retry_cnt;
#endif

#include <driver/rmt.h>

// RMT Clock source is @ 80 MHz. Dividing it by 4 gives us 20 MHz frequency, or 50ns period.
#define LED_STRIP_RMT_CLK_DIV  4   /* 8 still seems to work, but timings become marginal */
#define RMT_DURATION_NS       12.5 /* minimum time of a single RMT duration based on clock ns */

// These macros help us convert from ESP32 clock cycles to RMT "ticks"
#define PERIOD  50  /* RMT_DURATION_NS * LED_STRIP_RMT_CLK_DIV */
#define TO_NS(_CLKS) (((((long)(_CLKS)) * 1000 - 999) / F_CPU_MHZ))

#define RMT_MAX_WAIT 100  // Should really figure out how many ticks in 45us
#define RMT_ITEMS_SIZE (20 * 3 * 8)  // Number of RMT items for 20 pixels -- 1840 bytes

static int Next_RMT_Channel = 0;

// Info on reading cycle counter from https://github.com/kbeckmann/nodemcu-firmware/blob/ws2812-dual/app/modules/ws2812.c
__attribute__ ((always_inline)) inline static uint32_t __clock_cycles() {
  uint32_t cyc;
  __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc));
  return cyc;
}

#define FASTLED_HAS_CLOCKLESS 1

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 5>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {

    typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPin<DATA_PIN>::port_t data_t;

    data_t mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;

    uint16_t mT0H, mT1H, mT0L, mT1L;
    rmt_channel_t mLED_RMT_CHANNEL;
    rmt_config_t mRMT_config;    
    rmt_item32_t mRMT_items[RMT_ITEMS_SIZE];

public:
    virtual void init() {
	// -- Assign RMT channels sequentially
	if (Next_RMT_Channel > 7) {
	    Serial.println("ERROR: Not enough RMT channels!");
	}
	mLED_RMT_CHANNEL = (rmt_channel_t) Next_RMT_Channel;
	Next_RMT_Channel++;

	FastPin<DATA_PIN>::setOutput();
	mPinMask = FastPin<DATA_PIN>::mask();
	mPort = FastPin<DATA_PIN>::port();

	// -- Compute the timing values
	//    We are converting from ESP32 clock cycles (~4ns) to RMT peripheral ticks (12.5ns)
	mT0H = TO_NS(T1) / PERIOD;
	mT1H = TO_NS(T1 + T2) / PERIOD;
	mT0L = TO_NS(T2 + T3) / PERIOD;
	mT1L = TO_NS(T3) / PERIOD;

	// -- Set up the RMT peripheral
	mRMT_config.rmt_mode = RMT_MODE_TX;
	mRMT_config.channel = mLED_RMT_CHANNEL;
	mRMT_config.clk_div = LED_STRIP_RMT_CLK_DIV;
	mRMT_config.gpio_num = (gpio_num_t) DATA_PIN;
	mRMT_config.mem_block_num = 1;

	mRMT_config.tx_config.loop_en = false;
	mRMT_config.tx_config.carrier_freq_hz = 100; // Not used, but has to be set to avoid divide by 0 err
	mRMT_config.tx_config.carrier_duty_percent = 50;
	mRMT_config.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
	mRMT_config.tx_config.carrier_en = false;
	mRMT_config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
	mRMT_config.tx_config.idle_output_en = true;

	esp_err_t cfg_ok = rmt_config(&mRMT_config);
	if (cfg_ok != ESP_OK) {
	    Serial.println("RMT config failed");
	    return;
	}
	esp_err_t install_ok = rmt_driver_install(mRMT_config.channel, 0, 0);
	if (install_ok != ESP_OK) {
	    Serial.println("RMT driver install failed");
	    return;
	}
    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:

    virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
	mWait.wait();
	int cnt = FASTLED_INTERRUPT_RETRY_COUNT;
	while((showRGBInternal(pixels)==0) && cnt--) {
#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
	    _retry_cnt++;
#endif
	    // ets_intr_unlock();
	    delayMicroseconds(WAIT_TIME);
	    // ets_intr_lock();
	}
	mWait.mark();
    }

#define _ESP_ADJ (0)
#define _ESP_ADJ2 (0)

    // -- convertBit
    //    Translate a single bit into an RMT signal entry using the given timing variables
    __attribute__ ((always_inline)) 
    inline void convertBit(rmt_item32_t * item, register uint32_t b) 
    {
	if (b & 0x80000000L) {
	    item->level0 = 1;
	    item->duration0 = mT1H; // LED_STRIP_RMT_TICKS_BIT_1_HIGH_WS2812;
	    item->level1 = 0;
	    item->duration1 = mT1L; // LED_STRIP_RMT_TICKS_BIT_1_LOW_WS2812;
	} else {
	    item->level0 = 1;
	    item->duration0 = mT0H; // LED_STRIP_RMT_TICKS_BIT_0_HIGH_WS2812;
	    item->level1 = 0;
	    item->duration1 = mT0L; // LED_STRIP_RMT_TICKS_BIT_0_LOW_WS2812;
	}
    }
    
    uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
	
	int start_item = 0;
	rmt_item32_t * cur_item = & mRMT_items[0];

	// Setup the pixel controller and load/scale the first byte
	pixels.preStepFirstByteDithering();
	register uint32_t b = pixels.loadAndScale0();
	pixels.preStepFirstByteDithering();

	uint32_t start = __clock_cycles();
	while (pixels.has(1)) {

	    // -- Prepare a chunk of RMT items for no more than 10 pixels
	    int num_items = 0;
	    while (pixels.has(1) && (num_items < (RMT_ITEMS_SIZE/2))) {

		// Write first byte, read next byte
		b <<= 24;
		for (register uint32_t i = 8; i > 0; i--) {
		    convertBit(cur_item, b);
		    cur_item++;
		    num_items++;
		    b <<= 1;
		}
		b = pixels.loadAndScale1();
	    
		// Write second byte, read 3rd byte
		b <<= 24;
		for (register uint32_t i = 8; i > 0; i--) {
		    convertBit(cur_item, b);
		    cur_item++;
		    num_items++;
		    b <<= 1;
		}
		b = pixels.loadAndScale2();
	    
		// Write third byte, read 1st byte of next pixel
		b <<= 24;
		for (register uint32_t i = 8; i > 0; i--) {
		    convertBit(cur_item, b);
		    cur_item++;
		    num_items++;
		    b <<= 1;
		}
		b = pixels.advanceAndLoadAndScale0();
	    
		pixels.stepDithering();   
	    }

	    // -- Wait for the previous chunk of 10 items to finish
	    rmt_wait_tx_done(mLED_RMT_CHANNEL, RMT_MAX_WAIT);

	    // -- Send the new chunk
	    rmt_write_items(mLED_RMT_CHANNEL, &mRMT_items[start_item], num_items, false);

	    // -- Shift the window foward to compute the next chunk, wrapping around
            //    as necessary
	    start_item += num_items;
	    if (start_item >= RMT_ITEMS_SIZE) start_item = 0;
	    cur_item = & mRMT_items[start_item];
	}

#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
	_frame_cnt++;
#endif

	// -- Now, actually send the bits!
	// rmt_write_items(mLED_RMT_CHANNEL, rmt_items, num_rmt_items, true);
	// free(rmt_items);

	// -- Wait for the last chunk of values to be sent
	rmt_wait_tx_done(mLED_RMT_CHANNEL, RMT_MAX_WAIT);
	
	return __clock_cycles() - start;
    }

    // -------------- OLD VERSION -------------------------------

    template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & last_mark, register uint32_t b) {
	b = ~b; b <<= 24;
	for(register uint32_t i = BITS; i > 0; i--) {
	    while((__clock_cycles() - last_mark) < (T1+T2+T3));
	    last_mark = __clock_cycles();
	    FastPin<DATA_PIN>::hi();
	    
	    while((__clock_cycles() - last_mark) < T1);
	    if(b & 0x80000000L) { FastPin<DATA_PIN>::lo(); }
	    b <<= 1;
	    
	    while((__clock_cycles() - last_mark) < (T1+T2));
	    FastPin<DATA_PIN>::lo();
	}
    }

    // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
    // gcc will use register Y for the this pointer.
    static uint32_t showRGBInternalOLD(PixelController<RGB_ORDER> pixels) {
	// Setup the pixel controller and load/scale the first byte
	pixels.preStepFirstByteDithering();
	register uint32_t b = pixels.loadAndScale0();
	pixels.preStepFirstByteDithering();

	ets_intr_lock();

	uint32_t start = __clock_cycles();
	uint32_t last_mark = start;
	while(pixels.has(1)) {

	    // Write first byte, read next byte
	    writeBits<8+XTRA0>(last_mark, b);
	    b = pixels.loadAndScale1();
	    
	    // Write second byte, read 3rd byte
	    writeBits<8+XTRA0>(last_mark, b);
	    b = pixels.loadAndScale2();
	    
	    // Write third byte, read 1st byte of next pixel
	    writeBits<8+XTRA0>(last_mark, b);
	    b = pixels.advanceAndLoadAndScale0();
	    
#if (FASTLED_ALLOW_INTERRUPTS == 1)
	    ets_intr_unlock();	    
#endif

	    pixels.stepDithering();
	    
#if (FASTLED_ALLOW_INTERRUPTS == 1)
	    ets_intr_lock();
	    // if interrupts took longer than 45µs, punt on the current frame
	    if((int32_t)(__clock_cycles()-last_mark) > 0) {
		if((int32_t)(__clock_cycles()-last_mark) > (T1+T2+T3+((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US))) {
		    ets_intr_unlock();
		    return 0; 
		}
	    }
#endif
	};

	ets_intr_unlock();

#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
	_frame_cnt++;
#endif

	return __clock_cycles() - start;
    }
};

FASTLED_NAMESPACE_END

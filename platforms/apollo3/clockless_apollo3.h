#ifndef __INC_CLOCKLESS_APOLLO3_H
#define __INC_CLOCKLESS_APOLLO3_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_APOLLO3)

//#include "ap3_analog.h"
#include "am_hal_systick.h"

#ifndef SYSTICK_MAX_TICKS
#define SYSTICK_MAX_TICKS ((1 << 24)-1)
#endif

#define FASTLED_HAS_CLOCKLESS 1

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

  CMinWait<WAIT_TIME> mWait;

public:
	virtual void init() {
		FastPin<DATA_PIN>::setOutput();
    FastPin<DATA_PIN>::lo();
    am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, 0);
    am_hal_systick_load(0x00FFFFFF);
    am_hal_systick_int_enable();
    am_hal_interrupt_master_enable();
    am_hal_systick_start();
	}

  // extern "C" void am_systick_isr(void)
  // {
  //   am_hal_systick_reset();
  // }

	virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
    //mWait.wait();
		if(!showRGBInternal(pixels)) {
      //sei(); delayMicroseconds(WAIT_TIME); cli();
      //showRGBInternal(pixels);
    }
    //mWait.mark();
  }

	template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register uint8_t & b)  {
		for(register uint32_t i = BITS-1; i > 0; i--) {
      while(am_hal_systick_count() < next_mark);
      next_mark = am_hal_systick_count() + 10;//(T1+T2+T3);
      //if (next_mark > SYSTICK_MAX_TICKS) { next_mark = next_mark - SYSTICK_MAX_TICKS; }
      FastPin<DATA_PIN>::hi();
			if(b&0x80) {
        while((next_mark - am_hal_systick_count()) > 5);//(T3+(200*(F_CPU/24000000))));
        FastPin<DATA_PIN>::lo();
			} else {
        while((next_mark - am_hal_systick_count()) > 7);//(T2+T3+(200*(F_CPU/24000000))));
        FastPin<DATA_PIN>::lo();
			}
			b <<= 1;
		}

    while(am_hal_systick_count() < next_mark);
    next_mark = am_hal_systick_count() + 10;//(T1+T2+T3);
    //if (next_mark > SYSTICK_MAX_TICKS) { next_mark = next_mark - SYSTICK_MAX_TICKS; }
    FastPin<DATA_PIN>::hi();
    if(b&0x80) {
      while((next_mark - am_hal_systick_count()) > 5);//(T3+(200*(F_CPU/24000000))));
      FastPin<DATA_PIN>::lo();
    } else {
      while((next_mark - am_hal_systick_count()) > 7);//(T2+T3+(200*(F_CPU/24000000))));
      FastPin<DATA_PIN>::lo();
    }
	}

	static uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
    FastPin<DATA_PIN>::lo();

		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		register uint8_t b = pixels.loadAndScale0();

		cli();
    register uint32_t next_mark = am_hal_systick_count() + 10;//(T1+T2+T3);
    if (next_mark > SYSTICK_MAX_TICKS) { next_mark = next_mark - SYSTICK_MAX_TICKS; }

		while(pixels.has(1)) {
			pixels.stepDithering();

			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			cli();
      // if interrupts took longer than 45µs, punt on the current frame
			if(am_hal_systick_count() > next_mark) {
				if((am_hal_systick_count() - next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return 0; }
			}
			#endif

			// Write first byte, read next byte
			writeBits<8+XTRA0>(next_mark, b);
			b = pixels.loadAndScale1();

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0>(next_mark, b);
			b = pixels.loadAndScale2();

			// Write third byte, read 1st byte of next pixel
			writeBits<8+XTRA0>(next_mark, b);
			b = pixels.advanceAndLoadAndScale0();

			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			sei();
			#endif
		};

		sei();
		return (am_hal_systick_count());
	}

};


#endif

FASTLED_NAMESPACE_END

#endif

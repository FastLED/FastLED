#pragma once

FASTLED_NAMESPACE_BEGIN

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
public:
	virtual void init() {
		FastPin<DATA_PIN>::setOutput();
		mPinMask = FastPin<DATA_PIN>::mask();
		mPort = FastPin<DATA_PIN>::port();
	}

	virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
    //mWait.wait();
    showRGBInternal(pixels);
    //mWait.mark();
  }

	template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register data_ptr_t port, register data_t hi, register data_t lo, register uint8_t & b)  {
		for(register uint32_t i = BITS; i > 0; i--) {
			while(__clock_cycles() < next_mark);
			next_mark = __clock_cycles() + (T1+T2+T3);
      FastPin<DATA_PIN>::hi();
			if(b&0x80) {
				while((next_mark - __clock_cycles()) > (T3 - 3));
        FastPin<DATA_PIN>::lo();
			} else {
				while((next_mark - __clock_cycles()) > (T2+T3 - 3));

        FastPin<DATA_PIN>::lo();
			}
			b <<= 1;
		}

		// while(__clock_cycles() < next_mark);
		// next_mark = __clock_cycles() + (T1+T2+T3);
    // FastPin<DATA_PIN>::hi();
    //
		// if(b&0x80) {
		// 	while((next_mark - __clock_cycles()) > (T3+(2*(F_CPU/24000000))));
    //   FastPin<DATA_PIN>::lo();
		// } else {
		// 	while((next_mark - __clock_cycles()) > (T2+T3+(2*(F_CPU/24000000))));
    //   FastPin<DATA_PIN>::lo();
		// }
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static uint32_t ICACHE_RAM_ATTR showRGBInternal(PixelController<RGB_ORDER> & pixels) {
		register data_ptr_t port = FastPin<DATA_PIN>::port();
		register data_t hi = *port | FastPin<DATA_PIN>::mask();;
		register data_t lo = *port & ~FastPin<DATA_PIN>::mask();;
		*port = lo;

		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		register uint8_t b = pixels.loadAndScale0();

		os_intr_lock();
    uint32_t start = __clock_cycles();
		uint32_t next_mark = start + (T1+T2+T3);
		while(pixels.has(1)) {
			pixels.stepDithering();
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			os_intr_lock();
			// if interrupts took longer than 45µs, punt on the current frame
			if(__clock_cycles() > next_mark) {
				if((__clock_cycles()-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return __clock_cycles(); }
			}

			hi = *port | FastPin<DATA_PIN>::mask();
			lo = *port & ~FastPin<DATA_PIN>::mask();
			#endif
			// Write first byte, read next byte
			writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
			b = pixels.loadAndScale1();

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
			b = pixels.loadAndScale2();

			// Write third byte, read 1st byte of next pixel
			writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
      b = pixels.advanceAndLoadAndScale0();
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			os_intr_unlock();
			#endif
		};

		os_intr_unlock();
		return __clock_cycles();
	}
};

FASTLED_NAMESPACE_END

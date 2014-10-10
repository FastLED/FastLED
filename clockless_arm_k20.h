#ifndef __INC_CLOCKLESS_ARM_K20_H
#define __INC_CLOCKLESS_ARM_K20_H

// Definition for a single channel clockless controller for the k20 family of chips, like that used in the teensy 3.0/3.1
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSY3)
template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 500>
class ClocklessController : public CLEDController {
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

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

protected:

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());

		mWait.wait();
		cli();

		uint32_t clocks = showRGBInternal(pixels);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		MS_COUNTER += (1 + (microsTaken / 1000));
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());

		mWait.wait();
		cli();

		uint32_t clocks = showRGBInternal(pixels);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		MS_COUNTER += (1 + (microsTaken / 1000));
		sei();
		mWait.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
		mWait.wait();
		cli();

		uint32_t clocks = showRGBInternal(pixels);


		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		MS_COUNTER += (1 + (microsTaken / 1000));
		sei();
		mWait.mark();
	}
#endif

	template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register data_ptr_t port, register data_t hi, register data_t lo, register uint8_t & b)  {
		for(register uint32_t i = BITS-1; i > 0; i--) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
			FastPin<DATA_PIN>::fastset(port, hi);
			if(b&0x80) {
				while((next_mark - ARM_DWT_CYCCNT) > (T3+(2*(F_CPU/24000000))));
				FastPin<DATA_PIN>::fastset(port, lo);
			} else {
				while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+(2*(F_CPU/24000000))));
				FastPin<DATA_PIN>::fastset(port, lo);
			}
			b <<= 1;
		}

		while(ARM_DWT_CYCCNT < next_mark);
		next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
		FastPin<DATA_PIN>::fastset(port, hi);

		if(b&0x80) {
			while((next_mark - ARM_DWT_CYCCNT) > (T3+(2*(F_CPU/24000000))));
			FastPin<DATA_PIN>::fastset(port, lo);
		} else {
			while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+(2*(F_CPU/24000000))));
			FastPin<DATA_PIN>::fastset(port, lo);
		}
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static uint32_t showRGBInternal(PixelController<RGB_ORDER> & pixels) {
	    // Get access to the clock
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;

		register data_ptr_t port = FastPin<DATA_PIN>::port();
		register data_t hi = *port | FastPin<DATA_PIN>::mask();;
		register data_t lo = *port & ~FastPin<DATA_PIN>::mask();;
		*port = lo;

		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		register uint8_t b = pixels.loadAndScale0();

		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		while(pixels.has(1)) {
			pixels.stepDithering();

			// Write first byte, read next byte
			writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
			b = pixels.loadAndScale1();

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
			b = pixels.loadAndScale2();

			// Write third byte, read 1st byte of next pixel
			writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
			b = pixels.advanceAndLoadAndScale0();
		};

		return ARM_DWT_CYCCNT;
	}
};
#endif

#endif

#ifndef __INC_CLOCKLESS_ARM_K20_H
#define __INC_CLOCKLESS_ARM_K20_H

// Definition for a single channel clockless controller for the k20 family of chips, like that used in the teensy 3.0/3.1
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSY3)
template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, bool FLIP = false, int WAIT_TIME = 50>
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

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		mWait.wait();
		cli();

		showRGBInternal<0, false>(nLeds, scale, (const byte*)&data);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 8 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, uint8_t scale = 255) { 
		mWait.wait();
		cli();

		showRGBInternal<0, true>(nLeds, scale, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 8 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, uint8_t scale = 255) { 
		mWait.wait();
		cli();

		showRGBInternal<1, true>(nLeds, scale, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 8 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}
#endif

	inline static void write8Bits(register data_ptr_t port, register data_t hi, register data_t lo, register uint32_t & b)  __attribute__ ((always_inline)) {
		// TODO: hand rig asm version of this method.  The timings are based on adjusting/studying GCC compiler ouptut.  This
		// will bite me in the ass at some point, I know it.
		for(register uint32_t i = 7; i > 0; i--) { 
			FastPin<DATA_PIN>::fastset(port, hi);
			delaycycles<T1 - 5>(); // 5 cycles - 2 store, 1 and, 1 test, 1 if
			if(b & 0x80) { FastPin<DATA_PIN>::fastset(port, hi); } else { FastPin<DATA_PIN>::fastset(port, lo); }
			b <<= 1;
			delaycycles<T2 - 2>(); // 2 cycles,  1 store/skip,  1 shift 
			FastPin<DATA_PIN>::fastset(port, lo);
			delaycycles<T3 - 5>(); // 3 cycles, 2 store, 1 sub, 1 branch backwards
		}
		// delay an extra cycle because falling out of the loop takes on less cycle than looping around
		delaycycles<1>();

		FastPin<DATA_PIN>::fastset(port, hi);
		delaycycles<T1 - 6>();
		if(b & 0x80) { FastPin<DATA_PIN>::fastset(port, hi); } else { FastPin<DATA_PIN>::fastset(port, lo); }
		delaycycles<T2 - 2>(); // 4 cycles, 2 store, store/skip
		FastPin<DATA_PIN>::fastset(port, lo);
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	template<int SKIP, bool ADVANCE> static void showRGBInternal(register int nLeds, register uint8_t scale, register const byte *rgbdata) {
		register byte *data = (byte*)rgbdata;
		register data_t mask = FastPin<DATA_PIN>::mask();
		register data_ptr_t port = FastPin<DATA_PIN>::port();
		nLeds *= (3 + SKIP);
		register uint8_t *end = data + nLeds; 
		register data_t hi = *port | mask;
		register data_t lo = *port & ~mask;
		*port = lo;

		register uint32_t b;
		b = ((ADVANCE)?data:rgbdata)[SKIP + RGB_BYTE0(RGB_ORDER)];
		b = scale8(b, scale);
		while(data < end) { 
			// Write first byte, read next byte
			write8Bits(port, hi, lo, b);

			b = ((ADVANCE)?data:rgbdata)[SKIP + RGB_BYTE1(RGB_ORDER)];
			INLINE_SCALE(b, scale);
			delaycycles<T3 - 5>(); // 1 store, 2 load, 1 mul, 1 shift, 

			// Write second byte
			write8Bits(port, hi, lo, b);

			b = ((ADVANCE)?data:rgbdata)[SKIP + RGB_BYTE2(RGB_ORDER)];
			INLINE_SCALE(b, scale);

			data += 3 + SKIP;
			if((RGB_ORDER & 0070) == 0) {
				delaycycles<T3 - 6>(); // 1 store, 2 load, 1 mul, 1 shift,  1 adds if BRG or GRB
			} else {
				delaycycles<T3 - 5>(); // 1 store, 2 load, 1 mul, 1 shift, 
			}

			// Write third byte
			write8Bits(port, hi, lo, b);

			b = ((ADVANCE)?data:rgbdata)[SKIP + RGB_BYTE0(RGB_ORDER)];
			INLINE_SCALE(b, scale);

			delaycycles<T3 - 11>(); // 1 store, 2 load (with increment), 1 mul, 1 shift, 1 cmp, 1 branch backwards, 1 movim
		};
	}
};
#endif

#endif


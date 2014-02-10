#ifndef __CLOCKLESS_AVR_H
#define __CLOCKLESS_AVR_H

#ifdef FASTLED_AVR

// Definition for a single channel clockless controller for the avr family of chips, like those used in
// the arduino and teensy 2.x.  Note that there is a special case for hardware-mul-less versions of the avr, 
// which are tracked in clockless_trinket.h
template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
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

	template <int N, int ADJ>inline static void bitSetLast(register data_ptr_t port, register data_t hi, register data_t lo, register uint8_t b) { 
		// First cycle
		SET_HI; 							// 1 clock cycle if using out, 2 otherwise
		delaycycles<T1 - (AVR_PIN_CYCLES(DATA_PIN) + 1)>();					// 1st cycle length minus 1 clock for out, 1 clock for sbrs
		__asm__ __volatile__ ("sbrs %0, %1" :: "r" (b), "M" (N) :); // 1 clock for check (+1 if skipping, next op is also 1 clock)

		// Second cycle
		SET_LO;							// 1/2 clock cycle if using out
		delaycycles<T2 - AVR_PIN_CYCLES(DATA_PIN)>(); 						// 2nd cycle length minus 1/2 clock for out

		// Third cycle
		SET_LO;							// 1/2 clock cycle if using out
		delaycycles<T3 - (AVR_PIN_CYCLES(DATA_PIN) + ADJ)>();				// 3rd cycle length minus 7 clocks for out, loop compare, jump, next uint8_t load
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

		showRGBInternal<1, true>((long)nLeds, scale, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 8 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}
#endif

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	template<int SKIP, bool ADVANCE> static void showRGBInternal(register int nLeds, register uint8_t scale, register const byte *rgbdata) {
		register byte *data = (byte*)rgbdata;
		register data_ptr_t port = FastPin<DATA_PIN>::port();
		nLeds *= (3 + SKIP);
		register uint8_t *end = data + nLeds; 
		register data_t hi = FastPin<DATA_PIN>::hival();
		register data_t lo = FastPin<DATA_PIN>::loval();;
		*port = lo;

		register uint8_t b;

		if(ADVANCE) { 
			b = data[SKIP + RGB_BYTE0(RGB_ORDER)];
		} else { 
			b = rgbdata[SKIP + RGB_BYTE0(RGB_ORDER)];
		}
		b = scale8_LEAVING_R1_DIRTY(b, scale);

		register uint8_t c;
		register uint8_t d;
		while(data < end) { 
			for(register byte x=5; x; x--) {
				bitSetLast<7, 4>(port, hi, lo, b);
				b <<= 1;
			}
			delaycycles<1>();
			// Leave an extra 2 clocks for the next byte load
			bitSetLast<7, 1>(port, hi, lo, b);
			delaycycles<1>();

			// Leave an extra 4 clocks for the scale
			bitSetLast<6, 6>(port, hi, lo, b);
			if(ADVANCE) { 
				c = data[SKIP + RGB_BYTE1(RGB_ORDER)];
			} else { 
				c = rgbdata[SKIP + RGB_BYTE1(RGB_ORDER)];
				delaycycles<1>();
			}
			INLINE_SCALE(c, scale);
			bitSetLast<5, 1>(port, hi, lo, b);
			
			if(XTRA0 > 0) { 
				for(register byte x=XTRA0; x; x--) { 
					bitSetLast<4,4>(port, hi, lo, b);
					b <<=1;
				}
				delaycycles<1>();
			}

			for(register byte x=5; x; x--) {
				bitSetLast<7, 4>(port, hi, lo, c);
				c <<= 1;
			}
			delaycycles<1>();
			// Leave an extra 2 clocks for the next byte load
			bitSetLast<7, 1>(port, hi, lo, c);
			delaycycles<1>();
			
			// Leave an extra 4 clocks for the scale
			bitSetLast<6, 6>(port, hi, lo, c);
			if(ADVANCE) { 
				d = data[SKIP + RGB_BYTE2(RGB_ORDER)];
			} else { 
				d = rgbdata[SKIP + RGB_BYTE2(RGB_ORDER)];
				delaycycles<1>();
			}
			INLINE_SCALE(d, scale);
			bitSetLast<5, 1>(port, hi, lo, c);
			
			if(XTRA0 > 0) { 
				for(register byte x=XTRA0; x; x--) { 
					bitSetLast<4,4>(port, hi, lo, c);
					c <<=1;
				}
				delaycycles<1>();
			}

			for(register byte x=5; x; x--) {
				bitSetLast<7, 4>(port, hi, lo, c);
				c <<= 1;
			}
			delaycycles<1>();
			// Leave an extra 2 clocks for the next byte load
			bitSetLast<7, 2>(port, hi, lo, d);
			data += (SKIP + 3);
			// Leave an extra 4 clocks for the scale
			bitSetLast<6, 6>(port, hi, lo, d);
			if(ADVANCE) { 
				b = data[SKIP + RGB_BYTE0(RGB_ORDER)];
			} else { 
				b = rgbdata[SKIP + RGB_BYTE0(RGB_ORDER)];
				delaycycles<1>();
			}
			INLINE_SCALE(b, scale);
			
			if(XTRA0 > 0) { 
				bitSetLast<5, 1>(port, hi, lo, d);
				for(register byte x=XTRA0-1; x; x--) { 
					bitSetLast<4,4>(port, hi, lo, d);
					d <<=1;
				}
				delaycycles<1>();
				bitSetLast<4,6>(port,hi,lo,d);
			} else {
				bitSetLast<5,6>(port, hi, lo, d);
			}

		}
		cleanup_R1();
	}
};
#endif

#endif


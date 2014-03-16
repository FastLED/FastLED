#ifndef __INC_CLOCKLESS2_H
#define __INC_CLOCKLESS2_H

#include "controller.h"
#include "lib8tion.h"
#include "led_sysdefs.h"
#include "delay.h"

#ifdef FASTLED_TEENSY3
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base template for clockless controllers.  These controllers have 3 control points in their cycle for each bit.  The first point
// is where the line is raised hi.  The second pointsnt is where the line is dropped low for a zero.  The third point is where the 
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Convinience macros to wrap around the toggling of hi vs. lo
#define SET2_LO FLIP ? FastPin<DATA_PIN>::hi() : FastPin<DATA_PIN>::lo()
#define SET2_HI FLIP ? FastPin<DATA_PIN>::lo() : FastPin<DATA_PIN>::hi()

#define SET2_LO2 FLIP ? FastPin<DATA_PIN2>::hi() : FastPin<DATA_PIN2>::lo()
#define SET2_HI2 FLIP ? FastPin<DATA_PIN2>::lo() : FastPin<DATA_PIN2>::hi()


template <uint8_t DATA_PIN, uint8_t DATA_PIN2, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController2 : public CLEDController {
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() { 
		FastPin<DATA_PIN>::setOutput();
		FastPin<DATA_PIN2>::setOutput();
	}


	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		mWait.wait();
		cli();

		showRGBInternal<0, false>(nLeds, scale, (const byte*)&data, (const byte*)&data);

		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, uint8_t scale = 255) { 
		mWait.wait();
		cli();

		showRGBInternal<0, true>(nLeds, scale, (const byte*)rgbdata, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, const struct CRGB *rgbdata2, int nLeds, uint8_t scale = 255) { 
		mWait.wait();
		cli();

		showRGBInternal<0, true>(nLeds, scale, (const byte*)rgbdata, (const byte*)rgbdata2 );

		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
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
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}
#endif

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	template<int SKIP, bool ADVANCE> static void showRGBInternal(register int nLeds, register uint8_t scale, register const byte *rgbdata, register const byte *rgbdata2) {
		register byte *data = (byte*)rgbdata;
		register byte *data2 = (byte*)rgbdata2;

		nLeds *= (3 + SKIP);
		register uint8_t *end = data + nLeds; 

		register uint32_t b;
		register uint32_t c;
		if(ADVANCE) { 
			b = data[SKIP + RGB_BYTE0(RGB_ORDER)];
			c = data2[SKIP + RGB_BYTE0(RGB_ORDER)];
		} else { 
			b = rgbdata[SKIP + RGB_BYTE0(RGB_ORDER)];
			c = rgbdata2[SKIP + RGB_BYTE0(RGB_ORDER)];
		}
		b = scale8(b, scale);
		c = scale8(c, scale);

		while(data < end) { 
			// TODO: hand rig asm version of this method.  The timings are based on adjusting/studying GCC compiler ouptut.  This
			// will bite me in the ass at some point, I know it.
			for(register uint32_t i = 7; i > 0; i--) { 
				SET2_HI; delaycycles<3>(); SET2_HI2;
				delaycycles<T1 - 6>(); // 6 cycles - 1/1 store, 1/1 test, 1/1 if, 1/1 port lookup
				if(b & 0x80) { SET2_HI; } else { SET2_LO; }
				if(c & 0x80) { SET2_HI2; } else { SET2_LO2; }
				b <<= 1; c <<=1;
				delaycycles<T2 - 8>(); // 3 cycles, 1/1 store, 1/1 store/skip,  1/1 shift , 1/1 port lookup
				SET2_LO; SET2_LO2;
				delaycycles<T3 - 6>(); // 4 cycles, 1/1 store, 1 sub, 1 branch backwards, 1/1 port lookup
			}
			// extra delay because branch is faster falling through
			delaycycles<1>();

			// 8th bit, interleave loading rest of data
			SET2_HI; delaycycles<3>(); SET2_HI2;
			delaycycles<T1 - 6>();
			if(b & 0x80) { SET2_HI; } else { SET2_LO; }
			if(c & 0x80) { SET2_HI2; } else { SET2_LO2; }
			delaycycles<T2 - 6>(); // 4 cycles, 2/2 store, 1/1 store/skip, 1/1 port lookup
			SET2_LO; SET2_LO2;

			if(ADVANCE) { 
				b = data[SKIP + RGB_BYTE1(RGB_ORDER)];
				c = data2[SKIP + RGB_BYTE1(RGB_ORDER)];
			} else { 
				b = rgbdata[SKIP + RGB_BYTE1(RGB_ORDER)];
				c = rgbdata2[SKIP + RGB_BYTE1(RGB_ORDER)];
			}
			b = scale8(b, scale);
			c = scale8(c, scale);

			delaycycles<T3 - 12>(); // 1/1 store, 2/2 load, 1/1 mul, 1/1 shift, , 1/1 port lookup

			for(register uint32_t i = 7; i > 0; i--) { 
				SET2_HI; delaycycles<3>(); SET2_HI2;
				delaycycles<T1 - 6>(); // 3 cycles - 1 store, 1 test, 1 if
				if(b & 0x80) { SET2_HI; } else { SET2_LO; }
				if(c & 0x80) { SET2_HI2; } else { SET2_LO2; }
				b <<= 1; c <<= 1;
				delaycycles<T2 - 8>(); // 6 cycles, 1/1 store, 1/1 store/skip,  1/1 shift , 1/1 port lookup
				SET2_LO; SET2_LO2;
				delaycycles<T3 - 6>(); // 4 cycles, 1/1 store, 1 sub, 1 branch backwards, 1/1 port lookup
			}
			// extra delay because branch is faster falling through
			delaycycles<1>();

			// 8th bit, interleave loading rest of data
			SET2_HI; delaycycles<3>(); SET2_HI2;
			delaycycles<T1 - 6>();
			if(b & 0x80) { SET2_HI; } else { SET2_LO; }
			if(b & 0x80) { SET2_HI2; } else { SET2_LO2; }
			delaycycles<T2 - 6>(); // 4 cycles, 2 store, store/skip, 1/1 port lookup
			SET2_LO; SET2_LO2;

			if(ADVANCE) { 
				b = data[SKIP + RGB_BYTE2(RGB_ORDER)];
				c = data2[SKIP + RGB_BYTE2(RGB_ORDER)];
			} else { 
				b = rgbdata[SKIP + RGB_BYTE2(RGB_ORDER)];
				c = rgbdata2[SKIP + RGB_BYTE2(RGB_ORDER)];
			}
			b = scale8(b, scale);
			c = scale8(c, scale);

			data += 3 + SKIP; data2 += 3 + SKIP;
			if((RGB_ORDER & 0070) == 0) {
				delaycycles<T3 - 14>(); // 1/1 store, 2/2 load, 1/1 mul, 1/1 shift,  1/1 adds if BRG or GRB, 1/1 port lookup
			} else {
				delaycycles<T3 - 12>(); // 1/1 store, 2/2 load, 1/1 mul, 1/1 shift, , 1/1 port lookup
			}

			for(register uint32_t i = 7; i > 0; i--) { 
				SET2_HI; delaycycles<3>(); SET2_HI2;
				delaycycles<T1 - 6>(); // 3 cycles - 1 store, 1 test, 1 if
				if(b & 0x80) { SET2_HI; } else { SET2_LO; }
				if(c & 0x80) { SET2_HI2; } else { SET2_LO2; }
				b <<= 1; c <<= 1;
				delaycycles<T2 - 8>(); // 3 cycles, 1 store, 1 store/skip,  1 shift , 1/1 port lookup
				SET2_LO; SET2_LO2;
				delaycycles<T3 - 6>(); // 3 cycles, 1/1 store, 1 sub, 1 branch backwards, 1/1 port lookup
			}
			// extra delay because branch is faster falling through
			delaycycles<1>();

			// 8th bit, interleave loading rest of data
			SET2_HI; delaycycles<3>(); SET2_HI2;
			delaycycles<T1 - 6>();
			if(b & 0x80) { SET2_HI; } else { SET2_LO; }
			if(c & 0x80) { SET2_HI2; } else { SET2_LO2; }
			delaycycles<T2 - 6>(); // 4 cycles, 2/2 store, store/skip, 1/1 port lookup
			SET2_LO; SET2_LO2;

			if(ADVANCE) { 
				b = data[SKIP + RGB_BYTE0(RGB_ORDER)];
				c = data2[SKIP + RGB_BYTE0(RGB_ORDER)];
			} else { 
				b = rgbdata[SKIP + RGB_BYTE0(RGB_ORDER)];
				c = rgbdata2[SKIP + RGB_BYTE0(RGB_ORDER)];
			}
			b = scale8(b, scale);
			c = scale8(c, scale);
			delaycycles<T3 - 15>(); // 1/1 store, 2/2 load (with increment), 1/1 mul, 1/1 shift, 1 cmp, 1 branch backwards, 1 movim, 1/1 port lookup
		};
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(struct CARGB *data, int nLeds) { 
		// TODO: IMPLEMENTME
	}
#endif
};

#undef SET2_HI
#undef SET2_HI2
#undef SET2_LO
#undef SET2_LO2

#endif

#endif
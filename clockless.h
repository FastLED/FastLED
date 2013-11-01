#ifndef __INC_CLOCKLESS_H
#define __INC_CLOCKLESS_H

#include "controller.h"
#include "lib8tion.h"
#include <avr/interrupt.h> // for cli/se definitions

// Macro to convert from nano-seconds to clocks and clocks to nano-seconds
// #define NS(_NS) (_NS / (1000 / (F_CPU / 1000000L)))
#if F_CPU < 96000000
#define NS(_NS) ( (_NS * (F_CPU / 1000000L))) / 1000
#define CLKS_TO_MICROS(_CLKS) ((long)(_CLKS)) / (F_CPU / 1000000L)
#else
#define NS(_NS) ( (_NS * (F_CPU / 2000000L))) / 1000
#define CLKS_TO_MICROS(_CLKS) ((long)(_CLKS)) / (F_CPU / 2000000L)
#endif

//  Macro for making sure there's enough time available
#define NO_TIME(A, B, C) (NS(A) < 3 || NS(B) < 3 || NS(C) < 6)

#if defined(__MK20DX128__)
   extern volatile uint32_t systick_millis_count;
#  define MS_COUNTER systick_millis_count
#else
#  if defined(CORE_TEENSY)
     extern volatile unsigned long timer0_millis_count;
#    define MS_COUNTER timer0_millis_count
#  else
     extern volatile unsigned long timer0_millis;
#    define MS_COUNTER timer0_millis
#  endif
#endif

// Scaling macro choice
#if defined(LIB8_ATTINY)
#  define INLINE_SCALE(B, SCALE) delaycycles<3>()
#  warning "No hardware multiply, inline brightness scaling disabled"
#else
#   define INLINE_SCALE(B, SCALE) B = scale8_LEAVING_R1_DIRTY(B, SCALE)
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base template for clockless controllers.  These controllers have 3 control points in their cycle for each bit.  The first point
// is where the line is raised hi.  The second pointsnt is where the line is dropped low for a zero.  The third point is where the 
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int WAIT_TIME = 50>
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

#if defined(__MK20DX128__)
	// We don't use the bitSetFast methods for ARM.
#else
	template <int N>inline static void bitSetFast(register data_ptr_t port, register data_t hi, register data_t lo, register uint8_t b) { 
		// First cycle
		FastPin<DATA_PIN>::fastset(port, hi); 								// 1/2 clock cycle if using out
		delaycycles<T1 - (_CYCLES(DATA_PIN) + 1)>();					// 1st cycle length minus 1/2 clock for out, 1 clock for sbrs
		__asm__ __volatile__ ("sbrs %0, %1" :: "r" (b), "M" (N) :); 	// 1 clock for check (+1 if skipping, next op is also 1 clock)

		// Second cycle
		FastPin<DATA_PIN>::fastset(port, lo);								// 1/2 clock cycle if using out
		delaycycles<T2 - _CYCLES(DATA_PIN)>(); 							// 2nd cycle length minus 1/2 clock for out

		// Third cycle
		FastPin<DATA_PIN>::fastset(port, lo);								// 1 clock cycle if using out
		delaycycles<T3 - _CYCLES(DATA_PIN)>();							// 3rd cycle length minus 1 clock for out
	}
	
	#define END_OF_BYTE
	#define END_OF_LOOP 6 		// loop compare, jump, next uint8_t load
	template <int N, int ADJ>inline static void bitSetLast(register data_ptr_t port, register data_t hi, register data_t lo, register uint8_t b) { 
		// First cycle
		FastPin<DATA_PIN>::fastset(port, hi); 							// 1 clock cycle if using out, 2 otherwise
		delaycycles<T1 - (_CYCLES(DATA_PIN))>();					// 1st cycle length minus 1 clock for out, 1 clock for sbrs
		__asm__ __volatile__ ("sbrs %0, %1" :: "r" (b), "M" (N) :); // 1 clock for check (+1 if skipping, next op is also 1 clock)

		// Second cycle
		FastPin<DATA_PIN>::fastset(port, lo);							// 1/2 clock cycle if using out
		delaycycles<T2 - (_CYCLES(DATA_PIN))>(); 						// 2nd cycle length minus 1/2 clock for out

		// Third cycle
		FastPin<DATA_PIN>::fastset(port, lo);							// 1/2 clock cycle if using out
		delaycycles<T3 - (_CYCLES(DATA_PIN) + ADJ)>();				// 3rd cycle length minus 7 clocks for out, loop compare, jump, next uint8_t load
	}
#endif

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		mWait.wait();
		cli();

		showRGBInternal<0, false>(nLeds, scale, (const byte*)&data);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(nLeds * 24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, uint8_t scale = 255) { 
		mWait.wait();
		cli();

		showRGBInternal<0, true>(nLeds, scale, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(nLeds * 24 * (T1 + T2 + T3));
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
		long microsTaken = CLKS_TO_MICROS(nLeds * 24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}
#endif

#if defined(__MK20DX128__)
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
#endif

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

#if defined(__MK20DX128__)
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
#else
#if 0
		register uint8_t b = *data++;
		while(data <= end) { 
			bitSetFast<7>(port, hi, lo, b);
			bitSetFast<6>(port, hi, lo, b);
			bitSetFast<5>(port, hi, lo, b);
			bitSetFast<4>(port, hi, lo, b);
			bitSetFast<3>(port, hi, lo, b);
			// Leave an extra 2 clocks for the next byte load
			bitSetLast<2, 2>(port, hi, lo, b);
			register uint8_t next = *data++;
			// Leave an extra 4 clocks for the scale
			bitSetLast<1, 4>(port, hi, lo, b);
			next = scale8(next, scale);
			bitSetLast<0, END_OF_LOOP>(port, hi, lo, b);
			b = next;
		}
#else
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
			
			for(register byte x=5; x; x--) {
				bitSetLast<7, 4>(port, hi, lo, d);
				d <<= 1;
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
			bitSetLast<5, 6>(port, hi, lo, d);
		}
		cleanup_R1();
#endif
#endif
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(struct CARGB *data, int nLeds) { 
		// TODO: IMPLEMENTME
	}
#endif
};

#endif
#ifndef __INC_CLOCKLESS_TRINKET_H
#define __INC_CLOCKLESS_TRINKET_H

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
// is where the line is raised hi.  The second point is where the line is dropped low for a zero.  The third point is where the 
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int WAIT_TIME = 50>
class ClocklessController_Trinket : public CLEDController {
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
		delaycycles<T3 - (_CYCLES(DATA_PIN) + ADJ)>();				// 3rd cycle length minus the passed in adjustment 
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
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, uint8_t scale = 255) { 
		mWait.wait();
		cli();

		showRGBInternal<0, true>(nLeds, scale, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 24 * (T1 + T2 + T3));
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
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}
#endif

/// Macro defs for the asm block, flagged with cycle timings
// 1 cycle, write hi to the port
#define HI1 "out %[PORT], %[hi]\n\t"
// 1 cycle, write lo to the port
#define LO1 "out %[PORT], %[lo]\n\t"
// 2 cycles, sbrs on flipping the line to lo if we're pushing out a 0
#define QLO2(B, N) "sbrs %[" #B "], " #N "\n\t" \
			"out %[PORT], %[lo]\n\t"
// 0 cycles - nop0 - used to placeholder where wait points would be for other timings/chipsets
#define NOP0 ""
// 1 cycle nop
#define NOP1 "nop\n\t"
// 2 cycle nop
#define NOP2 "nop\n\tnop\n\t"
// 3 cycle nop
#define NOP3 NOP2 NOP1
// 4 cycle nop
#define NOP4 NOP2 NOP2
// 2 cycle byte load 
#define LD2(B,O) "ldd %[" #B "], Z + %[" #O "]\n\t"
// 2 cycle data pointer increment
#define IDATA2 "adiw %[data], %[ADV]\n\t"
// 1 cycle decrement counter
#define DCOUNT1 "subi %[count], 1\n\t"
// 2 cycle loop jump
#define JMPLOOP2 "rjmp loop_%=\n\t"
// 1 cycle (if not branched) end of loop check
#define BRLOOP1 "breq done_%=\n\t"

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	template<int SKIP, bool ADVANCE> static void showRGBInternal(register int nLeds, register uint8_t scale, register const byte *rgbdata) {
		register byte *data = (byte*)rgbdata;
		data_t mask = FastPin<DATA_PIN>::mask();
		data_ptr_t port = FastPin<DATA_PIN>::port();
		// register uint8_t *end = data + nLeds; 
		register data_t hi = *port | mask;
		register data_t lo = *port & ~mask;
		*port = lo;

		register uint8_t b0, b1, b2;
		register uint8_t count = nLeds & 0xFF;
		register uint8_t scale_sum = 0;

		b0 = data[RGB_BYTE0(RGB_ORDER)];
		// b0 = scale8(b0, scale);
		b1 = data[RGB_BYTE1(RGB_ORDER)];
		b2 = 0;

		asm __volatile__(
			/* asm */
			"loop_%=:		\n\r"	
			// Sum of the clock counts across each row should be 10 for 8Mhz, WS2811
			HI1	NOP0 QLO2(b0, 7) LD2(b2,O2) NOP2 	LO1 NOP2			
			HI1 NOP0 QLO2(b0, 6) NOP4 				LO1 NOP2		
			HI1 NOP0 QLO2(b0, 5) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b0, 4) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b0, 3) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b0, 2) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b0, 1) NOP4			 	LO1 NOP2			
			HI1 NOP0 QLO2(b0, 0) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b1, 7) LD2(b0,O0) NOP2 	LO1 NOP2			
			HI1 NOP0 QLO2(b1, 6) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b1, 5) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b1, 4) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b1, 3) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b1, 2) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b1, 1) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b1, 0) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b2, 7) LD2(b1, O1) NOP2 	LO1 NOP2			
			HI1 NOP0 QLO2(b2, 6) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b2, 5) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b2, 4) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b2, 3) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b2, 2) NOP4 				LO1 NOP2			
			HI1 NOP0 QLO2(b2, 1) IDATA2 NOP2		LO1 DCOUNT1 NOP1	
			// The last bit is tricky.  We do the 3 cycle hi, bit check, lo.  Then we do a breq
			// that if we don't branch, will be 1 cycle, then 3 cycles of nop, then 1 cycle out, then
			// 2 cycles of jumping around the loop.  If we do branch, then that's 2 cycles, we need to 
			// wait 2 more cycles, then do the final low and waiting
			HI1 NOP0 QLO2(b2, 0) BRLOOP1 NOP3 		LO1 JMPLOOP2	
				
			"done_%=:\n\t"
			NOP2 LO1 NOP2

			: /* write variables */
			[b0] "+r" (b0),
			[b1] "+r" (b1),
			[b2] "+r" (b2),
			[count] "+r" (count),
			[scale_sum] "+r" (scale_sum),
			[data] "+z" (data)
			: /* use variables */
			[hi] "r" (hi),
			[lo] "r" (lo),
			[scale] "r" (scale),
			[O0] "M" (RGB_BYTE0(RGB_ORDER)),
			[O1] "M" (RGB_BYTE1(RGB_ORDER)),
			[O2] "M" (RGB_BYTE2(RGB_ORDER)),
			[PORT] "M" (FastPin<DATA_PIN>::port()),
			[ADV] "M" (ADVANCE?(SKIP+3):0)
			: /* clobber registers */
		);
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(struct CARGB *data, int nLeds) { 
		// TODO: IMPLEMENTME
	}
#endif
};

#endif

#ifndef __INC_CLOCKLESS_H
#define __INC_CLOCKLESS_H

#include "controller.h"
#include <avr/interrupt.h> // for cli/se definitions

// Macro to convert from nano-seconds to clocks
#define NS(_NS) (_NS / (1000 / (F_CPU / 1000000L)))

//  Macro for making sure there's enough time available
#define NO_TIME(A, B, C) (NS(A) < 3 || NS(B) < 2 || NS(C) < 6)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base template for clockless controllers.  These controllers have 3 control points in their cycle for each bit.  The first point
// is where the line is raised hi.  The second pointsnt is where the line is dropped low for a zero.  The third point is where the 
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, int T1, int T2, int T3>
class ClocklessController : public CLEDController {
	uint8_t mPinMask;
	volatile uint8_t *mPort;
public:
	virtual void init() { 
		Pin<DATA_PIN>::setOutput();
		mPinMask = Pin<DATA_PIN>::mask();
		mPort = Pin<DATA_PIN>::port();
	}

	template <int N>inline static void bitSetFast(register volatile uint8_t *port, register uint8_t hi, register uint8_t lo, register uint8_t b) { 
		// First cycle
		Pin<DATA_PIN>::fastset(port, hi); 								// 1/2 clock cycle if using out
		delaycycles<T1 - (_CYCLES(DATA_PIN) + 1)>();					// 1st cycle length minus 1/2 clock for out, 1 clock for sbrs
		__asm__ __volatile__ ("sbrs %0, %1" :: "r" (b), "M" (N) :); 	// 1 clock for check (+1 if skipping, next op is also 1 clock)

		// Second cycle
		Pin<DATA_PIN>::fastset(port, lo);								// 1/2 clock cycle if using out
		delaycycles<T2 - _CYCLES(DATA_PIN)>(); 							// 2nd cycle length minus 1/2 clock for out

		// Third cycle
		Pin<DATA_PIN>::fastset(port, lo);								// 1 clock cycle if using out
		delaycycles<T3 - _CYCLES(DATA_PIN)>();							// 3rd cycle length minus 1 clock for out
	}
	
	#define END_OF_LOOP 6 		// loop compare, jump, next uint8_t load
	template <int N, int ADJ>inline static void bitSetLast(register volatile uint8_t *port, register uint8_t hi, register uint8_t lo, register uint8_t b) { 
		// First cycle
		Pin<DATA_PIN>::fastset(port, hi); 							// 1 clock cycle if using out, 2 otherwise
		delaycycles<T1 - (_CYCLES(DATA_PIN) + 1)>();					// 1st cycle length minus 1 clock for out, 1 clock for sbrs
		__asm__ __volatile__ ("sbrs %0, %1" :: "r" (b), "M" (N) :); // 1 clock for check (+1 if skipping, next op is also 1 clock)

		// Second cycle
		Pin<DATA_PIN>::fastset(port, lo);							// 1/2 clock cycle if using out
		delaycycles<T2 - _CYCLES(DATA_PIN)>(); 						// 2nd cycle length minus 1/2 clock for out

		// Third cycle
		Pin<DATA_PIN>::fastset(port, lo);							// 1/2 clock cycle if using out
		delaycycles<T3 - (_CYCLES(DATA_PIN) + ADJ)>();				// 3rd cycle length minus 7 clocks for out, loop compare, jump, next uint8_t load
	}

	virtual void showRGB(register uint8_t *data, register int nLeds) {
		cli();
		
		register uint8_t mask = mPinMask;
		register volatile uint8_t *port = mPort;
		nLeds *= (3);
		register uint8_t *end = data + nLeds; 
		register uint8_t hi = *port | mask;
		register uint8_t lo = *port & ~mask;
		*port = lo;

		while(data != end) { 
			register uint8_t b = *data++;
			bitSetFast<7>(port, hi, lo, b);
			bitSetFast<6>(port, hi, lo, b);
			bitSetFast<5>(port, hi, lo, b);
			bitSetFast<4>(port, hi, lo, b);
			bitSetFast<3>(port, hi, lo, b);
			bitSetFast<2>(port, hi, lo, b);
			bitSetFast<1>(port, hi, lo, b);
			bitSetLast<0, END_OF_LOOP>(port, hi, lo, b);
		}
		sei();
	}

	virtual void showARGB(uint8_t *data, int nLeds) { 
	}
};

#endif
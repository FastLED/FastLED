#ifndef __INC_CLOCKLESS_H
#define __INC_CLOCKLESS_H

#include "controller.h"
#include <avr/interrupt.h> // for cli/se definitions

// Macro to convert from nano-seconds to clocks
// #define NS(_NS) (_NS / (1000 / (F_CPU / 1000000L)))
#if F_CPU < 96000000
#define NS(_NS) ( (_NS * (F_CPU / 1000000L))) / 1000
#else
#define NS(_NS) ( (_NS * (F_CPU / 2000000L))) / 1000
#endif

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
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
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
	
	#define END_OF_LOOP 6 		// loop compare, jump, next uint8_t load
	template <int N, int ADJ>inline static void bitSetLast(register data_ptr_t port, register data_t hi, register data_t lo, register uint8_t b) { 
		// First cycle
		FastPin<DATA_PIN>::fastset(port, hi); 							// 1 clock cycle if using out, 2 otherwise
		delaycycles<T1 - (_CYCLES(DATA_PIN) + 1)>();					// 1st cycle length minus 1 clock for out, 1 clock for sbrs
		__asm__ __volatile__ ("sbrs %0, %1" :: "r" (b), "M" (N) :); // 1 clock for check (+1 if skipping, next op is also 1 clock)

		// Second cycle
		FastPin<DATA_PIN>::fastset(port, lo);							// 1/2 clock cycle if using out
		delaycycles<T2 - _CYCLES(DATA_PIN)>(); 						// 2nd cycle length minus 1/2 clock for out

		// Third cycle
		FastPin<DATA_PIN>::fastset(port, lo);							// 1/2 clock cycle if using out
		delaycycles<T3 - (_CYCLES(DATA_PIN) + ADJ)>();				// 3rd cycle length minus 7 clocks for out, loop compare, jump, next uint8_t load
	}
#endif

	virtual void showRGB(register uint8_t *data, register int nLeds) {
		cli();
		
		register data_t mask = mPinMask;
		register data_ptr_t port = mPort;
		nLeds *= (3);
		register uint8_t *end = data + nLeds; 
		register data_t hi = *port | mask;
		register data_t lo = *port & ~mask;
		*port = lo;

#if defined(__MK20DX128__)
		register uint32_t b = *data++;
		while(data <= end) { 
			// TODO: hand rig asm version of this method.  The timings are based on adjusting/studying GCC compiler ouptut.  This
			// will bite me in the ass at some point, I know it.
			for(register uint32_t i = 7; i > 0; i--) { 
				FastPin<DATA_PIN>::fastset(port, hi);
				delaycycles<T1 - 3>(); // 3 cycles - 1 store, 1 test, 1 if
				if(b & 0x80) { FastPin<DATA_PIN>::fastset(port, hi); } else { FastPin<DATA_PIN>::fastset(port, lo); }
				b <<= 1;
				delaycycles<T2 - 3>(); // 3 cycles, 1 store, 1 store/skip,  1 shift 
				FastPin<DATA_PIN>::fastset(port, lo);
				delaycycles<T3 - 3>(); // 3 cycles, 1 store, 1 sub, 1 branch backwards
			}
			// extra delay because branch is faster falling through
			delaycycles<1>();

			// 8th bit, interleave loading rest of data
			FastPin<DATA_PIN>::fastset(port, hi);
			delaycycles<T1 - 3>();
			if(b & 0x80) { FastPin<DATA_PIN>::fastset(port, hi); } else { FastPin<DATA_PIN>::fastset(port, lo); }
			delaycycles<T2 - 2>(); // 4 cycles, 2 store, store/skip
			FastPin<DATA_PIN>::fastset(port, lo);
			b = *data++;
			delaycycles<T3 - 6>(); // 1 store, 2 load, 1 cmp, 1 branch backwards, 1 movim
		};
#else
		while(data <= end) { 
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
#endif
		sei();
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(uint8_t *data, int nLeds) { 
		// TODO: IMPLEMENTME
	}
#endif
};

#endif
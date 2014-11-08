#ifndef __INC_CLOCKLESS_TRINKET_H
#define __INC_CLOCKLESS_TRINKET_H

#include "controller.h"
#include "lib8tion.h"
#include "delay.h"
#include <avr/interrupt.h> // for cli/se definitions

#if defined(FASTLED_AVR)

// Scaling macro choice
#ifndef TRINKET_SCALE
#define TRINKET_SCALE 1
// whether or not to use dithering
#define DITHER 1
#endif

// Variations on the functions in delay.h - w/a loop var passed in to preserve registers across calls by the optimizer/compiler
template<int CYCLES> inline void _dc(register uint8_t & loopvar);

template<int _LOOP, int PAD> inline void _dc_AVR(register uint8_t & loopvar) {
	_dc<PAD>(loopvar);
	// The convolution in here is to ensure that the state of the carry flag coming into the delay loop is preserved
	asm __volatile__ (  "BRCS L_PC%=\n\t"
						"        LDI %[loopvar], %[_LOOP]\n\tL_%=: DEC %[loopvar]\n\t BRNE L_%=\n\tBREQ L_DONE%=\n\t"
						"L_PC%=: LDI %[loopvar], %[_LOOP]\n\tLL_%=: DEC %[loopvar]\n\t BRNE LL_%=\n\tBSET 0\n\t"
						"L_DONE%=:\n\t"
						:
							[loopvar] "+a" (loopvar) : [_LOOP] "M" (_LOOP) : );
}

template<int CYCLES> __attribute__((always_inline)) inline void _dc(register uint8_t & loopvar) {
	_dc_AVR<CYCLES/6,CYCLES%6>(loopvar);
}
template<> __attribute__((always_inline)) inline void _dc<-6>(register uint8_t & loopvar) {}
template<> __attribute__((always_inline)) inline void _dc<-5>(register uint8_t & loopvar) {}
template<> __attribute__((always_inline)) inline void _dc<-4>(register uint8_t & loopvar) {}
template<> __attribute__((always_inline)) inline void _dc<-3>(register uint8_t & loopvar) {}
template<> __attribute__((always_inline)) inline void _dc<-2>(register uint8_t & loopvar) {}
template<> __attribute__((always_inline)) inline void _dc<-1>(register uint8_t & loopvar) {}
template<> __attribute__((always_inline)) inline void _dc<0>(register uint8_t & loopvar) {}
template<> __attribute__((always_inline)) inline void _dc<1>(register uint8_t & loopvar) {asm __volatile__("mov r0,r0":::);}
template<> __attribute__((always_inline)) inline void _dc<2>(register uint8_t & loopvar) {asm __volatile__("rjmp .+0":::);}
template<> __attribute__((always_inline)) inline void _dc<3>(register uint8_t & loopvar) { _dc<2>(loopvar); _dc<1>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<4>(register uint8_t & loopvar) { _dc<2>(loopvar); _dc<2>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<5>(register uint8_t & loopvar) { _dc<2>(loopvar); _dc<3>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<6>(register uint8_t & loopvar) { _dc<2>(loopvar); _dc<2>(loopvar); _dc<2>(loopvar);}
template<> __attribute__((always_inline)) inline void _dc<7>(register uint8_t & loopvar) { _dc<4>(loopvar); _dc<3>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<8>(register uint8_t & loopvar) { _dc<4>(loopvar); _dc<4>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<9>(register uint8_t & loopvar) { _dc<5>(loopvar); _dc<4>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<10>(register uint8_t & loopvar) { _dc<6>(loopvar); _dc<4>(loopvar); }

#define D1(ADJ) _dc<T1-(AVR_PIN_CYCLES(DATA_PIN)+ADJ)>(loopvar);
#define D2(ADJ) _dc<T2-(AVR_PIN_CYCLES(DATA_PIN)+ADJ)>(loopvar);
#define D3(ADJ) _dc<T3-(AVR_PIN_CYCLES(DATA_PIN)+ADJ)>(loopvar);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base template for clockless controllers.  These controllers have 3 control points in their cycle for each bit.  The first point
// is where the line is raised hi.  The second point is where the line is dropped low for a zero.  The third point is where the
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CLEDController {
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() {
		FastPin<DATA_PIN>::setOutput();
	}

	virtual void clearLeds(int nLeds) {
		CRGB zeros(0,0,0);
		showAdjTime((uint8_t*)&zeros, nLeds, zeros, false, 0);
	}

protected:

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
		showAdjTime((uint8_t*)&rgbdata, nLeds, scale, false, 0);
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
		showAdjTime((uint8_t*)rgbdata, nLeds, scale, true, 0);
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
		showAdjTime((uint8_t*)rgbdata, nLeds, scale, true, 1);
	}
#endif

	void showAdjTime(const uint8_t *data, int nLeds, CRGB & scale, bool advance, int skip) {
		PixelController<RGB_ORDER> pixels(data, nLeds, scale, getDither(), advance, skip);

		mWait.wait();
		cli();

		showRGBInternal(pixels);

		// Adjust the timer
#if !defined(NO_CORRECTION) || (NO_CORRECTION == 0)
		uint32_t microsTaken = (uint32_t)nLeds * (uint32_t)CLKS_TO_MICROS(24 * (T1 + T2 + T3));
                if(microsTaken > 1024) {
		  MS_COUNTER += (microsTaken >> 10);
 		} else {
		  MS_COUNTER++;
		}
#endif
		sei();
		mWait.mark();
	}
#define USE_ASM_MACROS

// The variables that our various asm statemetns use.  The same block of variables needs to be declared for
// all the asm blocks because GCC is pretty stupid and it would clobber variables happily or optimize code away too aggressively
#define ASM_VARS : /* write variables */				\
				[count] "+x" (count),					\
				[data] "+z" (data),						\
				[b0] "+a" (b0),							\
				[b1] "+a" (b1),							\
				[b2] "+a" (b2),							\
				[scale_base] "+a" (scale_base),			\
				[loopvar] "+a" (loopvar),				\
				[d0] "+r" (d0),							\
				[d1] "+r" (d1),							\
				[d2] "+r" (d2)							\
				: /* use variables */					\
				[ADV] "r" (advanceBy),					\
				[hi] "r" (hi),							\
				[lo] "r" (lo),							\
				[s0] "r" (s0),							\
				[s1] "r" (s1),							\
				[s2] "r" (s2),							\
				[PORT] "M" (FastPin<DATA_PIN>::port()-0x20),		\
				[O0] "M" (RGB_BYTE0(RGB_ORDER)),		\
				[O1] "M" (RGB_BYTE1(RGB_ORDER)),		\
				[O2] "M" (RGB_BYTE2(RGB_ORDER))		\
				: /* clobber registers */


// 1 cycle, write hi to the port
#define HI1 if((int)(FastPin<DATA_PIN>::port())-0x20 < 64) { asm __volatile__("out %[PORT], %[hi]" ASM_VARS ); } else { *FastPin<DATA_PIN>::port()=hi; }
// 1 cycle, write lo to the port
#define LO1 if((int)(FastPin<DATA_PIN>::port())-0x20 < 64) { asm __volatile__("out %[PORT], %[lo]" ASM_VARS ); } else { *FastPin<DATA_PIN>::port()=lo; }
// 2 cycles, sbrs on flipping the lne to lo if we're pushing out a 0
#define QLO2(B, N) asm __volatile__("sbrs %[" #B "], " #N ASM_VARS ); LO1;
// load a byte from ram into the given var with the given offset
#define LD2(B,O) asm __volatile__("ldd %[" #B "], Z + %[" #O "]" ASM_VARS );
// 4 cycles - load a byte from ram into the scaling scratch space with the given offset, clear the target var, clear carry
#define LDSCL4(B,O) asm __volatile__("ldd %[scale_base], Z + %[" #O "]\n\tclr %[" #B "]\n\tclc" ASM_VARS );
// 2 cycles - perform one step of the scaling (if a given bit is set in scale, add scale-base to the scratch space)
#define SCALE02(B, N) asm __volatile__("sbrc %[s0], " #N "\n\tadd %[" #B "], %[scale_base]" ASM_VARS );
#define SCALE12(B, N) asm __volatile__("sbrc %[s1], " #N "\n\tadd %[" #B "], %[scale_base]" ASM_VARS );
#define SCALE22(B, N) asm __volatile__("sbrc %[s2], " #N "\n\tadd %[" #B "], %[scale_base]" ASM_VARS );

// apply dithering value  before we do anything with scale_base
#define PRESCALE4(D) if(DITHER) { asm __volatile__("cpse %[scale_base], __zero_reg__\n\t add %[scale_base],%[" #D "]\n\tbrcc L_%=\n\tldi %[scale_base], 0xFF\n\tL_%=:\n\t" ASM_VARS); } \
				      else { _dc<4>(loopvar); }

// Do the add for the prescale
#define PRESCALEA2(D) if(DITHER) { asm __volatile__("cpse %[scale_base], __zero_reg__\n\t add %[scale_base],%[" #D "]\n\t" ASM_VARS); } \
				      else { _dc<2>(loopvar); }
// Do the clamp for the prescale, clear carry when we're done - NOTE: Must ensure carry flag state is preserved!
#define PRESCALEB3(D) if(DITHER) { asm __volatile__("brcc L_%=\n\tldi %[scale_base], 0xFF\n\tL_%=:\n\tCLC" ASM_VARS); } \
				      else { _dc<3>(loopvar); }


// 1 cycle - rotate right, pulling in from carry
#define ROR1(B) asm __volatile__("ror %[" #B "]" ASM_VARS );
// 1 cycle, clear the carry bit
#define CLC1 asm __volatile__("clc" ASM_VARS );
// 1 cycle, move one register to another
#define MOV1(B1, B2) asm __volatile__("mov %[" #B1 "], %[" #B2 "]" ASM_VARS );
// 4 cycles, rotate, clear carry, scale next bit
#define RORSC04(B, N) ROR1(B) CLC1 SCALE02(B, N)
#define RORSC14(B, N) ROR1(B) CLC1 SCALE12(B, N)
#define RORSC24(B, N) ROR1(B) CLC1 SCALE22(B, N)
// 4 cycles, scale bit, rotate, clear carry
#define SCROR04(B, N) SCALE02(B,N) ROR1(B) CLC1
#define SCROR14(B, N) SCALE12(B,N) ROR1(B) CLC1
#define SCROR24(B, N) SCALE22(B,N) ROR1(B) CLC1

/////////////////////////////////////////////////////////////////////////////////////
// Loop life cycle

// dither adjustment macro - should be kept in sync w/what's in stepDithering
#define ADJDITHER2(D, E) D = E - D;

// #define xstr(a) str(a)
// #define str(a) #a
// #define ADJDITHER2(D,E) asm __volatile__("subi %[" #D "], " xstr(DUSE) "\n\tand %[" #D "], %[" #E "]\n\t" ASM_VARS);

// define the beginning of the loop
#define LOOP asm __volatile__("1:" ASM_VARS );
// define the end of the loop
#define DONE asm __volatile__("2:" ASM_VARS );

// 2 cycles - increment the data pointer
#define IDATA2 asm __volatile__("add %A[data], %[ADV]\n\tadc %B[data], __zero_reg__"  ASM_VARS );
// 2 cycles - decrement the counter
#define DCOUNT2 asm __volatile__("sbiw %[count], 1" ASM_VARS );
// 2 cycles - jump to the beginning of the loop
#define JMPLOOP2 asm __volatile__("rjmp 1b" ASM_VARS );
// 2 cycles - jump out of the loop
#define BRLOOP1 asm __volatile__("breq 2f" ASM_VARS );

#define DADVANCE 3
#define DUSE (0xFF - (DADVANCE-1))

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static void __attribute__ ((always_inline))  showRGBInternal(PixelController<RGB_ORDER> & pixels) {
		uint8_t *data = (uint8_t*)pixels.mData;
		data_ptr_t port = FastPin<DATA_PIN>::port();
		data_t mask = FastPin<DATA_PIN>::mask();
		uint8_t scale_base = 0;

		// register uint8_t *end = data + nLeds;
		data_t hi = *port | mask;
		data_t lo = *port & ~mask;
		*port = lo;

		uint8_t b0 = 0;
		uint8_t b1 = 0;
		uint8_t b2 = 0;

		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		b0 = pixels.loadAndScale0();

		// pull the dithering/adjustment values out of the pixels object for direct asm access

		uint8_t advanceBy = pixels.advanceBy();
		uint16_t count = pixels.mLen;

		uint8_t s0 = pixels.mScale.raw[RO(0)];
		uint8_t s1 = pixels.mScale.raw[RO(1)];
		uint8_t s2 = pixels.mScale.raw[RO(2)];
		uint8_t d0 = pixels.d[RO(0)];
		uint8_t d1 = pixels.d[RO(1)];
		uint8_t d2 = pixels.d[RO(2)];
		uint8_t e0 = pixels.e[RO(0)];
		uint8_t e1 = pixels.e[RO(1)];
		uint8_t e2 = pixels.e[RO(2)];

		uint8_t loopvar=0;

		{
			while(count--)
			{
				// Loop beginning, does some stuff that's outside of the pixel write cycle, namely incrementing d0-2 and masking off
				// by the E values (see the definition )
				// LOOP;
				ADJDITHER2(d0,e0);
				ADJDITHER2(d1,e1);
				ADJDITHER2(d2,e2);

				hi = *port | mask;
				lo = *port & ~mask;

				// Sum of the clock counts across each row should be 10 for 8Mhz, WS2811
				// The values in the D1/D2/D3 indicate how many cycles the previous column takes
				// to allow things to line back up.
				//
				// While writing out byte 0, we're loading up byte 1, applying the dithering adjustment,
				// then scaling it using 8 cycles of shift/add interleaved in between writing the bits
				// out.  When doing byte 1, we're doing the above for byte 2.  When we're doing byte 2,
				// we're cycling back around and doing the above for byte 0.
#if TRINKET_SCALE
				// Inline scaling - RGB ordering
				HI1 D1(1) QLO2(b0, 7) LDSCL4(b1,O1) 	D2(4)	LO1	PRESCALEA2(d1)	D3(2)
				HI1	D1(1) QLO2(b0, 6) PRESCALEB3(d1)	D2(3)	LO1	SCALE12(b1,0)	D3(2)
				HI1 D1(1) QLO2(b0, 5) RORSC14(b1,1) 	D2(4)	LO1 ROR1(b1) CLC1	D3(2)
				HI1 D1(1) QLO2(b0, 4) SCROR14(b1,2)		D2(4)	LO1 SCALE12(b1,3)	D3(2)
				HI1 D1(1) QLO2(b0, 3) RORSC14(b1,4) 	D2(4)	LO1 ROR1(b1) CLC1	D3(2)
				HI1 D1(1) QLO2(b0, 2) SCROR14(b1,5) 	D2(4)	LO1 SCALE12(b1,6)	D3(2)
				HI1 D1(1) QLO2(b0, 1) RORSC14(b1,7) 	D2(4)	LO1 ROR1(b1) CLC1	D3(2)
				HI1 D1(1) QLO2(b0, 0) 				 	D2(0)	LO1 				D3(0)
				switch(XTRA0) {
					case 4: HI1 D1(1) QLO2(b0,0) D2(0) LO1 D3(0);
					case 3: HI1 D1(1) QLO2(b0,0) D2(0) LO1 D3(0);
					case 2: HI1 D1(1) QLO2(b0,0) D2(0) LO1 D3(0);
					case 1: HI1 D1(1) QLO2(b0,0) D2(0) LO1 D3(0);
				}
				HI1 D1(1) QLO2(b1, 7) LDSCL4(b2,O2) 	D2(4)	LO1	PRESCALEA2(d2)	D3(2)
				HI1	D1(1) QLO2(b1, 6) PRESCALEB3(d2)	D2(3)	LO1	SCALE22(b2,0)	D3(2)
				HI1 D1(1) QLO2(b1, 5) RORSC24(b2,1) 	D2(4)	LO1 ROR1(b2) CLC1	D3(2)
				HI1 D1(1) QLO2(b1, 4) SCROR24(b2,2)		D2(4)	LO1 SCALE22(b2,3)	D3(2)
				HI1 D1(1) QLO2(b1, 3) RORSC24(b2,4) 	D2(4)	LO1 ROR1(b2) CLC1	D3(2)
				HI1 D1(1) QLO2(b1, 2) SCROR24(b2,5) 	D2(4)	LO1 SCALE22(b2,6)	D3(2)
				HI1 D1(1) QLO2(b1, 1) RORSC24(b2,7) 	D2(4)	LO1 ROR1(b2) CLC1	D3(2)
				HI1 D1(1) QLO2(b1, 0) IDATA2 CLC1		D2(3) 	LO1 				D3(0)
				switch(XTRA0) {
					case 4: HI1 D1(1) QLO2(b1,0) D2(0) LO1 D3(0);
					case 3: HI1 D1(1) QLO2(b1,0) D2(0) LO1 D3(0);
					case 2: HI1 D1(1) QLO2(b1,0) D2(0) LO1 D3(0);
					case 1: HI1 D1(1) QLO2(b1,0) D2(0) LO1 D3(0);
				}
				HI1 D1(1) QLO2(b2, 7) LDSCL4(b0,O0) 	D2(4)	LO1	PRESCALEA2(d0)	D3(2)
				HI1	D1(1) QLO2(b2, 6) PRESCALEB3(d0)	D2(3)	LO1	SCALE02(b0,0)	D3(2)
				HI1 D1(1) QLO2(b2, 5) RORSC04(b0,1) 	D2(4)	LO1 ROR1(b0) CLC1	D3(2)
				HI1 D1(1) QLO2(b2, 4) SCROR04(b0,2)		D2(4)	LO1 SCALE02(b0,3)	D3(2)
				HI1 D1(1) QLO2(b2, 3) RORSC04(b0,4) 	D2(4)	LO1 ROR1(b0) CLC1	D3(2)
				HI1 D1(1) QLO2(b2, 2) SCROR04(b0,5) 	D2(4)	LO1 SCALE02(b0,6)	D3(2)
				HI1 D1(1) QLO2(b2, 1) RORSC04(b0,7) 	D2(4)	LO1 ROR1(b0) CLC1	D3(2)
				// HI1 D1(1) QLO2(b2, 0) DCOUNT2 BRLOOP1 	D2(3) 	LO1 D3(2) JMPLOOP2
				HI1 D1(1) QLO2(b2, 0) 					D2(0) 	LO1 				D3(0)
				switch(XTRA0) {
					case 4: HI1 D1(1) QLO2(b1,0) D2(0) LO1 D3(0);
					case 3: HI1 D1(1) QLO2(b1,0) D2(0) LO1 D3(0);
					case 2: HI1 D1(1) QLO2(b1,0) D2(0) LO1 D3(0);
					case 1: HI1 D1(1) QLO2(b1,0) D2(0) LO1 D3(0);
				}
#else
				// no inline scaling - non-straight RGB ordering
				HI1	D1(1) QLO2(b0, 7) LD2(b1,O1)	D2(2)	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 6) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 5) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 4) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 3) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 2) 				D2(0)	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 1) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 0) 				D2(0) 	LO1 D3(0)
				HI1	D1(1) QLO2(b1, 7) LD2(b2,O2) 	D2(2)	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 6) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 5) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 4) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 3) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 2) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 1) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 0) IDATA2 		D2(2)	LO1 D3(0)
				HI1	D1(1) QLO2(b2, 7) LD2(b0,O0) 	D2(2)	LO1 D3(0)
				HI1 D1(1) QLO2(b2, 6) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b2, 5) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b2, 4) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b2, 3) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b2, 2) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b2, 1) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b2, 0) 				D2(0) 	LO1 D3(0)
#endif
				// DONE
				// D2(4) LO1 D3(0)
			}
		}
		// save the d values
		// d[0] = d0;
		// d[1] = d1;
		// d[2] = d2;
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(struct CARGB *data, int nLeds) {
		// TODO: IMPLEMENTME
	}
#endif
};

#endif

#endif

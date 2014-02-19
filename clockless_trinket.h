#ifndef __INC_CLOCKLESS_TRINKET_H
#define __INC_CLOCKLESS_TRINKET_H

#include "controller.h"
#include "lib8tion.h"
#include "delay.h"
#include <avr/interrupt.h> // for cli/se definitions

#if defined(LIB8_ATTINY)

// Scaling macro choice
#ifndef TRINKET_SCALE
#define TRINKET_SCALE 1
// whether or not to use dithering
#define DITHER 1
// whether or not to enable scale_video adjustments
#define SCALE_VIDEO 1
#endif

// Variations on the functions in delay.h - w/a loop var passed in to preserve registers across calls by the optimizer/compiler
template<int CYCLES> inline void _dc(register uint8_t & loopvar);

template<int _LOOP, int PAD> inline void _dc_AVR(register uint8_t & loopvar) { 
	_dc<PAD>(loopvar);
	asm __volatile__ ( "LDI %[loopvar], %[_LOOP]\n\tL_%=: DEC %[loopvar]\n\t BRNE L_%=\n\t" : 
							[loopvar] "+a" (loopvar) : [_LOOP] "M" (_LOOP) : );
}

template<int CYCLES> __attribute__((always_inline)) inline void _dc(register uint8_t & loopvar) { 
	_dc_AVR<CYCLES/3,CYCLES%3>(loopvar);
}
template<> __attribute__((always_inline)) inline void _dc<0>(register uint8_t & loopvar) {}
template<> __attribute__((always_inline)) inline void _dc<1>(register uint8_t & loopvar) {asm __volatile__("cp r0,r0":::);}
template<> __attribute__((always_inline)) inline void _dc<2>(register uint8_t & loopvar) {asm __volatile__("rjmp .+0":::);}

#define D1(ADJ) _dc<T1-(1+ADJ)>(loopvar);
#define D2(ADJ) _dc<T2-(1+ADJ)>(loopvar);
#define D3(ADJ) _dc<T3-(1+ADJ)>(loopvar);

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

	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() { 
		FastPin<DATA_PIN>::setOutput();
	}

	virtual void clearLeds(int nLeds) {
		static byte zeros[3] = {0,0,0};
		showRGBInternal_AdjTime(0, false, nLeds, CRGB(0,0,0), zeros);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale = CRGB::White) {
		showRGBInternal_AdjTime(0, false, nLeds, scale, (const byte*)&data);
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale = CRGB::White) { 
		showRGBInternal_AdjTime(0, true, nLeds, scale, (const byte*)rgbdata);
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale = CRGB::White) { 
		showRGBInternal_AdjTime(1, true, nLeds, scale, (const byte*)rgbdata);
	}
#endif

	void __attribute__ ((noinline)) showRGBInternal_AdjTime(int skip, bool advance, int nLeds, CRGB  scale,  const byte *rgbdata) {
		mWait.wait();
		cli();

		showRGBInternal(skip, advance, nLeds, scale, rgbdata);


		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

#define USE_ASM_MACROS
	
// The variables that our various asm statemetns use.  The same block of variables needs to be declared for
// all the asm blocks because GCC is pretty stupid and it would clobber variables happily or optimize code away too aggressively			
#define ASM_VARS : /* write variables */				\
				[b0] "+a" (b0),							\
				[b1] "+a" (b1),							\
				[b2] "+a" (b2),							\
				[count] "+x" (count),					\
				[scale_base] "+a" (scale_base),			\
				[data] "+z" (data),						\
				[loopvar] "+a" (loopvar)				\
				: /* use variables */					\
				[ADV] "r" (advanceBy),					\
				[hi] "r" (hi),							\
				[lo] "r" (lo),							\
				[s0] "r" (s0),							\
				[s1] "r" (s1),							\
				[s2] "r" (s2),							\
				[d0] "r" (d0),							\
				[d1] "r" (d1),							\
				[d2] "r" (d2),							\
				[e0] "r" (e0),							\
				[e1] "r" (e1),							\
				[e2] "r" (e2),							\
				[PORT] "M" (FastPin<DATA_PIN>::port()-0x20),			\
				[O0] "M" (RGB_BYTE0(RGB_ORDER)),		\
				[O1] "M" (RGB_BYTE1(RGB_ORDER)),		\
				[O2] "M" (RGB_BYTE2(RGB_ORDER))		\
				: /* clobber registers */				


// 1 cycle, write hi to the port
#define HI1 asm __volatile__("out %[PORT], %[hi]" ASM_VARS );
// 1 cycle, write lo to the port
#define LO1 asm __volatile__("out %[PORT], %[lo]" ASM_VARS );
// 2 cycles, sbrs on flipping the lne to lo if we're pushing out a 0
#define QLO2(B, N) asm __volatile__("sbrs %[" #B "], " #N ASM_VARS ); LO1;
// load a byte from ram into the given var with the given offset
#define LD2(B,O) asm __volatile__("ldd %[" #B "], Z + %[" #O "]" ASM_VARS );
// 3 cycles - load a byte from ram into the scaling scratch space with the given offset, clear the target var
#define LDSCL3(B,O) asm __volatile__("ldd %[scale_base], Z + %[" #O "]\n\tclr %[" #B "]" ASM_VARS );
// 2 cycles - perform one step of the scaling (if a given bit is set in scale, add scale-base to the scratch space)
#define SCALE02(B, N) asm __volatile__("sbrc %[s0], " #N "\n\tadd %[" #B "], %[scale_base]" ASM_VARS );
#define SCALE12(B, N) asm __volatile__("sbrc %[s1], " #N "\n\tadd %[" #B "], %[scale_base]" ASM_VARS );
#define SCALE22(B, N) asm __volatile__("sbrc %[s2], " #N "\n\tadd %[" #B "], %[scale_base]" ASM_VARS );

// apply dithering value  before we do anything with scale_base
#define PRESCALE4(D) if(DITHER) { asm __volatile__("cpse %[scale_base], __zero_reg__\n\t add %[scale_base],%[" #D "]\n\tbrcc L_%=\n\tldi %[scale_base], 0xFF\n\tL_%=:\n\t" ASM_VARS); } \
				      else { _dc<4>(loopvar); }

// Do a (rough) approximation of the nscale8_video jump
#if (SCALE_VIDEO == 1) 
#define VIDADJ2(B) asm __volatile__("cpse %[scale_base], __zero_reg__\n\tsubi %[" #B "], 0xFF\n\t" ASM_VARS);
#else
#define VIDADJ2(B) asm __volatile__("rjmp .+0" ASM_VARS);
#endif

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

// #define ADJUST_DITHER  d0 += DADVANCE; d1 += DADVANCE; d2 += DADVANCE; d0 &= e0; d1 &= e1; d2 &= d2; 
#define ADJDITHER2(D, E) D += DADVANCE; D &= E;
// #define xstr(a) str(a)
// #define str(a) #a
// #define ADJDITHER2(D,E) asm __volatile__("subi %[" #D "], " xstr(DUSE) "\n\tand %[" #D "], %[" #E "]\n\t" ASM_VARS);

// define the beginning of the loop
#define LOOP asm __volatile__("1:" ASM_VARS );
// define the end of the loop
#define DONE asm __volatile__("2:" ASM_VARS );

// 2 cycles - increment the data pointer
#define IDATA2 asm __volatile__("add %A[data], %A[ADV]\n\tadc %B[data], %B[ADV]"  ASM_VARS );
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
	static void __attribute__ ((noinline)) showRGBInternal(int skip, bool advance, int nLeds, CRGB & scale,  const byte *rgbdata) {
		byte *data = (byte*)rgbdata;
		data_t mask = FastPin<DATA_PIN>::mask();
		data_ptr_t port = FastPin<DATA_PIN>::port();
		// register uint8_t *end = data + nLeds; 
		data_t hi = *port | mask;
		data_t lo = *port & ~mask;
		*port = lo;

		register uint8_t d0, d1, d2;
		register uint8_t e0, e1, e2;
		uint8_t s0, s1, s2;
		uint8_t b0, b1, b2;
		static uint8_t d[3] = {0,0,0};

		uint16_t count = nLeds;
		uint8_t scale_base = 0;
		uint16_t advanceBy = advance ? (skip+3) : 0;
		// uint8_t dadv = DADVANCE;

		// initialize the scales
		s0 = scale.raw[B0];
		s1 = scale.raw[B1];
		s2 = scale.raw[B2];

		// initialize the e & d values
		uint8_t S;
		S = s0; e0 = 0xFF; while(S >>= 1) { e0 >>= 1; }
		d0 = d[0] & e0;
		S = s1; e1 = 0xFF; while(S >>= 1) { e1 >>= 1; }
		d1 = d[1] & e1;
		S = s2; e2 = 0xFF; while(S >>= 1) { e2 >>= 1; }
		d2 = d[2] & e0;

		b0 = data[RGB_BYTE0(RGB_ORDER)];
		if(DITHER && b0) { b0 = qadd8(b0, d0); }
		b0 = scale8_video(b0, s0);
		b1 = 0;
		b2 = 0;
		register uint8_t loopvar=0;

		{
			{
				// Loop beginning, does some stuff that's outside of the pixel write cycle, namely incrementing d0-2 and masking off
				// by the E values (see the definition )
				LOOP; 
				ADJDITHER2(d0,e0)
				ADJDITHER2(d1,e1) 
				ADJDITHER2(d2,e2) 
				VIDADJ2(b0);
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
				HI1 D1(1) QLO2(b0, 7) LDSCL3(b1,O1) 	D2(3)	LO1					D3(0)	
				HI1	D1(1) QLO2(b0, 6) PRESCALE4(d1)		D2(4)	LO1	SCALE12(b1,0)	D3(2)		
				HI1 D1(1) QLO2(b0, 5) RORSC14(b1,1) 	D2(4)	LO1 ROR1(b1) CLC1	D3(2)
				HI1 D1(1) QLO2(b0, 4) SCROR14(b1,2)		D2(4)	LO1 SCALE12(b1,3)	D3(2)			
				HI1 D1(1) QLO2(b0, 3) RORSC14(b1,4) 	D2(4)	LO1 ROR1(b1) CLC1	D3(2)			
				HI1 D1(1) QLO2(b0, 2) SCROR14(b1,5) 	D2(4)	LO1 SCALE12(b1,6)	D3(2)			
				HI1 D1(1) QLO2(b0, 1) RORSC14(b1,7) 	D2(4)	LO1 ROR1(b1) CLC1	D3(2)		
				HI1 D1(1) QLO2(b0, 0) 				 	D2(0)	LO1 VIDADJ2(b1)		D3(2)			
				HI1 D1(1) QLO2(b1, 7) LDSCL3(b2,O2) 	D2(3)	LO1					D3(0)	
				HI1	D1(1) QLO2(b1, 6) PRESCALE4(d2)		D2(4)	LO1	SCALE22(b2,0)	D3(2)		
				HI1 D1(1) QLO2(b1, 5) RORSC24(b2,1) 	D2(4)	LO1 ROR1(b2) CLC1	D3(2)
				HI1 D1(1) QLO2(b1, 4) SCROR24(b2,2)		D2(4)	LO1 SCALE22(b2,3)	D3(2)	
				HI1 D1(1) QLO2(b1, 3) RORSC24(b2,4) 	D2(4)	LO1 ROR1(b2) CLC1	D3(2)	
				HI1 D1(1) QLO2(b1, 2) SCROR24(b2,5) 	D2(4)	LO1 SCALE22(b2,6)	D3(2)	
				HI1 D1(1) QLO2(b1, 1) RORSC24(b2,7) 	D2(4)	LO1 ROR1(b2) CLC1	D3(2)
				HI1 D1(1) QLO2(b1, 0) IDATA2 			D2(2) 	LO1 VIDADJ2(b2)		D3(0)
				HI1 D1(1) QLO2(b2, 7) LDSCL3(b0,O0) 	D2(3)	LO1					D3(0)	
				HI1	D1(1) QLO2(b2, 6) PRESCALE4(d0)		D2(4)	LO1	SCALE22(b0,0)	D3(2)		
				HI1 D1(1) QLO2(b2, 5) RORSC04(b0,1) 	D2(4)	LO1 ROR1(b0) CLC1	D3(2)
				HI1 D1(1) QLO2(b2, 4) SCROR04(b0,2)		D2(4)	LO1 SCALE02(b0,3)	D3(2)	
				HI1 D1(1) QLO2(b2, 3) RORSC04(b0,4) 	D2(4)	LO1 ROR1(b0) CLC1	D3(2)	
				HI1 D1(1) QLO2(b2, 2) SCROR04(b0,5) 	D2(4)	LO1 SCALE02(b0,6)	D3(2)	
				HI1 D1(1) QLO2(b2, 1) RORSC04(b0,7) 	D2(4)	LO1 ROR1(b0) CLC1	D3(2)
				HI1 D1(1) QLO2(b2, 0) DCOUNT2 BRLOOP1 	D2(3) 	LO1 D3(2) JMPLOOP2	
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
				HI1 D1(1) QLO2(b2, 0) DCOUNT2 BRLOOP1 D2(3) 		LO1 D3(2) JMPLOOP2	
#endif			
				DONE
				D2(4) LO1 D3(0)
			}
		}

		// save the d values
		d[0] = d0;
		d[1] = d1;
		d[2] = d2;
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(struct CARGB *data, int nLeds) { 
		// TODO: IMPLEMENTME
	}
#endif
};

#endif

#endif

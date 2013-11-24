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

#ifndef TRINKET_SCALE
#define TRINKET_SCALE 1
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

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		mWait.wait();
		cli();

		showRGBInternal(0, false, nLeds, scale, (const byte*)&data);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, uint8_t scale = 255) { 
		mWait.wait();
		cli();

		showRGBInternal(0, true, nLeds, scale, (const byte*)rgbdata);

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

#define USE_ASM_MACROS

#define ASM_VARS : /* write variables */				\
				[b0] "+r" (b0),							\
				[b1] "+r" (b1),							\
				[b2] "+r" (b2),							\
				[count] "+x" (count),					\
				[scale_base] "+r" (scale_base),			\
				[data] "+z" (data)						\
				: /* use variables */					\
				[hi] "r" (hi),							\
				[lo] "r" (lo),							\
				[scale] "r" (scale),					\
				[ADV] "r" (advanceBy),					\
				[zero] "r" (zero),						\
				[O0] "M" (RGB_BYTE0(RGB_ORDER)),		\
				[O1] "M" (RGB_BYTE1(RGB_ORDER)),		\
				[O2] "M" (RGB_BYTE2(RGB_ORDER)),		\
				[PORT] "M" (0x18)						\
				: /* clobber registers */				

// 1 cycle, write hi to the port
#define HI1 asm __volatile__("out %[PORT], %[hi]" ASM_VARS );
// 1 cycle, write lo to the port
#define LO1 asm __volatile__("out %[PORT], %[lo]" ASM_VARS );
// 2 cycles, sbrs on flipping the lne to lo if we're pushing out a 0
#define QLO2(B, N) asm __volatile__("sbrs %[" #B "], " #N ASM_VARS ); LO1;
// 0 cycle placeholder nop to keep code columns lined up
#define NOP0 
// 1 cycle nop/delay
#define NOP1 asm __volatile__("cp r0,r0" ASM_VARS );
// 2 cycle nop/delay
#define NOP2 asm __volatile__("rjmp .+0" ASM_VARS );
// 3 cycle nop/delay
#define NOP3 NOP1 NOP2
// 4 cycle nop/delay
#define NOP4 NOP2 NOP2
// load a byte from ram into the given var with the given offset
#define LD2(B,O) asm __volatile__("ldd %[" #B "], Z + %[" #O "]" ASM_VARS );
// 3 cycles - load a byte from ram into the scaling scratch space with the given offset, clear the target var
#define LDSCL3(B,O) asm __volatile__("ldd %[scale_base], Z + %[" #O "]\n\tclr %[" #B "]" ASM_VARS );
// 2 cycles - increment the data pointer
#define IDATA2 asm __volatile__("add %A[data], %A[ADV]\n\tadc %B[data], %B[ADV]"  ASM_VARS );
// 2 cycles - decrement the counter
#define DCOUNT2 asm __volatile__("sbiw %[count], 1" ASM_VARS );
// 2 cycles - jump to the beginning of the loop
#define JMPLOOP2 asm __volatile__("rjmp 1b" ASM_VARS );
// 2 cycles - jump out of the loop
#define BRLOOP1 asm __volatile__("breq 2f" ASM_VARS );
// 2 cycles - perform one step of the scaling (if a given bit is set in scale, add scale-base to the scratch space)
#define SCALE2(B, N) asm __volatile__("sbrc %[scale], " #N "\n\tadd %[" #B "], %[scale_base]" ASM_VARS );
// 1 cycle - rotate right, pulling in from carry
#define ROR1(B) asm __volatile__("ror %[" #B "]" ASM_VARS );
// 1 cycle, clear the carry bit
#define CLC1 asm __volatile__("clc" ASM_VARS );
// 1 cycle, move one register to another
#define MOV1(B1, B2) asm __volatile__("mov %[" #B1 "], %[" #B2 "]" ASM_VARS );
// 4 cycles, rotate, clear carry, scale next bit
#define RORSC4(B, N) ROR1(B) CLC1 SCALE2(B, N)
// 4 cycles, scale bit, rotate, clear carry
#define SCROR4(B, N) SCALE2(B,N) ROR1(B) CLC1
// define the beginning of the loop
#define LOOP asm __volatile__("1:" ASM_VARS );
#define DONE asm __volatile__("2:" ASM_VARS );

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	static void __attribute__ ((noinline)) showRGBInternal(int skip, bool advance, int nLeds, uint8_t scale,  const byte *rgbdata) {
		byte *data = (byte*)rgbdata;
		data_t mask = FastPin<DATA_PIN>::mask();
		data_ptr_t port = FastPin<DATA_PIN>::port();
		// register uint8_t *end = data + nLeds; 
		data_t hi = *port | mask;
		data_t lo = *port & ~mask;
		*port = lo;

		uint8_t b0, b1, b2;
		uint16_t count = nLeds;
		uint8_t scale_base = 0;
		uint16_t advanceBy = advance ? (skip+3) : 0;
		const uint8_t zero = 0;
		b0 = data[RGB_BYTE0(RGB_ORDER)];
		// b0 = scale8(b0, scale);
		b1 = data[RGB_BYTE1(RGB_ORDER)];
		b2 = 0;

		if(RGB_ORDER == RGB) {
			// If the rgb order is RGB, we can cut back on program space usage by making a much more compact
			// representation.

			// multiply count by 3, don't use * because there's no hardware multiply
			count = count+(count<<1);
			advanceBy = advance ? 1 : 0;
			{
				/* asm */
				LOOP
				// Sum of the clock counts across each row should be 10 for 8Mhz, WS2811
#if TRINKET_SCALE
				// Inline scaling
				HI1	NOP0 QLO2(b0, 7) LDSCL3(b1,O1) NOP1 LO1 SCALE2(b1,0)			
				HI1 NOP0 QLO2(b0, 6) RORSC4(b1,1) 		LO1 ROR1(b1) CLC1		
				HI1 NOP0 QLO2(b0, 5) SCROR4(b1,2)		LO1 SCALE2(b1,3)			
				HI1 NOP0 QLO2(b0, 4) RORSC4(b1,4) 		LO1 ROR1(b1) CLC1			
				HI1 NOP0 QLO2(b0, 3) SCROR4(b1,5) 		LO1 SCALE2(b1,6)			
				HI1 NOP0 QLO2(b0, 2) RORSC4(b1,7) 		LO1 ROR1(b1) CLC1		
				HI1 NOP0 QLO2(b0, 1) IDATA2 NOP2		LO1 DCOUNT2	
				// The last bit is tricky.  We do the 3 cycle hi, bit check, lo.  Then we do a breq
				// that if we don't branch, will be 1 cycle, then 3 cycles of nop, then 1 cycle out, then
				// 2 cycles of jumping around the loop.  If we do branch, then that's 2 cycles, we need to 
				// wait 2 more cycles, then do the final low and waiting
				HI1 NOP0 QLO2(b0, 0) 
				BRLOOP1 
				MOV1(b0, b1) NOP2 	LO1 
				JMPLOOP2	
#else
				// no inline scaling
				HI1	NOP0 QLO2(b0, 7) LD2(b1,O1) 		LO1 NOP2			
				HI1 NOP0 QLO2(b0, 6) NOP4 		LO1 NOP2		
				HI1 NOP0 QLO2(b0, 5) NOP4		LO1 NOP2			
				HI1 NOP0 QLO2(b0, 4) NOP4 		LO1 NOP2			
				HI1 NOP0 QLO2(b0, 3) NOP4 		LO1 NOP2			
				HI1 NOP0 QLO2(b0, 2) NOP4 		LO1 NOP2		
				HI1 NOP0 QLO2(b0, 1) IDATA2 NOP2	LO1 DCOUNT2
				// The last bit is tricky.  We do the 3 cycle hi, bit check, lo.  Then we do a breq
				// that if we don't branch, will be 1 cycle, then 3 cycles of nop, then 1 cycle out, then
				// 2 cycles of jumping around the loop.  If we do branch, then that's 2 cycles, we need to 
				// wait 2 more cycles, then do the final low and waiting
				HI1 NOP0 QLO2(b2, 0) BRLOOP1 MOV1(b0,b1) NOP2 LO1 JMPLOOP2	
#endif			
				DONE
				NOP2 LO1 NOP2
			}
		} 
		else
		{
			{
				/* asm */
				LOOP
				// Sum of the clock counts across each row should be 10 for 8Mhz, WS2811
#if TRINKET_SCALE
				// Inline scaling
				HI1 NOP0 QLO2(b0, 7) LDSCL3(b1,O1) NOP1 LO1 SCALE2(b1,0)			
				HI1 NOP0 QLO2(b0, 6) RORSC4(b1,1) 		LO1 ROR1(b1) CLC1		
				HI1 NOP0 QLO2(b0, 5) SCROR4(b1,2)		LO1 SCALE2(b1,3)			
				HI1 NOP0 QLO2(b0, 4) RORSC4(b1,4) 		LO1 ROR1(b1) CLC1			
				HI1 NOP0 QLO2(b0, 3) SCROR4(b1,5) 		LO1 SCALE2(b1,6)			
				HI1 NOP0 QLO2(b0, 2) RORSC4(b1,7) 		LO1 ROR1(b1) CLC1		
				HI1 NOP0 QLO2(b0, 1) NOP4			 	LO1 NOP2			
				HI1 NOP0 QLO2(b0, 0) NOP4 				LO1 NOP2			
				HI1	NOP0 QLO2(b1, 7) LDSCL3(b2,O2) NOP1 LO1 SCALE2(b2,0)			
				HI1 NOP0 QLO2(b1, 6) RORSC4(b2,1) 		LO1 ROR1(b2) CLC1		
				HI1 NOP0 QLO2(b1, 5) SCROR4(b2,2)		LO1 SCALE2(b2,3)			
				HI1 NOP0 QLO2(b1, 4) RORSC4(b2,4) 		LO1 ROR1(b2) CLC1			
				HI1 NOP0 QLO2(b1, 3) SCROR4(b2,5) 		LO1 SCALE2(b2,6)			
				HI1 NOP0 QLO2(b1, 2) RORSC4(b2,7) 		LO1 ROR1(b2) CLC1		
				HI1 NOP0 QLO2(b1, 1) NOP4 				LO1 NOP2			
				HI1 NOP0 QLO2(b1, 0) IDATA2 NOP2 		LO1 NOP2			
				HI1	NOP0 QLO2(b2, 7) LDSCL3(b0,O0) NOP1 LO1 SCALE2(b0,0)			
				HI1 NOP0 QLO2(b2, 6) RORSC4(b0,1) 		LO1 ROR1(b0) CLC1		
				HI1 NOP0 QLO2(b2, 5) SCROR4(b0,2)		LO1 SCALE2(b0,3)			
				HI1 NOP0 QLO2(b2, 4) RORSC4(b0,4) 		LO1 ROR1(b0) CLC1			
				HI1 NOP0 QLO2(b2, 3) SCROR4(b0,5) 		LO1 SCALE2(b0,6)			
				HI1 NOP0 QLO2(b2, 2) RORSC4(b0,7) 		LO1 ROR1(b0) CLC1		
				HI1 NOP0 QLO2(b2, 1) NOP4				LO1 DCOUNT2	
				// The last bit is tricky.  We do the 3 cycle hi, bit check, lo.  Then we do a breq
				// that if we don't branch, will be 1 cycle, then 3 cycles of nop, then 1 cycle out, then
				// 2 cycles of jumping around the loop.  If we do branch, then that's 2 cycles, we need to 
				// wait 2 more cycles, then do the final low and waiting
				HI1 NOP0 QLO2(b2, 0) BRLOOP1 NOP3 		LO1 JMPLOOP2	
#else
				// no inline scaling
				HI1	NOP0 QLO2(b0, 7) LD2(b1,O1) NOP2 		LO1 NOP2
				HI1 NOP0 QLO2(b0, 6) NOP4 		LO1 NOP2
				HI1 NOP0 QLO2(b0, 5) NOP4		LO1 NOP2
				HI1 NOP0 QLO2(b0, 4) NOP4 		LO1 NOP2			
				HI1 NOP0 QLO2(b0, 3) NOP4 		LO1 NOP2			
				HI1 NOP0 QLO2(b0, 2) NOP4 		LO1 NOP2		
				HI1 NOP0 QLO2(b0, 1) NOP4		LO1 NOP2			
				HI1 NOP0 QLO2(b0, 0) NOP4 		LO1 NOP2			
				HI1	NOP0 QLO2(b1, 7) LD2(b2,O2) NOP2 		LO1 NOP2			
				HI1 NOP0 QLO2(b1, 6) NOP4 		LO1 NOP2		
				HI1 NOP0 QLO2(b1, 5) NOP4		LO1 NOP2			
				HI1 NOP0 QLO2(b1, 4) NOP4 		LO1 NOP2			
				HI1 NOP0 QLO2(b1, 3) NOP4 		LO1 NOP2			
				HI1 NOP0 QLO2(b1, 2) NOP4 		LO1 NOP2		
				HI1 NOP0 QLO2(b1, 1) NOP4 		LO1 NOP2			
				HI1 NOP0 QLO2(b1, 0) IDATA2 NOP2	LO1 NOP2			
				HI1	NOP0 QLO2(b2, 7) LD2(b0,O0) 		LO1 NOP2			
				HI1 NOP0 QLO2(b2, 6) NOP4 		LO1 NOP2		
				HI1 NOP0 QLO2(b2, 5) NOP4		LO1 NOP2			
				HI1 NOP0 QLO2(b2, 4) NOP4 		LO1 NOP2			
				HI1 NOP0 QLO2(b2, 3) NOP4 		LO1 NOP2			
				HI1 NOP0 QLO2(b2, 2) NOP4 		LO1 NOP2		
				HI1 NOP0 QLO2(b2, 1) NOP4		LO1 DCOUNT2	
				// The last bit is tricky.  We do the 3 cycle hi, bit check, lo.  Then we do a breq
				// that if we don't branch, will be 1 cycle, then 3 cycles of nop, then 1 cycle out, then
				// 2 cycles of jumping around the loop.  If we do branch, then that's 2 cycles, we need to 
				// wait 2 more cycles, then do the final low and waiting
				HI1 NOP0 QLO2(b2, 0) BRLOOP1 NOP3 		LO1 JMPLOOP2	
#endif			
				DONE
				NOP2 LO1 NOP2
			}
		}
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(struct CARGB *data, int nLeds) { 
		// TODO: IMPLEMENTME
	}
#endif
};

#endif

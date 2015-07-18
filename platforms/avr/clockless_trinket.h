#ifndef __INC_CLOCKLESS_TRINKET_H
#define __INC_CLOCKLESS_TRINKET_H

#include "controller.h"
#include "lib8tion.h"
#include <avr/interrupt.h> // for cli/se definitions

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_AVR)

// Scaling macro choice
#ifndef TRINKET_SCALE
#define TRINKET_SCALE 1
// whether or not to use dithering
#define DITHER 1
#endif

#if (F_CPU==8000000)
#define FASTLED_SLOW_CLOCK_ADJUST asm __volatile__ ("mov r0,r0\n\t");
#else
#define FASTLED_SLOW_CLOCK_ADJUST
#endif

#define US_PER_TICK (64 / (F_CPU/1000000))

// Variations on the functions in delay.h - w/a loop var passed in to preserve registers across calls by the optimizer/compiler
template<int CYCLES> inline void _dc(register uint8_t & loopvar);

template<int _LOOP, int PAD> __attribute__((always_inline)) inline void _dc_AVR(register uint8_t & loopvar) {
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
template<> __attribute__((always_inline)) inline void _dc< 0>(register uint8_t & loopvar) {}
template<> __attribute__((always_inline)) inline void _dc< 1>(register uint8_t & loopvar) {asm __volatile__("mov r0,r0":::);}
template<> __attribute__((always_inline)) inline void _dc< 2>(register uint8_t & loopvar) {asm __volatile__("rjmp .+0":::);}
template<> __attribute__((always_inline)) inline void _dc< 3>(register uint8_t & loopvar) { _dc<2>(loopvar); _dc<1>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc< 4>(register uint8_t & loopvar) { _dc<2>(loopvar); _dc<2>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc< 5>(register uint8_t & loopvar) { _dc<2>(loopvar); _dc<3>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc< 6>(register uint8_t & loopvar) { _dc<2>(loopvar); _dc<2>(loopvar); _dc<2>(loopvar);}
template<> __attribute__((always_inline)) inline void _dc< 7>(register uint8_t & loopvar) { _dc<4>(loopvar); _dc<3>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc< 8>(register uint8_t & loopvar) { _dc<4>(loopvar); _dc<4>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc< 9>(register uint8_t & loopvar) { _dc<5>(loopvar); _dc<4>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<10>(register uint8_t & loopvar) { _dc<6>(loopvar); _dc<4>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<11>(register uint8_t & loopvar) { _dc<10>(loopvar); _dc<1>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<12>(register uint8_t & loopvar) { _dc<10>(loopvar); _dc<2>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<13>(register uint8_t & loopvar) { _dc<10>(loopvar); _dc<3>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<14>(register uint8_t & loopvar) { _dc<10>(loopvar); _dc<4>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<15>(register uint8_t & loopvar) { _dc<10>(loopvar); _dc<5>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<16>(register uint8_t & loopvar) { _dc<10>(loopvar); _dc<6>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<17>(register uint8_t & loopvar) { _dc<10>(loopvar); _dc<7>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<18>(register uint8_t & loopvar) { _dc<10>(loopvar); _dc<8>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<19>(register uint8_t & loopvar) { _dc<10>(loopvar); _dc<9>(loopvar); }
template<> __attribute__((always_inline)) inline void _dc<20>(register uint8_t & loopvar) { _dc<10>(loopvar); _dc<10>(loopvar); }

#define DINTPIN(T,ADJ,PINADJ) (T-(PINADJ+ADJ)>0) ? _dc<T-(PINADJ+ADJ)>(loopvar) : _dc<0>(loopvar);
#define DINT(T,ADJ) if(AVR_PIN_CYCLES(DATA_PIN)==1) { DINTPIN(T,ADJ,1) } else { DINTPIN(T,ADJ,2); }
#define D1(ADJ) DINT(T1,ADJ)
#define D2(ADJ) DINT(T2,ADJ)
#define D3(ADJ) DINT(T3,ADJ)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base template for clockless controllers.  These controllers have 3 control points in their cycle for each bit.  The first point
// is where the line is raised hi.  The second point is where the line is dropped low for a zero.  The third point is where the
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if (!defined(NO_CORRECTION) || (NO_CORRECTION == 0)) && (FASTLED_ALLOW_INTERRUPTS == 0)
static uint8_t gTimeErrorAccum256ths;
#endif

#define FASTLED_HAS_CLOCKLESS 1

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 10>
class ClocklessController : public CLEDController {
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() {
		FastPin<DATA_PIN>::setOutput();
	}

	virtual uint16_t getMaxRefreshRate() const { return 400; }

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
#if (!defined(NO_CORRECTION) || (NO_CORRECTION == 0)) && (FASTLED_ALLOW_INTERRUPTS == 0)
        uint32_t microsTaken = (uint32_t)nLeds * (uint32_t)CLKS_TO_MICROS(24 * (T1 + T2 + T3));

        // adust for approximate observed actal runtime (as of January 2015)
        // roughly 9.6 cycles per pixel, which is 0.6us/pixel at 16MHz
        // microsTaken += nLeds * 0.6 * CLKS_TO_MICROS(16);
        microsTaken += scale16by8(nLeds,(0.6 * 256) + 1) * CLKS_TO_MICROS(16);

        // if less than 1000us, there is NO timer impact,
        // this is because the ONE interrupt that might come in while interrupts
        // are disabled is queued up, and it will be serviced as soon as
        // interrupts are re-enabled.
        // This actually should technically also account for the runtime of the
        // interrupt handler itself, but we're just not going to worry about that.
        if( microsTaken > 1000) {

            // Since up to one timer tick will be queued, we don't need
            // to adjust the MS_COUNTER for that one.
            microsTaken -= 1000;

            // Now convert microseconds to 256ths of a second, approximately like this:
            // 250ths = (us/4)
            // 256ths = 250ths * (263/256);
            uint16_t x256ths = microsTaken >> 2;
            x256ths += scale16by8(x256ths,7);

            x256ths += gTimeErrorAccum256ths;
            MS_COUNTER += (x256ths >> 8);
            gTimeErrorAccum256ths = x256ths & 0xFF;
        }

#if 0
        // For pixel counts of 30 and under at 16Mhz, no correction is necessary.
        // For pixel counts of 15 and under at 8Mhz, no correction is necessary.
        //
        // This code, below, is smaller, and quicker clock correction, which drifts much
        // more significantly, but is a few bytes smaller.  Presented here for consideration
        // as an alternate on the ATtiny, which can't have more than about 150 pixels MAX
        // anyway, meaning that microsTaken will never be more than about 4,500, which fits in
        // a 16-bit variable.  The difference between /1000 and /1024 only starts showing
        // up in the range of about 100 pixels, so many ATtiny projects won't even
        // see a clock difference due to the approximation there.
		uint16_t microsTaken = (uint32_t)nLeds * (uint32_t)CLKS_TO_MICROS((24) * (T1 + T2 + T3));
        MS_COUNTER += (microsTaken >> 10);
#endif

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
				[b1] "+a" (b1),							\
				[d0] "+r" (d0),							\
				[d1] "+r" (d1),							\
				[d2] "+r" (d2),							\
				[loopvar] "+a" (loopvar),				\
				[scale_base] "+a" (scale_base)			\
				: /* use variables */					\
				[ADV] "r" (advanceBy),					\
				[b0] "a" (b0),							\
				[hi] "r" (hi),							\
				[lo] "r" (lo),							\
				[s0] "r" (s0),					  		\
				[s1] "r" (s1),							\
				[s2] "r" (s2),							\
				[e0] "r" (e0),							\
				[e1] "r" (e1),							\
				[e2] "r" (e2),							\
				[PORT] "M" (FastPin<DATA_PIN>::port()-0x20),		\
				[O0] "M" (RGB_BYTE0(RGB_ORDER)),		\
				[O1] "M" (RGB_BYTE1(RGB_ORDER)),		\
				[O2] "M" (RGB_BYTE2(RGB_ORDER))		\
				: "cc" /* clobber registers */


// Note: the code in the else in HI1/LO1 will be turned into an sts (2 cycle, 2 word) opcode
// 1 cycle, write hi to the port
#define HI1 FASTLED_SLOW_CLOCK_ADJUST if((int)(FastPin<DATA_PIN>::port())-0x20 < 64) { asm __volatile__("out %[PORT], %[hi]" ASM_VARS ); } else { *FastPin<DATA_PIN>::port()=hi; }
// 1 cycle, write lo to the port
#define LO1 if((int)(FastPin<DATA_PIN>::port())-0x20 < 64) { asm __volatile__("out %[PORT], %[lo]" ASM_VARS ); } else { *FastPin<DATA_PIN>::port()=lo; }

// 2 cycles, sbrs on flipping the line to lo if we're pushing out a 0
#define QLO2(B, N) asm __volatile__("sbrs %[" #B "], " #N ASM_VARS ); LO1;
// load a byte from ram into the given var with the given offset
#define LD2(B,O) asm __volatile__("ldd %[" #B "], Z + %[" #O "]\n\t" ASM_VARS ); 
// 4 cycles - load a byte from ram into the scaling scratch space with the given offset, clear the target var, clear carry
#define LDSCL4(B,O) asm __volatile__("ldd %[scale_base], Z + %[" #O "]\n\tclr %[" #B "]\n\tclc\n\t" ASM_VARS ); 

#if (DITHER==1) 
// apply dithering value  before we do anything with scale_base
#define PRESCALE4(D) asm __volatile__("cpse %[scale_base], __zero_reg__\n\t add %[scale_base],%[" #D "]\n\tbrcc L_%=\n\tldi %[scale_base], 0xFF\n\tL_%=:\n\t" ASM_VARS);

// Do the add for the prescale
#define PRESCALEA2(D) asm __volatile__("cpse %[scale_base], __zero_reg__\n\t add %[scale_base],%[" #D "]\n\t" ASM_VARS);

// Do the clamp for the prescale, clear carry when we're done - NOTE: Must ensure carry flag state is preserved!
#define PRESCALEB3(D) asm __volatile__("brcc L_%=\n\tldi %[scale_base], 0xFF\n\tL_%=:\n\tCLC" ASM_VARS);

#else
#define PRESCALE4(D) _dc<4>(loopvar);
#define PRESCALEA2(D) _dc<2>(loopvar);
#define PRESCALEB3(D) _dc<3>(loopvar);
#endif

// 2 cycles - perform one step of the scaling (if a given bit is set in scale, add scale-base to the scratch space)
#define _SCALE02(B, N) "sbrc %[s0], " #N "\n\tadd %[" #B "], %[scale_base]\n\t"
#define _SCALE12(B, N) "sbrc %[s1], " #N "\n\tadd %[" #B "], %[scale_base]\n\t" 
#define _SCALE22(B, N) "sbrc %[s2], " #N "\n\tadd %[" #B "], %[scale_base]\n\t" 
#define SCALE02(B,N) asm __volatile__( _SCALE02(B,N) ASM_VARS );
#define SCALE12(B,N) asm __volatile__( _SCALE12(B,N) ASM_VARS );
#define SCALE22(B,N) asm __volatile__( _SCALE22(B,N) ASM_VARS );

// 1 cycle - rotate right, pulling in from carry
#define _ROR1(B) "ror %[" #B "]\n\t" 
#define ROR1(B) asm __volatile__( _ROR1(B) ASM_VARS);

// 1 cycle, clear the carry bit
#define _CLC1 "clc\n\t" 
#define CLC1 asm __volatile__( _CLC1 ASM_VARS );

// 2 cycles, rortate right, pulling in from carry then clear the carry bit
#define RORCLC2(B) asm __volatile__( _ROR1(B) _CLC1 ASM_VARS );

// 4 cycles, rotate, clear carry, scale next bit
#define RORSC04(B, N) asm __volatile__( _ROR1(B) _CLC1 _SCALE02(B, N) ASM_VARS );
#define RORSC14(B, N) asm __volatile__( _ROR1(B) _CLC1 _SCALE12(B, N) ASM_VARS );
#define RORSC24(B, N) asm __volatile__( _ROR1(B) _CLC1 _SCALE22(B, N) ASM_VARS );

// 4 cycles, scale bit, rotate, clear carry
#define SCROR04(B, N) asm __volatile__( _SCALE02(B,N) _ROR1(B) _CLC1 ASM_VARS ); 
#define SCROR14(B, N) asm __volatile__( _SCALE12(B,N) _ROR1(B) _CLC1 ASM_VARS );
#define SCROR24(B, N) asm __volatile__( _SCALE22(B,N) _ROR1(B) _CLC1 ASM_VARS );

/////////////////////////////////////////////////////////////////////////////////////
// Loop life cycle

// dither adjustment macro - should be kept in sync w/what's in stepDithering
// #define ADJDITHER2(D, E) D = E - D;
#define ADJDITHER2(D, E) asm __volatile__ ("neg %[" #D "]\n\tadd %[" #D "],%[" #E "]\n\t" ASM_VARS); 

// #define xstr(a) str(a)
// #define str(a) #a
// #define ADJDITHER2(D,E) asm __volatile__("subi %[" #D "], " xstr(DUSE) "\n\tand %[" #D "], %[" #E "]\n\t" ASM_VARS);

// define the beginning of the loop
#define LOOP asm __volatile__("1:" ASM_VARS );
// define the end of the loop
#define DONE asm __volatile__("2:" ASM_VARS );

// 2 cycles - increment the data pointer
#define IDATA2 asm __volatile__("add %A[data], %[ADV]\n\tadc %B[data], __zero_reg__\n\t"  ASM_VARS );
#define IDATACLC3 asm __volatile__("add %A[data], %[ADV]\n\tadc %B[data], __zero_reg__\n\t" _CLC1  ASM_VARS );

// 1 cycle mov
#define MOV1(B1, B2) asm __volatile__("mov %[" #B1 "], %[" #B2 "]" ASM_VARS );

// 2 cycles - decrement the counter
#define DCOUNT2 asm __volatile__("sbiw %[count], 1" ASM_VARS );
// 2 cycles - jump to the beginning of the loop
#define JMPLOOP2 asm __volatile__("rjmp 1b" ASM_VARS );
// 2 cycles - jump out of the loop
#define BRLOOP1 asm __volatile__("brne 3\n\trjmp 2f\n\t3:" ASM_VARS );

// 5 cycles 2 sbiw, 3 for the breq/rjmp
#define ENDLOOP5 asm __volatile__("sbiw %[count], 1\n\tbreq L_%=\n\trjmp 1b\n\tL_%=:\n\t" ASM_VARS);

// NOP using the variables, forcing a move
#define DNOP asm __volatile__("mov r0,r0" ASM_VARS);

#define DADVANCE 3
#define DUSE (0xFF - (DADVANCE-1))

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static void /*__attribute__((optimize("O0")))*/  /*__attribute__ ((always_inline))*/  showRGBInternal(PixelController<RGB_ORDER> & pixels)  {
		uint8_t *data = (uint8_t*)pixels.mData;
		data_ptr_t port = FastPin<DATA_PIN>::port();
		data_t mask = FastPin<DATA_PIN>::mask();
		uint8_t scale_base = 0;

		// register uint8_t *end = data + nLeds;
		data_t hi = *port | mask;
		data_t lo = *port & ~mask;
		*port = lo;

		// the byte currently being written out
		uint8_t b0 = 0;
		// the byte currently being worked on to write the next out
		uint8_t b1 = 0;

		// Setup the pixel controller
		pixels.preStepFirstByteDithering();

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

		// load/scale the first byte
#if !defined(LIB8_ATTINY)
		// we have a hardware multiply, can use loadAndScale0
		b0 = pixels.loadAndScale0();
#else
		// no hardware multiply, we have to do our own mul by hand here, lest we incur a
		// function call which will kill all of our register usage/allocations below
		b0 = data[RO(0)];
		{
			LDSCL4(b0,O0) 	PRESCALEA2(d0)
			PRESCALEB3(d0)	SCALE02(b0,0)
			RORSC04(b0,1) 	ROR1(b0) CLC1
			SCROR04(b0,2)		SCALE02(b0,3)
			RORSC04(b0,4) 	ROR1(b0) CLC1
			SCROR04(b0,5) 	SCALE02(b0,6)
			RORSC04(b0,7) 	ROR1(b0) CLC1
		}
#endif

		// #if (FASTLED_ALLOW_INTERRUPTS == 1)
		// TCCR0A |= 0x30;
		// OCR0B = (uint8_t)(TCNT0 + ((WAIT_TIME-INTERRUPT_THRESHOLD)/US_PER_TICK));
		// TIFR0 = 0x04;
		// #endif
		{
			// while(--count)
			{
				// Loop beginning, does some stuff that's outside of the pixel write cycle, namely incrementing d0-2 and masking off
				// by the E values (see the definition )
				DNOP;
				LOOP;

				// ADJDITHER2(d0,e0);
				// ADJDITHER2(d1,e1);
				// ADJDITHER2(d2,e2);
				// NOP;
				// #if (FASTLED_ALLOW_INTERRUPTS == 1)
				// cli();
				// if(TIFR0 & 0x04) {
				// 	sei();
				// 	TCCR0A &= ~0x30;
				// 	return;
				// }
				// hi = *port | mask;
				// lo = *port & ~mask;
				// #endif

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
				// DNOP
				HI1 D1(1) QLO2(b0, 7) LDSCL4(b1,O1) 	D2(4)	LO1	PRESCALEA2(d1)	D3(2) 
				HI1	D1(1) QLO2(b0, 6) PRESCALEB3(d1)	D2(3)	LO1	SCALE12(b1,0)	D3(2)
				HI1 D1(1) QLO2(b0, 5) RORSC14(b1,1) 	D2(4)	LO1 RORCLC2(b1)		D3(2)
				HI1 D1(1) QLO2(b0, 4) SCROR14(b1,2)		D2(4)	LO1 SCALE12(b1,3)	D3(2)
				HI1 D1(1) QLO2(b0, 3) RORSC14(b1,4) 	D2(4)	LO1 RORCLC2(b1) 	D3(2)
				HI1 D1(1) QLO2(b0, 2) SCROR14(b1,5) 	D2(4)	LO1 SCALE12(b1,6)	D3(2)
				HI1 D1(1) QLO2(b0, 1) RORSC14(b1,7) 	D2(4)	LO1 RORCLC2(b1) 	D3(2)
				HI1 D1(1) QLO2(b0, 0) 
				switch(XTRA0) {
					case 4: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
					case 3: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
					case 2: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
					case 1: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
				} 
				ADJDITHER2(d1,e1) D2(2) LO1 MOV1(b0,b1) D3(1)

				HI1 D1(1) QLO2(b0, 7) LDSCL4(b1,O2) 	D2(4)	LO1	PRESCALEA2(d2)	D3(2)
				HI1	D1(1) QLO2(b0, 6) PRESCALEB3(d2)	D2(3)	LO1	SCALE22(b1,0)	D3(2)
				HI1 D1(1) QLO2(b0, 5) RORSC24(b1,1) 	D2(4)	LO1 RORCLC2(b1) 	D3(2)
				HI1 D1(1) QLO2(b0, 4) SCROR24(b1,2)		D2(4)	LO1 SCALE22(b1,3)	D3(2)
				HI1 D1(1) QLO2(b0, 3) RORSC24(b1,4) 	D2(4)	LO1 RORCLC2(b1) 	D3(2)
				HI1 D1(1) QLO2(b0, 2) SCROR24(b1,5) 	D2(4)	LO1 SCALE22(b1,6)	D3(2)
				HI1 D1(1) QLO2(b0, 1) RORSC24(b1,7) 	D2(4)	LO1 RORCLC2(b1) 	D3(2)
				HI1 D1(1) QLO2(b0, 0) 			
				switch(XTRA0) {
					case 4: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
					case 3: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
					case 2: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
					case 1: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
				} 
				IDATACLC3 MOV1(b0,b1) D2(4) LO1 ADJDITHER2(d2,e2) D3(2)

				HI1 D1(1) QLO2(b0, 7) LDSCL4(b1,O0) 	D2(4)	LO1	PRESCALEA2(d0)	D3(2)
				HI1	D1(1) QLO2(b0, 6) PRESCALEB3(d0)	D2(3)	LO1	SCALE02(b1,0)	D3(2)
				HI1 D1(1) QLO2(b0, 5) RORSC04(b1,1) 	D2(4)	LO1 RORCLC2(b1) 	D3(2)
				HI1 D1(1) QLO2(b0, 4) SCROR04(b1,2)		D2(4)	LO1 SCALE02(b1,3)	D3(2)
				HI1 D1(1) QLO2(b0, 3) RORSC04(b1,4) 	D2(4)	LO1 RORCLC2(b1)  	D3(2)
				HI1 D1(1) QLO2(b0, 2) SCROR04(b1,5) 	D2(4)	LO1 SCALE02(b1,6)	D3(2)
				HI1 D1(1) QLO2(b0, 1) RORSC04(b1,7) 	D2(4)	LO1 RORCLC2(b1) 	D3(2)
				HI1 D1(1) QLO2(b0, 0) 	 
				switch(XTRA0) {
					case 4: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
					case 3: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
					case 2: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
					case 1: D2(0) LO1 D3(0) HI1 D1(1) QLO2(b0,0)
				} 
				ADJDITHER2(d0,e0) MOV1(b0,b1) D2(3) LO1 D3(6)
				ENDLOOP5
#else
				// no inline scaling - non-straight RGB ordering -- no longer in line with the actual asm macros above, left for
				// reference only
				HI1	D1(1) QLO2(b0, 7) LD2(b1,O1)	D2(2)	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 6) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 5) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 4) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 3) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 2) 				D2(0)	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 1) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b0, 0) 				D2(0) 	LO1 D3(0)
				HI1	D1(1) QLO2(b1, 7) LD2(b1,O2) 	D2(2)	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 6) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 5) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 4) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 3) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 2) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 1) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 0) IDATA2 		D2(2)	LO1 D3(0)
				HI1	D1(1) QLO2(b1, 7) LD2(b0,O0) 	D2(2)	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 6) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 5) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 4) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 3) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 2) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 1) 				D2(0) 	LO1 D3(0)
				HI1 D1(1) QLO2(b1, 0) 				D2(0) 	LO1 D3(0)
#endif

				// #if (FASTLED_ALLOW_INTERRUPTS == 1)
				// // set the counter mark
				// OCR0B = (uint8_t)(TCNT0 + ((WAIT_TIME-INTERRUPT_THRESHOLD)/US_PER_TICK));
				// TIFR0 = 0x04;
				// sei();
				// #endif
			}
			DONE;
		}

		#if (FASTLED_ALLOW_INTERRUPTS == 1)
		// stop using the clock juggler
		TCCR0A &= ~0x30;
		#endif
	}

#ifdef SUPPORT_ARGB
	virtual void showARGB(struct CARGB *data, int nLeds) {
		// TODO: IMPLEMENTME
	}
#endif
};

#endif

FASTLED_NAMESPACE_END

#endif

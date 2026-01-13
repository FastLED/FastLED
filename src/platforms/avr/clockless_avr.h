#ifndef __INC_CLOCKLESS_AVR_H
#define __INC_CLOCKLESS_AVR_H

#include "controller.h"
#include "lib8tion.h"
#include <avr/interrupt.h> // for cli/se definitions
#include "fl/force_inline.h"
#include "fl/chipsets/led_timing.h"
#include "fl/chipsets/timing_traits.h"
#include "fastled_delay.h"
#include "platforms/shared/clockless_blocking.h"
#include "platforms/is_platform.h"

namespace fl {

#if defined(FL_IS_AVR)

// Scaling macro choice
#ifndef FASTLED_AVR_SCALE
#define FASTLED_AVR_SCALE 1
// whether or not to use dithering
#define DITHER 1
#endif

#if (F_CPU==8000000)
#define FASTLED_SLOW_CLOCK_ADJUST // asm __volatile__ ("mov r0,r0\n\t");
#else
#define FASTLED_SLOW_CLOCK_ADJUST
#endif

#define US_PER_TICK (64 / (F_CPU/1000000))

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Due to the tight timing specifications of WS2811 and friends on AVR, interrupts are disabled by default to keep timings exact.
// However, many WS2811 or WS2812 strips are surprisingly tolerant of jittery timings (such as those caused by interrupts), on the
// condition that the shortest pulse in the specification (representing a 0 bit) is kept under a certain length. After exceeding
// that length it would be interpreted as a 1, causing a glitch. 
// 
// If you set FASTLED_ALLOW_INTERRUPTS to 1, interrupts will only be disabled for a few cycles at a time, when necessary to keep
// this signal pulse short.
// 
// Beware: even with FASTLED_ALLOW_INTERRUPTS enabled, you must ensure that your interrupt handlers are *very* fast. If they take
// longer than 5µs, which is 80 clock cycles on a 16MHz AVR, the strip might latch partway through rendering, and you will see big
// glitches.
// 
// Remember to account for the interrupt overhead when writing your ISR. This accounts for at least 10 cycles, often 20+.
//
// If you are using multiple timers with interrupts, you can set them out of phase so they only fire one at a time. 
//
// TODO: it would be possible to support longer interrupts providing that they only fire during the "on" pulse - holding the
//       signal high indefinitely will never latch, although it would affect the framerate. Maybe the subject of a future PR…
//
// See https://wp.josh.com/2014/05/13/ws2812-neopixels-are-not-so-finicky-once-you-get-to-know-them/ for more information on the
// tolerances.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//#define FASTLED_ALLOW_INTERRUPTS 1


// Variations on the functions in delay.h - w/a loop var passed in to preserve registers across calls by the optimizer/compiler
template<int CYCLES> inline void _dc(FASTLED_REGISTER uint8_t & loopvar);

// Delay function that consumes a specific number of CPU cycles while preserving carry flag state
// _LOOP: Number of 6-cycle iterations to perform (each loop iteration = 6 cycles: MOV + DEC + BRNE = 1 + 1 + 2 cycles, loop overhead = 2)
// PAD: Additional cycles to add (0-5) to fine-tune total delay
// The function branches based on carry flag state and restores it after the delay
template<int _LOOP, int PAD> FASTLED_FORCE_INLINE void _dc_AVR(FASTLED_REGISTER uint8_t & loopvar) {
	_dc<PAD>(loopvar);  // First handle the padding cycles (remainder when CYCLES % 6)

	// The convolution in here is to ensure that the state of the carry flag coming into the delay loop is preserved
	// This is critical because many scaling operations depend on the carry flag state
	// Use a const variable to allow "any register" constraint
	const uint8_t loop_count = _LOOP;

	// Assembly breakdown:
	// - BRCS: Branch if carry set (1 cycle if not taken, 2 if taken)
	// - If carry clear: Use first loop path (L_*:), which doesn't affect carry
	// - If carry set: Use second loop path (LL_*:), then BSET 0 to restore carry flag
	// - Each loop iteration: MOV=1 cycle, DEC=1 cycle, BRNE=2 cycles (when taken), 1 cycle (when not taken)
	// - Total per iteration: 4 cycles when looping, 3 cycles on exit
	asm __volatile__ (  "BRCS L_PC%=\n\t"  // Branch if carry set (preserves two code paths)
						"        MOV %[loopvar], %[loop_count]\n\tL_%=: DEC %[loopvar]\n\t BRNE L_%=\n\tBREQ L_DONE%=\n\t"  // Carry-clear path
						"L_PC%=: MOV %[loopvar], %[loop_count]\n\tLL_%=: DEC %[loopvar]\n\t BRNE LL_%=\n\tBSET 0\n\t"  // Carry-set path, restores carry at end
						"L_DONE%=:\n\t"
						:
							[loopvar] "+r" (loopvar) : [loop_count] "r" (loop_count) : );
}

// Primary template: splits requested cycles into 6-cycle chunks (handled by loop) plus remainder (handled by specializations)
template<int CYCLES> FASTLED_FORCE_INLINE void _dc(FASTLED_REGISTER uint8_t & loopvar) {
	_dc_AVR<CYCLES/6,CYCLES%6>(loopvar);
}

// ===== CYCLE DELAY SPECIALIZATIONS =====
// These template specializations provide exact cycle delays from -6 to 20 cycles
// Negative values and zero simply do nothing (used when timing calculations result in no delay needed)

// No-op for negative cycle requests (can occur when other operations already consumed the required time)
template<> FASTLED_FORCE_INLINE void _dc<-6>(FASTLED_REGISTER uint8_t & ) {}
template<> FASTLED_FORCE_INLINE void _dc<-5>(FASTLED_REGISTER uint8_t & ) {}
template<> FASTLED_FORCE_INLINE void _dc<-4>(FASTLED_REGISTER uint8_t & ) {}
template<> FASTLED_FORCE_INLINE void _dc<-3>(FASTLED_REGISTER uint8_t & ) {}
template<> FASTLED_FORCE_INLINE void _dc<-2>(FASTLED_REGISTER uint8_t & ) {}
template<> FASTLED_FORCE_INLINE void _dc<-1>(FASTLED_REGISTER uint8_t & ) {}
template<> FASTLED_FORCE_INLINE void _dc< 0>(FASTLED_REGISTER uint8_t & ) {}

// 1 cycle: MOV r0,r0 is a no-op that consumes exactly 1 cycle
template<> FASTLED_FORCE_INLINE void _dc< 1>(FASTLED_REGISTER uint8_t & ) {asm __volatile__("mov r0,r0":::);}

// 2 cycles: Different implementations for different AVR variants
#if defined(__LGT8F__)
// LGT8F: Use two 1-cycle operations (MOV instructions have different timing on LGT8F)
template<> FASTLED_FORCE_INLINE void _dc< 2>(FASTLED_REGISTER uint8_t & loopvar) { _dc<1>(loopvar); _dc<1>(loopvar); }
#else
// Standard AVR: RJMP .+0 (relative jump to next instruction) = 2 cycles
template<> FASTLED_FORCE_INLINE void _dc< 2>(FASTLED_REGISTER uint8_t & ) {asm __volatile__("rjmp .+0":::);}
#endif

// 3-20 cycles: Build up delays by combining smaller delays
// This approach ensures accurate timing through template metaprogramming
template<> FASTLED_FORCE_INLINE void _dc< 3>(FASTLED_REGISTER uint8_t & loopvar) { _dc<2>(loopvar); _dc<1>(loopvar); }  // 2+1
template<> FASTLED_FORCE_INLINE void _dc< 4>(FASTLED_REGISTER uint8_t & loopvar) { _dc<2>(loopvar); _dc<2>(loopvar); }  // 2+2
template<> FASTLED_FORCE_INLINE void _dc< 5>(FASTLED_REGISTER uint8_t & loopvar) { _dc<2>(loopvar); _dc<3>(loopvar); }  // 2+3
template<> FASTLED_FORCE_INLINE void _dc< 6>(FASTLED_REGISTER uint8_t & loopvar) { _dc<2>(loopvar); _dc<2>(loopvar); _dc<2>(loopvar);}  // 2+2+2
template<> FASTLED_FORCE_INLINE void _dc< 7>(FASTLED_REGISTER uint8_t & loopvar) { _dc<4>(loopvar); _dc<3>(loopvar); }  // 4+3
template<> FASTLED_FORCE_INLINE void _dc< 8>(FASTLED_REGISTER uint8_t & loopvar) { _dc<4>(loopvar); _dc<4>(loopvar); }  // 4+4
template<> FASTLED_FORCE_INLINE void _dc< 9>(FASTLED_REGISTER uint8_t & loopvar) { _dc<5>(loopvar); _dc<4>(loopvar); }  // 5+4
template<> FASTLED_FORCE_INLINE void _dc<10>(FASTLED_REGISTER uint8_t & loopvar) { _dc<6>(loopvar); _dc<4>(loopvar); }  // 6+4
template<> FASTLED_FORCE_INLINE void _dc<11>(FASTLED_REGISTER uint8_t & loopvar) { _dc<10>(loopvar); _dc<1>(loopvar); }  // 10+1
template<> FASTLED_FORCE_INLINE void _dc<12>(FASTLED_REGISTER uint8_t & loopvar) { _dc<10>(loopvar); _dc<2>(loopvar); }  // 10+2
template<> FASTLED_FORCE_INLINE void _dc<13>(FASTLED_REGISTER uint8_t & loopvar) { _dc<10>(loopvar); _dc<3>(loopvar); }  // 10+3
template<> FASTLED_FORCE_INLINE void _dc<14>(FASTLED_REGISTER uint8_t & loopvar) { _dc<10>(loopvar); _dc<4>(loopvar); }  // 10+4
template<> FASTLED_FORCE_INLINE void _dc<15>(FASTLED_REGISTER uint8_t & loopvar) { _dc<10>(loopvar); _dc<5>(loopvar); }  // 10+5
template<> FASTLED_FORCE_INLINE void _dc<16>(FASTLED_REGISTER uint8_t & loopvar) { _dc<10>(loopvar); _dc<6>(loopvar); }  // 10+6
template<> FASTLED_FORCE_INLINE void _dc<17>(FASTLED_REGISTER uint8_t & loopvar) { _dc<10>(loopvar); _dc<7>(loopvar); }  // 10+7
template<> FASTLED_FORCE_INLINE void _dc<18>(FASTLED_REGISTER uint8_t & loopvar) { _dc<10>(loopvar); _dc<8>(loopvar); }  // 10+8
template<> FASTLED_FORCE_INLINE void _dc<19>(FASTLED_REGISTER uint8_t & loopvar) { _dc<10>(loopvar); _dc<9>(loopvar); }  // 10+9
template<> FASTLED_FORCE_INLINE void _dc<20>(FASTLED_REGISTER uint8_t & loopvar) { _dc<10>(loopvar); _dc<10>(loopvar); }  // 10+10

// ===== INTERRUPT AND TIMING ADJUSTMENT =====

#if (FASTLED_ALLOW_INTERRUPTS == 1)
// If interrupts are enabled, HI1 actually takes 2 clocks due to cli().
// To keep the timings exact, D3 (which precedes it) must be 1 clock less.
// The same adjustment must be made for D2, due the corresponding sei().
#define D_INT_ADJ 1
#else
#define D_INT_ADJ 0
#endif

// DINTPIN: Delay for timing periods, accounting for pin access costs
// T: Target delay in cycles
// ADJ: Additional adjustment cycles (from other operations)
// PINADJ: Pin access overhead (1 cycle for fast OUT instruction, 2 cycles for STS instruction)
#define DINTPIN(T,ADJ,PINADJ) (T-(PINADJ+ADJ)>0) ? _dc<T-(PINADJ+ADJ)>(loopvar) : _dc<0>(loopvar);

// DINT: Choose delay based on whether pin uses fast (OUT) or slow (STS) access
// AVR_PIN_CYCLES(DATA_PIN)==1 means port is in lower I/O space, uses 1-cycle OUT instruction
// Otherwise uses 2-cycle STS (store to SRAM) instruction
#define DINT(T,ADJ) if(AVR_PIN_CYCLES(DATA_PIN)==1) { DINTPIN(T,ADJ,1) } else { DINTPIN(T,ADJ,2); }

// _D1, _D2, _D3: Delay macros for the three timing periods in the bit protocol
// T1: HIGH pulse duration before potential LOW (for '0' bit)
// T2: Time from start to when '1' bit drops LOW
// T3: Total bit duration
#define _D1(ADJ) DINT(T1,ADJ)
#define _D2(ADJ) DINT(T2,ADJ+D_INT_ADJ)
#define _D3(ADJ) DINT(T3,ADJ+D_INT_ADJ)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base template for clockless controllers.  These controllers have 3 control points in their cycle for each bit.  The first point
// is where the line is raised hi.  The second point is where the line is dropped low for a zero.  The third point is where the
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
// WS2812/WS2811 BIT PROTOCOL TIMING:
// Each bit transmission has three critical timing points:
//   1. Line goes HIGH (start of bit)
//   2. For '0' bit: Line goes LOW early (typically ~0.4µs from start = T1)
//   3. For '1' bit: Line stays HIGH longer, then goes LOW (typically ~0.8µs from start = T2)
//   4. Bit period ends at T3 (typically ~1.25µs total)
//
// Visual representation:
//   '0' bit:  HIGH |-------|______________________  (T1=short, T2-T1=long, T3=total)
//   '1' bit:  HIGH |------------------|__________  (T1=short, T2-T1=medium, T3=total)
//
// The QLO2 macro tests each bit and drops the line LOW at T1 if bit is '0', or lets it continue until T2 if bit is '1'
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if ((FASTLED_ALLOW_INTERRUPTS == 0) && defined(NO_CORRECTION) && (NO_CORRECTION == 1) && !(defined(NO_CLOCK_CORRECTION)))
// we hit this if you were trying to turn off clock correction without also trying to enable interrupts.
#	pragma message "In older versions of FastLED defining NO_CORRECTION 1 would mistakenly turn off color correction as well as clock correction."
#	pragma message "define NO_CLOCK_CORRECTION 1 to fix this warning."
#	define NO_CLOCK_CORRECTION 1
#endif


inline uint8_t& avr_time_accumulator() {
	static uint8_t gTimeErrorAccum256ths = 0;
	return gTimeErrorAccum256ths;
}

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

template <uint8_t DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 10>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
	// Extract timing values from struct and convert from nanoseconds to clock cycles
	// Formula: cycles = (nanoseconds * CPU_MHz + 500) / 1000
	// The +500 provides rounding to nearest integer
	static constexpr uint32_t T1 = (TIMING::T1 * (F_CPU / 1000000UL) + 500) / 1000;
	static constexpr uint32_t T2 = (TIMING::T2 * (F_CPU / 1000000UL) + 500) / 1000;
	static constexpr uint32_t T3 = (TIMING::T3 * (F_CPU / 1000000UL) + 500) / 1000;

	static_assert(T1 >= 2 && T2 >= 2 && T3 >= 3, "Not enough cycles - use a higher clock speed");

	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

	CMinWait<WAIT_TIME> mWait;

public:
	virtual void init() {
		FastPin<DATA_PIN>::setOutput();
	}

	virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {

		mWait.wait();
#if (!defined(FASTLED_ALLOW_INTERRUPTS) || FASTLED_ALLOW_INTERRUPTS == 0)
		cli();
#endif

		if(pixels.mLen > 0) {
			showRGBInternal(pixels);
		}

		// Adjust the timer
#if (!defined(NO_CLOCK_CORRECTION) || (NO_CLOCK_CORRECTION == 0)) && (FASTLED_ALLOW_INTERRUPTS == 0)
		uint32_t microsTaken = pixels.size();
		microsTaken *= CLKS_TO_MICROS(24 * (T1 + T2 + T3));

        // adust for approximate observed actal runtime (as of January 2015)
        // roughly 9.6 cycles per pixel, which is 0.6us/pixel at 16MHz
        // microsTaken += nLeds * 0.6 * CLKS_TO_MICROS(16);
        microsTaken += scale16by8(pixels.size(),(0.6 * 256) + 1) * CLKS_TO_MICROS(16);

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

			uint8_t& accum = avr_time_accumulator();

            x256ths += accum;
            MS_COUNTER += (x256ths >> 8);
            accum = x256ths & 0xFF;
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
		uint32_t microsTaken = nLeds;
		microsTaken *= CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		//uint16_t microsTaken = (uint32_t)nLeds * (uint32_t)CLKS_TO_MICROS((24) * (T1 + T2 + T3));
        MS_COUNTER += static_cast<uint16_t>(microsTaken >> 10);
#endif

#endif

#if (!defined(FASTLED_ALLOW_INTERRUPTS) || FASTLED_ALLOW_INTERRUPTS == 0)
		sei();
#endif
		mWait.mark();
	}
#define USE_ASM_MACROS


#if defined(__AVR_ATmega4809__)
// Not used - place holder so existing ASM_VARS macro can remain the same
#define ASM_VAR_PORT "r" (*FastPin<DATA_PIN>::port())

#elif defined(__AVR_ATtinyxy7__) || defined(__AVR_ATtinyxy6__) || defined(__AVR_ATtinyxy4__) || defined(__AVR_ATtinyxy2__)
// Probably unused - place holder so existing ASM_VARS macro can remain the same
#define ASM_VAR_PORT "r" (((PORT_t*)FastPin<DATA_PIN>::port())->OUT)

#else
// existing ASM_VARS macro for other AVR platforms
#define ASM_VAR_PORT "M" (FastPin<DATA_PIN>::port() - 0x20)

#endif // ASM_VAR_PORT


// ===== ASM VARIABLE CONSTRAINTS =====
// The variables that our various asm statements use.  The same block of variables needs to be declared for
// all the asm blocks because GCC is pretty stupid and it would clobber variables happily or optimize code away too aggressively

// REGISTER ALLOCATION STRATEGY:
// The inline assembly uses specific register constraints to ensure correct code generation:
// - "+x" (count): X register (r26:r27) for 16-bit pixel counter, read/write
// - "+z" (data): Z register (r30:r31) for pointer to pixel data, read/write
// - "+a" or "=&a" (b0, b1, loopvar, scale_base): Any register, read/write
//    The "=&a" is an "early clobber" constraint that prevents GCC from reusing input registers for output
// - "+r" (d0-d2): Any register for dither values, read/write
// - "r" (hi, lo, s0-s2, e0-e2, ADV): Any register for read-only values
// - "M" (PORT, O0-O2): Immediate integer constants (for I/O port addresses and byte offsets)

// The existing code led to the generation of spurious move operations in the asm statements
// that threw off timing when optimizing for debugging with -Og.
// With the constraint "[b1] "=&a" (b1)", this is not any longer the case.
// The new code appears to work in other circumstances as well, but it will only
// be activated when FASTLED_AVR_LEGACY_CODE == 0.
// If FASTLED_AVR_LEGACY_CODE is not defined, then it will be set to 1 only
// if the -Os (__OPTIMIZE_SIZE__) option has not been chosen.

#ifndef FASTLED_AVR_LEGACY_CODE
#ifdef __OPTIMIZE_SIZE__
#define FASTLED_AVR_LEGACY_CODE 1
#else
#define FASTLED_AVR_LEGACY_CODE 0
#endif
#endif
#if FASTLED_AVR_LEGACY_CODE
// LEGACY MODE: Uses "+a" constraint for b1 (may cause spurious moves with -Og optimization)
#define ASM_VARS : /* write variables (output operands that are read and modified) */	\
				[count] "+x" (count),			/* Pixel counter (X register pair) */		\
				[data] "+z" (data),				/* Pointer to pixel data (Z register pair) */		\
				[b1] "+a" (b1),					/* Next byte being prepared (any register) */		\
				[d0] "+r" (d0),					/* Dither value for byte 0 */	\
				[d1] "+r" (d1),					/* Dither value for byte 1 */	\
				[d2] "+r" (d2),					/* Dither value for byte 2 */	\
				[loopvar] "+a" (loopvar),		/* Loop variable for delays */	\
				[scale_base] "+a" (scale_base)	/* Scaling accumulator */	\
				: /* read-only variables (input operands) */	\
				[ADV] "r" (advanceBy),			/* Data pointer advance amount (signed) */	\
				[b0] "a" (b0),					/* Current byte being output */	\
				[hi] "r" (hi),					/* Port value for HIGH signal */	\
				[lo] "r" (lo),					/* Port value for LOW signal */	\
				[s0] "r" (s0),					/* Scale value for byte 0 (color correction) */	\
				[s1] "r" (s1),					/* Scale value for byte 1 (color correction) */	\
				[s2] "r" (s2),					/* Scale value for byte 2 (color correction) */	\
				[e0] "r" (e0),					/* Dither step value for byte 0 */	\
				[e1] "r" (e1),					/* Dither step value for byte 1 */	\
				[e2] "r" (e2),					/* Dither step value for byte 2 */	\
				[PORT] ASM_VAR_PORT,			/* I/O port address */	\
				[O0] "M" (RGB_BYTE0(RGB_ORDER)),	/* Offset to first color byte in pixel */	\
				[O1] "M" (RGB_BYTE1(RGB_ORDER)),	/* Offset to second color byte in pixel */	\
				[O2] "M" (RGB_BYTE2(RGB_ORDER))		/* Offset to third color byte in pixel */	\
				: "cc" /* clobber condition codes (carry flag modified) */
#else
// EXPERIMENTAL MODE: Uses "=&a" constraint for b1 (early clobber prevents spurious moves)
#warning "Experimental code to support debugging! Define FASTLED_AVR_LEGACY_CODE as 1 to force using legacy code."
#define ASM_VARS : /* write variables (output operands that are read and modified) */	\
				[count] "+x" (count),			/* Pixel counter (X register pair) */		\
				[data] "+z" (data),				/* Pointer to pixel data (Z register pair) */		\
				[b1] "=&a" (b1),				/* Next byte being prepared (early clobber) */	\
	                        [d0] "+r" (d0),					/* Dither value for byte 0 */	\
				[d1] "+r" (d1),					/* Dither value for byte 1 */	\
				[d2] "+r" (d2),					/* Dither value for byte 2 */	\
				[loopvar] "+a" (loopvar),		/* Loop variable for delays */	\
				[scale_base] "+a" (scale_base)	/* Scaling accumulator */	\
				: /* read-only variables (input operands) */	\
				[ADV] "r" (advanceBy),			/* Data pointer advance amount (signed) */	\
				[b0] "a" (b0),					/* Current byte being output */	\
				[hi] "r" (hi),					/* Port value for HIGH signal */	\
				[lo] "r" (lo),					/* Port value for LOW signal */	\
				[s0] "r" (s0),					/* Scale value for byte 0 (color correction) */	\
				[s1] "r" (s1),					/* Scale value for byte 1 (color correction) */	\
				[s2] "r" (s2),					/* Scale value for byte 2 (color correction) */	\
				[e0] "r" (e0),					/* Dither step value for byte 0 */	\
				[e1] "r" (e1),					/* Dither step value for byte 1 */	\
				[e2] "r" (e2),					/* Dither step value for byte 2 */	\
				[PORT] ASM_VAR_PORT,			/* I/O port address */	\
				[O0] "M" (RGB_BYTE0(RGB_ORDER)),	/* Offset to first color byte in pixel */	\
				[O1] "M" (RGB_BYTE1(RGB_ORDER)),	/* Offset to second color byte in pixel */	\
				[O2] "M" (RGB_BYTE2(RGB_ORDER))		/* Offset to third color byte in pixel */	\
				: "cc" /* clobber condition codes (carry flag modified) */
#endif
	
// ===== CORE BIT-BANGING MACROS =====

#if (FASTLED_ALLOW_INTERRUPTS == 1)
#define HI1CLI cli()  // Disable interrupts when going HIGH (keeps critical timing tight)
#define QLO2SEI sei()  // Re-enable interrupts after potential LOW (after short pulse is secure)
#else
#define HI1CLI  // No interrupt control when interrupts are globally disabled
#define QLO2SEI
#endif

#if defined(__AVR_ATmega4809__)

// ATmega4809: Direct memory access for newer AVR architecture
// 1 cycle, write hi to the port (sets data line HIGH)
#define HI1 HI1CLI; do {*FastPin<DATA_PIN>::port()=hi;} while(0);
// 1 cycle, write lo to the port (sets data line LOW)
#define LO1 do {*FastPin<DATA_PIN>::port()=lo;} while(0);

#else

// Standard AVR: Use fast OUT instruction for low I/O ports, fall back to STS for high ports
// Port addresses 0x00-0x3F (physical 0x20-0x5F) can use 1-cycle OUT instruction
// Higher addresses require 2-cycle STS instruction

// 1 cycle, write hi to the port (sets data line HIGH, starts the bit transmission)
// FASTLED_SLOW_CLOCK_ADJUST: Extra delay for 8MHz CPUs if needed
// HI1CLI: Disables interrupts at this critical timing point (if interrupt mode enabled)
#define HI1 FASTLED_SLOW_CLOCK_ADJUST HI1CLI; if((int)(FastPin<DATA_PIN>::port())-0x20 < 64) { asm __volatile__("out %[PORT], %[hi]" ASM_VARS ); } else { *FastPin<DATA_PIN>::port()=hi; }

// 1 cycle, write lo to the port (sets data line LOW, ends the bit HIGH period)
#define LO1 if((int)(FastPin<DATA_PIN>::port())-0x20 < 64) { asm __volatile__("out %[PORT], %[lo]" ASM_VARS ); } else { *FastPin<DATA_PIN>::port()=lo; }

#endif

// QLO2: "Quick LOW on bit test" - Core of the bit encoding
// 2 cycles total: SBRS (Skip if Bit in Register is Set) + conditional LO1
// - SBRS tests bit N in byte B (1 cycle)
// - If bit is 0: Execute LO1 (drop line LOW early = '0' bit encoding)
// - If bit is 1: Skip LO1 (keep line HIGH longer = '1' bit encoding)
// - QLO2SEI: Re-enable interrupts after the critical short pulse timing
// This is the heart of the WS2812 protocol implementation
#define QLO2(B, N) asm __volatile__("sbrs %[" #B "], " #N ASM_VARS ); LO1; QLO2SEI;
// ===== DATA LOADING MACROS =====

// LD2: Load a byte from pixel data memory
// 2 cycles - LDD (Load with Displacement) from Z pointer + offset O into byte B
// Z pointer (r30:r31) points to current pixel data
#define LD2(B,O) asm __volatile__("ldd %[" #B "], Z + %[" #O "]\n\t" ASM_VARS );

// LDSCL4: Load byte and initialize for scaling operation
// 4 cycles total:
// 1. LDD: Load pixel byte from Z+offset into scale_base (2 cycles)
// 2. CLR: Clear target byte B to prepare for accumulation (1 cycle)
// 3. CLC: Clear carry flag for upcoming add operations (1 cycle)
// This prepares for the 8-bit software multiply used for color scaling
#define LDSCL4(B,O) asm __volatile__("ldd %[scale_base], Z + %[" #O "]\n\tclr %[" #B "]\n\tclc\n\t" ASM_VARS );

// ===== DITHERING AND PRESCALING MACROS =====
// Dithering adds temporal noise to reduce color banding when brightness is low
// The dither value changes per pixel to distribute quantization error over time

#if (DITHER==1)
// PRESCALE4: Apply dither value to pixel byte before scaling
// 4 cycles: Add dither value D to scale_base (the raw pixel byte), clamping to 255 on overflow
// 1. CPSE: Compare scale_base with zero, skip ADD if zero (prevents dithering black pixels) - 1 cycle
// 2. ADD: Add dither value to scale_base - 1 cycle
// 3. BRCC/SER: If carry (overflow >255), set scale_base to 255 (saturate) - 2 cycles
#define PRESCALE4(D) asm __volatile__("cpse %[scale_base], __zero_reg__\n\t add %[scale_base],%[" #D "]\n\tbrcc L_%=\n\tser %[scale_base]\n\tL_%=:\n\t" ASM_VARS);

// PRESCALEA2: First part of split prescale (just the add, for timing flexibility)
// 2 cycles: CPSE + ADD (overflow handling comes later in PRESCALEB4)
#define PRESCALEA2(D) asm __volatile__("cpse %[scale_base], __zero_reg__\n\t add %[scale_base],%[" #D "]\n\t" ASM_VARS);

// PRESCALEB4: Second part of split prescale + dither preparation
// 4 cycles:
// 1-2. BRCC/SER: Clamp overflow from previous add to 255
// 3. NEG: Negate dither value D for later adjustment (D = -D)
// 4. CLC: Clear carry flag for upcoming operations
// NOTE: The negation prepares D for the dither stepping algorithm
#define PRESCALEB4(D) asm __volatile__("brcc L_%=\n\tser %[scale_base]\n\tL_%=:\n\tneg %[" #D "]\n\tCLC" ASM_VARS);

// PSBIDATA4: Prescale clamp + data pointer increment (used for middle byte of RGB triple)
// 4 cycles:
// 1-2. BRCC/SER: Clamp prescale overflow
// 3-4. ADD/ADC: Increment data pointer by advanceBy (handles RGB byte stride)
// Since data pointer can't wrap 64K, this also effectively clears carry
#define PSBIDATA4(D) asm __volatile__("brcc L_%=\n\tser %[scale_base]\n\tL_%=:\n\tadd %A[data], %A[ADV]\n\tadc %B[data], %B[ADV]\n\t" ASM_VARS);

#else
// When dithering disabled, replace with equivalent timing delays
#define PRESCALE4(D) _dc<4>(loopvar);
#define PRESCALEA2(D) _dc<2>(loopvar);
#define PRESCALEB4(D) _dc<4>(loopvar);
// Still need to increment data pointer even without dithering
#define PSBIDATA4(D) asm __volatile__( "add %A[data], %A[ADV]\n\tadc %B[data], %B[ADV]\n\trjmp .+0\n\t" ASM_VARS );
#endif

// ===== SOFTWARE MULTIPLY / SCALING MACROS =====
// AVR lacks hardware multiply, so we implement 8x8 bit multiply through shift-and-add
// Algorithm: For each bit N in scale factor s, if bit is set, add (scale_base << N) to result
// The result is built up in byte B through 8 iterations, rotating right to position each contribution
//
// Example: 200 * 0.5 (scale=128=0b10000000)
//   Only bit 7 is set, so we add scale_base once and rotate 8 times → result ≈ 100

// _SCALE*: String macros for inline assembly combining (2 cycles each)
// SBRC: Skip next instruction if Bit in Register is Clear (1 cycle if skip, 2 if execute)
// ADD: Add scale_base to accumulator byte B if bit N in scale factor s0/s1/s2 is set (1 cycle)
#define _SCALE02(B, N) "sbrc %[s0], " #N "\n\tadd %[" #B "], %[scale_base]\n\t"  // Scale using s0 (byte 0 scale factor)
#define _SCALE12(B, N) "sbrc %[s1], " #N "\n\tadd %[" #B "], %[scale_base]\n\t"  // Scale using s1 (byte 1 scale factor)
#define _SCALE22(B, N) "sbrc %[s2], " #N "\n\tadd %[" #B "], %[scale_base]\n\t"  // Scale using s2 (byte 2 scale factor)

// Standalone scale operations (rarely used alone)
#define SCALE02(B,N) asm __volatile__( _SCALE02(B,N) ASM_VARS );
#define SCALE12(B,N) asm __volatile__( _SCALE12(B,N) ASM_VARS );
#define SCALE22(B,N) asm __volatile__( _SCALE22(B,N) ASM_VARS );

// _ROR1: Rotate right through carry (1 cycle)
// Shifts byte right by 1 bit, pulling carry flag into bit 7, pushing bit 0 into carry
// This positions the accumulated result correctly for the next scaling iteration
#define _ROR1(B) "ror %[" #B "]\n\t"
#define ROR1(B) asm __volatile__( _ROR1(B) ASM_VARS);

// _CLC1: Clear carry flag (1 cycle)
// Necessary between scaling operations to prevent carry contamination
#define _CLC1 "clc\n\t"
#define CLC1 asm __volatile__( _CLC1 ASM_VARS );

// RORCLC2: Rotate right, then clear carry (2 cycles)
// Used between scaling iterations
#define RORCLC2(B) asm __volatile__( _ROR1(B) _CLC1 ASM_VARS );

// RORSC*: Rotate, clear carry, then scale next bit (4 cycles)
// Combined operations for efficiency: ROR (1) + CLC (1) + SCALE (2)
#define RORSC04(B, N) asm __volatile__( _ROR1(B) _CLC1 _SCALE02(B, N) ASM_VARS );
#define RORSC14(B, N) asm __volatile__( _ROR1(B) _CLC1 _SCALE12(B, N) ASM_VARS );
#define RORSC24(B, N) asm __volatile__( _ROR1(B) _CLC1 _SCALE22(B, N) ASM_VARS );

// SCROR*: Scale bit, rotate, clear carry (4 cycles)
// Alternative ordering: SCALE (2) + ROR (1) + CLC (1)
// Choice between RORSC* and SCROR* depends on where we need timing flexibility in the bit output loop
#define SCROR04(B, N) asm __volatile__( _SCALE02(B,N) _ROR1(B) _CLC1 ASM_VARS );
#define SCROR14(B, N) asm __volatile__( _SCALE12(B,N) _ROR1(B) _CLC1 ASM_VARS );
#define SCROR24(B, N) asm __volatile__( _SCALE22(B,N) _ROR1(B) _CLC1 ASM_VARS );

/////////////////////////////////////////////////////////////////////////////////////
// ===== LOOP LIFECYCLE AND UTILITY MACROS =====

// ===== DITHER STEPPING MACROS =====
// After each pixel, dither values are adjusted by: D = E - D
// This creates a triangular dither pattern that distributes quantization error

#define _NEGD1(D) "neg %[" #D "]\n\t"  // Negate D (1 cycle): D = -D
#define _ADJD1(D,E) "add %[" #D "], %[" #E "]\n\t"  // Add E to D (1 cycle): D = D + E

// ADJDITHER2: Complete dither adjustment (2 cycles)
// Computes: D = E - D (via D = -D + E)
// This advances the dither pattern to the next pixel
#define ADJDITHER2(D, E) asm __volatile__ ( _NEGD1(D) _ADJD1(D, E) ASM_VARS);

// ADDDE1: Just add E to D (1 cycle) - used when D was already negated
#define ADDDE1(D, E) asm __volatile__ ( _ADJD1(D, E) ASM_VARS );

// ===== LOOP CONTROL MACROS =====

// LOOP: Define loop start label (0 cycles, just a label)
// Label "1:" is the target for backward jumps
#define LOOP asm __volatile__("1:" ASM_VARS );

// DONE: Define loop end label (0 cycles, just a label)
// Label "2:" is the target for forward jumps when loop exits
#define DONE asm __volatile__("2:" ASM_VARS );

// ===== DATA POINTER MANIPULATION =====

// IDATA2: Increment data pointer by advanceBy (2 cycles)
// ADD/ADC handles 16-bit pointer arithmetic with carry
#define IDATA2 asm __volatile__("add %A[data], %A[ADV]\n\tadc %B[data], %B[ADV]\n\t"  ASM_VARS );

// IDATACLC3: Increment data pointer and clear carry (3 cycles)
#define IDATACLC3 asm __volatile__("add %A[data], %A[ADV]\n\tadc %B[data], %B[ADV]\n\t" _CLC1  ASM_VARS );

// ===== MOVE AND SCALE FIXUP MACROS =====

#define _MOV1(B1, B2) "mov %[" #B1 "], %[" #B2 "]\n\t"  // 1 cycle move
#define MOV1(B1, B2) asm __volatile__( _MOV1(B1,B2) ASM_VARS );

// _MOV_FIX*: Move with optional scale8 correction (3 cycles)
// FASTLED_SCALE8_FIXED handles the scale8((x+1), 255) issue where scale8(255,255) should return 255 not 254
// When s==255 (incremented to 0 via s++), use scale_base directly instead of scaled result
#if (FASTLED_SCALE8_FIXED == 1)
// MOV scale_base to B1, then CPSE (compare skip if equal) - if s0/s1/s2 is zero, skip the MOV of B2
// This gives scale_base (255) when scale factor is 255, or B2 (scaled result) otherwise
#define _MOV_FIX03(B1, B2) "mov %[" #B1 "], %[scale_base]\n\tcpse %[s0], __zero_reg__\n\t" _MOV1(B1, B2)
#define _MOV_FIX13(B1, B2) "mov %[" #B1 "], %[scale_base]\n\tcpse %[s1], __zero_reg__\n\t" _MOV1(B1, B2)
#define _MOV_FIX23(B1, B2) "mov %[" #B1 "], %[scale_base]\n\tcpse %[s2], __zero_reg__\n\t" _MOV1(B1, B2)
#else
// Without scale fix, just do regular move plus 2-cycle NOP to maintain timing
#define _MOV_FIX03(B1, B2) _MOV1(B1, B2) "rjmp .+0\n\t"
#define _MOV_FIX13(B1, B2) _MOV1(B1, B2) "rjmp .+0\n\t"
#define _MOV_FIX23(B1, B2) _MOV1(B1, B2) "rjmp .+0\n\t"
#endif

// MOV_* macros: Combine move with dither adjustment (4 cycles)
// These are used at the end of processing each byte to prepare for the next
#define MOV_NEGD04(B1, B2, D) asm __volatile( _MOV_FIX03(B1, B2) _NEGD1(D) ASM_VARS );  // Move + negate dither
#define MOV_ADDDE04(B1, B2, D, E) asm __volatile( _MOV_FIX03(B1, B2) _ADJD1(D, E) ASM_VARS );  // Move + adjust dither
#define MOV_NEGD14(B1, B2, D) asm __volatile( _MOV_FIX13(B1, B2) _NEGD1(D) ASM_VARS );
#define MOV_ADDDE14(B1, B2, D, E) asm __volatile( _MOV_FIX13(B1, B2) _ADJD1(D, E) ASM_VARS );
#define MOV_NEGD24(B1, B2, D) asm __volatile( _MOV_FIX23(B1, B2) _NEGD1(D) ASM_VARS );

// ===== LOOP COUNTER MACROS =====

// DCOUNT2: Decrement pixel counter (2 cycles)
// SBIW: Subtract Immediate from Word
#define DCOUNT2 asm __volatile__("sbiw %[count], 1" ASM_VARS );

// JMPLOOP2: Unconditional jump back to loop start (2 cycles)
#define JMPLOOP2 asm __volatile__("rjmp 1b" ASM_VARS );

// BRLOOP1: Conditional branch out of loop (2 cycles)
// If count != 0, skip the jump to exit; otherwise jump to label 2 (loop end)
#define BRLOOP1 asm __volatile__("brne 3\n\trjmp 2f\n\t3:" ASM_VARS );

// ENDLOOP5: Decrement counter and conditionally loop (5 cycles)
// 2 cycles SBIW + 2 cycles BREQ (not taken) + 2 cycles RJMP = 6 cycles when looping
// 2 cycles SBIW + 1 cycle BREQ (taken) = 3 cycles when exiting
// Used at the end of the main pixel loop
#define ENDLOOP5 asm __volatile__("sbiw %[count], 1\n\tbreq L_%=\n\trjmp 1b\n\tL_%=:\n\t" ASM_VARS);

// DNOP: No-operation that references variables (prevents compiler optimization issues)
#define DNOP asm __volatile__("mov r0,r0" ASM_VARS);

#define DADVANCE 3
#define DUSE (0xFF - (DADVANCE-1))

// Silence compiler warnings about switch/case that is explicitly intended to fall through.
#define FL_FALLTHROUGH __attribute__ ((fallthrough));

	// ===== MAIN BIT-BANGING FUNCTION =====
	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer. By making it static, Y register becomes available for general use.
	//
	// FUNCTION PURPOSE:
	// Outputs RGB pixel data to WS2812/WS2811 LEDs using precisely timed bit-banging
	// While outputting each byte (8 bits), the next byte is simultaneously loaded, dithered, and color-scaled
	// This "double buffering" approach (b0=current output, b1=next prepared) is critical for meeting timing requirements
	//
	// TIMING ARCHITECTURE:
	// Each bit takes T1+T2+T3 cycles. During these cycles:
	// - T1 cycles: Line is HIGH, operations prepare for '0' bit test
	// - At T1: QLO2 tests bit - if 0, line goes LOW (encodes '0')
	// - T2-T1 cycles: More operations, line stays HIGH if bit was 1
	// - At T2: If bit was 1, line finally goes LOW (encodes '1')
	// - T3-T2 cycles: Final operations, line is LOW, prepare for next bit
	//
	// RGB TRIPLE PROCESSING:
	// The loop processes 3 bytes (RGB) per pixel iteration:
	// 1. Output byte 0 (e.g., R) while preparing byte 1 (e.g., G)
	// 2. Output byte 1 (e.g., G) while preparing byte 2 (e.g., B)
	// 3. Output byte 2 (e.g., B) while preparing byte 0 of next pixel (e.g., next R)
	static void /*__attribute__((optimize("O0")))*/  /*__attribute__ ((always_inline))*/  showRGBInternal(PixelController<RGB_ORDER> & pixels)  {
		uint8_t *data = (uint8_t*)pixels.mData;  // Pointer to pixel array
		data_ptr_t port = FastPin<DATA_PIN>::port();  // GPIO port register address
		data_t mask = FastPin<DATA_PIN>::mask();  // Pin bit mask
		uint8_t scale_base = 0;  // Scratch variable for scaling operations

		// FASTLED_REGISTER uint8_t *end = data + nLeds;

		// Compute port values for HIGH and LOW states
		data_t hi = *port | mask;   // Set data pin bit to 1 (HIGH), preserve other pins
		data_t lo = *port & ~mask;  // Clear data pin bit to 0 (LOW), preserve other pins
		*port = lo;  // Initialize line to LOW state

		// DOUBLE-BUFFER BYTES: b0=currently outputting, b1=next being prepared
		uint8_t b0 = 0;  // The byte currently being written out bit-by-bit
		uint8_t b1 = 0;  // The byte currently being loaded, dithered, and scaled for next output

		// Setup the pixel controller
		pixels.preStepFirstByteDithering();

		// ===== EXTRACT VARIABLES FOR ASM ACCESS =====
		// Pull all needed values into local variables so inline asm can reference them via ASM_VARS
		// These are mapped to AVR registers through the ASM_VARS constraint list

		// advanceBy: Stride to next pixel (typically 3 for RGB, 4 for RGBW, negative for reverse order)
		// Cast to int16 for sign extension (handles negative strides correctly)
		int16_t advanceBy = pixels.advanceBy();
		uint16_t count = pixels.mLen;  // Number of pixels to output

		// s0, s1, s2: Scale factors for each color channel (brightness/color correction)
		// Range: 0-255 (0=off, 255=full brightness)
		// RO() handles RGB_ORDER remapping (e.g., RGB vs GRB vs BGR)
		uint8_t s0 = pixels.mColorAdjustment.premixed.raw[RO(0)];
		uint8_t s1 = pixels.mColorAdjustment.premixed.raw[RO(1)];
		uint8_t s2 = pixels.mColorAdjustment.premixed.raw[RO(2)];
#if (FASTLED_SCALE8_FIXED==1)
		// Scale8 fix: increment scale factors so 255 wraps to 0
		// Assembly code detects s==0 as special case to return unscaled value
		// This makes scale8(x, 255) return x instead of (x*255)/256
		s0++; s1++; s2++;
#endif

		// d0, d1, d2: Current dither values for each channel
		// Dithering reduces color banding by adding controlled temporal noise
		uint8_t d0 = pixels.d[RO(0)];
		uint8_t d1 = pixels.d[RO(1)];
		uint8_t d2 = pixels.d[RO(2)];

		// e0, e1, e2: Dither step values (d is updated by: d = e - d after each pixel)
		uint8_t e0 = pixels.e[RO(0)];
		uint8_t e1 = pixels.e[RO(1)];
		uint8_t e2 = pixels.e[RO(2)];

		uint8_t loopvar=0;  // Scratch variable for delay loops

		// ===== FIRST BYTE INITIALIZATION =====
		// Load and scale the very first byte (byte 0 of first pixel) before entering main loop
		// This has to be done in asm to keep gcc from messing up the asm code further down
		b0 = data[RO(0)];  // Load first pixel's first byte
		{
			// Perform complete scaling operation on first byte:
			// 1. Load byte from data into scale_base, clear b0 accumulator, clear carry
			// 2. Add dither value d0 to scale_base (with overflow clamping)
			// 3. For each bit N (0-7) in scale factor s0:
			//    - If bit N is set: add scale_base to b0
			//    - Rotate b0 right (b0 >>= 1) to position for next bit
			// 4. After 8 iterations, b0 contains scaled result
			// 5. Copy result to b1, then back to b0 (with dither adjustment)

			LDSCL4(b0,O0) 	PRESCALEA2(d0)     // Load byte, clear accumulator; start dither add
			PRESCALEB4(d0)	SCALE02(b0,0)     // Finish dither clamp; scale bit 0
			RORSC04(b0,1) 	ROR1(b0) CLC1     // Rotate, clear, scale bit 1; rotate, clear
			SCROR04(b0,2)	SCALE02(b0,3)     // Scale bit 2, rotate, clear; scale bit 3
			RORSC04(b0,4) 	ROR1(b0) CLC1     // Rotate, clear, scale bit 4; rotate, clear
			SCROR04(b0,5) 	SCALE02(b0,6)     // Scale bit 5, rotate, clear; scale bit 6
			RORSC04(b0,7) 	ROR1(b0) CLC1     // Rotate, clear, scale bit 7; rotate, clear
			MOV_ADDDE04(b1,b0,d0,e0)          // Move result to b1, adjust dither (d0 = d0 + e0)
			MOV1(b0,b1)                       // Copy back to b0 for output
		}

		{
			// ===== MAIN OUTPUT LOOP =====
			// This loop processes all pixels, outputting 3 bytes (RGB) per iteration
			// Each iteration outputs 24 bits (3 bytes × 8 bits) to the LED strip
			{
				// Loop beginning
				DNOP;  // NOP for timing alignment
				LOOP;  // Assembly label "1:" - loop start target

				// ===== LOOP STRUCTURE OVERVIEW =====
				// The loop is organized into three sections, one for each byte in an RGB triple:
				//
				// SECTION 1: Output byte 0 (first color, e.g., R) while preparing byte 1 (e.g., G)
				// SECTION 2: Output byte 1 (second color, e.g., G) while preparing byte 2 (e.g., B)
				// SECTION 3: Output byte 2 (third color, e.g., B) while preparing byte 0 of next pixel
				//
				// Each section outputs 8 bits using this pattern for each bit N (7 down to 0):
				//   HI1        - Set line HIGH (start bit transmission)
				//   _D1(1)     - Delay T1 cycles
				//   QLO2(b0,N) - Test bit N: if 0, drop line LOW now (encode '0'); if 1, skip (encode '1')
				//   [OPERATIONS] - 4 cycles of scaling/dithering operations on next byte
				//   _D2(4)     - Delay to reach T2 timing point
				//   LO1        - Drop line LOW (if bit was 1, this is where '1' encoding happens)
				//   [OPERATIONS] - 2 cycles of scaling/dithering operations
				//   _D3(2)     - Delay to complete bit period T3
				//
				// TIMING DISCIPLINE:
				// Each row of assembly operations must consume exactly (T1+T2+T3) cycles
				// The _D1/_D2/_D3 macros adjust delays to account for the cycles consumed by
				// the operations in that row. For example, if operations take 4 cycles and we need
				// 6 cycles total, _D2(4) will insert a 2-cycle delay.
				//
				// INTERLEAVED SCALING:
				// While outputting 8 bits of b0, we perform the 8-step scaling operation on b1:
				// Bit 7 output: Load byte, dither it
				// Bit 6 output: Scale bit 0
				// Bit 5 output: Rotate, scale bit 1
				// Bit 4 output: Scale bit 2, scale bit 3
				// Bit 3 output: Rotate, scale bit 4
				// Bit 2 output: Scale bit 5, scale bit 6
				// Bit 1 output: Rotate, scale bit 7
				// Bit 0 output: Move result to b0, adjust dither
				//
				// This interleaving is what makes the timing work on slow AVR chips!

				// ===== BYTE 0 OUTPUT (while preparing byte 1) =====
				// Each line outputs one bit of b0 (bit 7 down to 0, MSB first)
				// Simultaneously performs one step of scaling b1 (byte 1)
				HI1 _D1(1) QLO2(b0, 7) LDSCL4(b1,O1) 	_D2(4)	LO1	PRESCALEA2(d1)	_D3(2)  // Bit 7: Load b1, start dither
				HI1 _D1(1) QLO2(b0, 6) PRESCALEB4(d1)	_D2(4)	LO1	SCALE12(b1,0)	_D3(2)  // Bit 6: Finish dither, scale bit 0
				HI1 _D1(1) QLO2(b0, 5) RORSC14(b1,1) 	_D2(4)	LO1 RORCLC2(b1)		_D3(2)  // Bit 5: Rotate+scale bit 1, rotate
				HI1 _D1(1) QLO2(b0, 4) SCROR14(b1,2)	_D2(4)	LO1 SCALE12(b1,3)	_D3(2)  // Bit 4: Scale bit 2+rotate, scale bit 3
				HI1 _D1(1) QLO2(b0, 3) RORSC14(b1,4) 	_D2(4)	LO1 RORCLC2(b1) 	_D3(2)  // Bit 3: Rotate+scale bit 4, rotate
				HI1 _D1(1) QLO2(b0, 2) SCROR14(b1,5) 	_D2(4)	LO1 SCALE12(b1,6)	_D3(2)  // Bit 2: Scale bit 5+rotate, scale bit 6
				HI1 _D1(1) QLO2(b0, 1) RORSC14(b1,7) 	_D2(4)	LO1 RORCLC2(b1) 	_D3(2)  // Bit 1: Rotate+scale bit 7, rotate
				HI1 _D1(1) QLO2(b0, 0)  // Bit 0 output starts (final bit of byte 0)
				// XTRA0: Optional extra bit periods for chipsets needing longer reset pulses
				// These output dummy bit 0 repeatedly to extend the reset time
				switch(XTRA0) {
					case 4: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)  /* fall through */
					case 3: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)  /* fall through */
					case 2: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)  /* fall through */
					case 1: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)
				}
				MOV_ADDDE14(b0,b1,d1,e1) _D2(4) LO1 _D3(0)  // Finish bit 0: move scaled b1 to b0, adjust dither d1

				// ===== BYTE 1 OUTPUT (while preparing byte 2) =====
				// Same pattern as byte 0, but scales b1→b2 and uses d2/s2 for byte 2
				HI1 _D1(1) QLO2(b0, 7) LDSCL4(b1,O2) 	_D2(4)	LO1	PRESCALEA2(d2)	_D3(2)  // Bit 7: Load b2, start dither
				HI1 _D1(1) QLO2(b0, 6) PSBIDATA4(d2)	_D2(4)	LO1	SCALE22(b1,0)	_D3(2)  // Bit 6: Finish dither+increment data ptr, scale bit 0
				HI1 _D1(1) QLO2(b0, 5) RORSC24(b1,1) 	_D2(4)	LO1 RORCLC2(b1) 	_D3(2)  // Bit 5: Rotate+scale bit 1, rotate
				HI1 _D1(1) QLO2(b0, 4) SCROR24(b1,2)	_D2(4)	LO1 SCALE22(b1,3)	_D3(2)  // Bit 4: Scale bit 2+rotate, scale bit 3
				HI1 _D1(1) QLO2(b0, 3) RORSC24(b1,4) 	_D2(4)	LO1 RORCLC2(b1) 	_D3(2)  // Bit 3: Rotate+scale bit 4, rotate
				HI1 _D1(1) QLO2(b0, 2) SCROR24(b1,5) 	_D2(4)	LO1 SCALE22(b1,6)	_D3(2)  // Bit 2: Scale bit 5+rotate, scale bit 6
				HI1 _D1(1) QLO2(b0, 1) RORSC24(b1,7) 	_D2(4)	LO1 RORCLC2(b1) 	_D3(2)  // Bit 1: Rotate+scale bit 7, rotate
				HI1 _D1(1) QLO2(b0, 0)  // Bit 0 output starts (final bit of byte 1)
				switch(XTRA0) {
					case 4: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)  /* fall through */
					case 3: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)  /* fall through */
					case 2: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)  /* fall through */
					case 1: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)
				}

				// Because PSBIDATA4 on byte 1 (bit 6) already incremented the data pointer,
				// we have to do both halves of updating d2 here - negating it (in the
				// MOV_NEGD24 macro) and then adding E back into it
				MOV_NEGD24(b0,b1,d2) _D2(4) LO1 ADDDE1(d2,e2) _D3(1)  // Finish bit 0: move scaled b2 to b0, adjust dither d2

				// ===== BYTE 2 OUTPUT (while preparing byte 0 of NEXT pixel) =====
				// Same pattern, but now we wrap around: scales b1→b0 (next pixel's first byte)
				HI1 _D1(1) QLO2(b0, 7) LDSCL4(b1,O0) 	_D2(4)	LO1	PRESCALEA2(d0)	_D3(2)  // Bit 7: Load next b0, start dither
				HI1 _D1(1) QLO2(b0, 6) PRESCALEB4(d0)	_D2(4)	LO1	SCALE02(b1,0)	_D3(2)  // Bit 6: Finish dither, scale bit 0
				HI1 _D1(1) QLO2(b0, 5) RORSC04(b1,1) 	_D2(4)	LO1 RORCLC2(b1) 	_D3(2)  // Bit 5: Rotate+scale bit 1, rotate
				HI1 _D1(1) QLO2(b0, 4) SCROR04(b1,2)	_D2(4)	LO1 SCALE02(b1,3)	_D3(2)  // Bit 4: Scale bit 2+rotate, scale bit 3
				HI1 _D1(1) QLO2(b0, 3) RORSC04(b1,4) 	_D2(4)	LO1 RORCLC2(b1)  	_D3(2)  // Bit 3: Rotate+scale bit 4, rotate
				HI1 _D1(1) QLO2(b0, 2) SCROR04(b1,5) 	_D2(4)	LO1 SCALE02(b1,6)	_D3(2)  // Bit 2: Scale bit 5+rotate, scale bit 6
				HI1 _D1(1) QLO2(b0, 1) RORSC04(b1,7) 	_D2(4)	LO1 RORCLC2(b1) 	_D3(2)  // Bit 1: Rotate+scale bit 7, rotate
				HI1 _D1(1) QLO2(b0, 0)  // Bit 0 output starts (final bit of byte 2, completes RGB triple)
				switch(XTRA0) {
					case 4: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)  /* fall through */
					case 3: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)  /* fall through */
					case 2: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)  /* fall through */
					case 1: _D2(0) LO1 _D3(0) HI1 _D1(1) QLO2(b0,0)
				}
				MOV_ADDDE04(b0,b1,d0,e0) _D2(4) LO1 _D3(5)  // Finish bit 0: move scaled b0 to output, adjust dither d0
				ENDLOOP5  // Decrement pixel counter, loop if more pixels remain
			}
			DONE;
		}

	}

};

#endif
}  // namespace fl
#endif

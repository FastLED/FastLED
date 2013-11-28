#ifndef __INC_DELAY_H
#define __INC_DELAY_H

////////////////////////////////////////////////////////////////////////////////////////////
//
// Clock cycle counted delay loop
//
////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__arm__) 
# define NOP __asm__ __volatile__ ("nop\n");
# define NOP2 __asm__ __volatile__ ("nop\n\tnop");
#else
#  define NOP __asm__ __volatile__ ("cp r0,r0\n");
#  define NOP2 __asm__ __volatile__ ("rjmp .+0");
#endif

// predeclaration to not upset the compiler
template<int CYCLES> inline void delaycycles();

// TODO: ARM version of _delaycycles_
// worker template - this will nop for LOOP * 3 + PAD cycles total
template<int LOOP, int PAD> inline void _delaycycles_AVR() { 
	delaycycles<PAD>();
	// the loop below is 3 cycles * LOOP.  the LDI is one cycle,
	// the DEC is 1 cycle, the BRNE is 2 cycles if looping back and
	// 1 if not (the LDI balances out the BRNE being 1 cycle on exit)
	__asm__ __volatile__ ( 
		"		LDI R16, %0\n"
		"L_%=:  DEC R16\n"
		"		BRNE L_%=\n"
		: /* no outputs */ 
		: "M" (LOOP) 
		: "r16"
		);
}

// usable definition
#if !defined(__MK20DX128__)
template<int CYCLES> __attribute__((always_inline)) inline void delaycycles() { 
	_delaycycles_AVR<CYCLES / 3, CYCLES % 3>();	
}
#else
template<int CYCLES> __attribute__((always_inline)) inline void delaycycles() { 
	NOP; delaycycles<CYCLES-1>();
}
#endif

// pre-instantiations for values small enough to not need the loop, as well as sanity holders
// for some negative values.
template<> __attribute__((always_inline)) inline void delaycycles<-6>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-5>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-4>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-3>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-2>() {}
template<> __attribute__((always_inline)) inline void delaycycles<-1>() {}
template<> __attribute__((always_inline)) inline void delaycycles<0>() {}
template<> __attribute__((always_inline)) inline void delaycycles<1>() {NOP;}
template<> __attribute__((always_inline)) inline void delaycycles<2>() {NOP2;}
template<> __attribute__((always_inline)) inline void delaycycles<3>() {NOP;NOP2;}
template<> __attribute__((always_inline)) inline void delaycycles<4>() {NOP2;NOP2;}
template<> __attribute__((always_inline)) inline void delaycycles<5>() {NOP2;NOP2;NOP;}

// Some timing related macros/definitions 

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

#endif
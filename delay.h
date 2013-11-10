#ifndef __INC_DELAY_H
#define __INC_DELAY_H

////////////////////////////////////////////////////////////////////////////////////////////
//
// Clock cycle counted delay loop
//
////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__arm__) 
# define NOP __asm__ __volatile__ ("nop\n");
#else
#  define NOP __asm__ __volatile__ ("cp r0,r0\n");
#endif

// predeclaration to not upset the compiler
template<int CYCLES> inline void delaycycles();

// TODO: ARM version of _delaycycles_

// usable definition
#if defined(FASTLED_AVR)
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

template<int CYCLES> __attribute__((always_inline)) inline void delaycycles() { 
	_delaycycles_AVR<CYCLES / 3, CYCLES % 3>();	
}
#else
// template<int LOOP, int PAD> inline void _delaycycles_ARM() { 
// 	delaycycles<PAD>();
// 	// the loop below is 3 cycles * LOOP.  the LDI is one cycle,
// 	// the DEC is 1 cycle, the BRNE is 2 cycles if looping back and
// 	// 1 if not (the LDI balances out the BRNE being 1 cycle on exit)
// 	__asm__ __volatile__ ( 
// 		"		mov.w r9, %0\n"
// 		"L_%=:  subs.w r9, r9, #1\n"
// 		"		bne.n L_%=\n"
// 		: /* no outputs */ 
// 		: "M" (LOOP) 
// 		: "r9"
// 		);
// }


template<int CYCLES> __attribute__((always_inline)) inline void delaycycles() { 
	// _delaycycles_ARM<CYCLES / 3, CYCLES % 3>();
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
template<> __attribute__((always_inline)) inline void delaycycles<2>() {NOP;NOP;}
template<> __attribute__((always_inline)) inline void delaycycles<3>() {NOP;NOP;NOP;}
template<> __attribute__((always_inline)) inline void delaycycles<4>() {NOP;NOP;NOP;NOP;}
template<> __attribute__((always_inline)) inline void delaycycles<5>() {NOP;NOP;NOP;NOP;NOP;}

#ifdef __SAM3X8E__
class SysClockSaver {
	SysTick_Type m_Saved;
public:
	SysClockSaver(int newTimeValue) { save(newTimeValue); }
	void save(int newTimeValue) { 
		m_Saved.CTRL = SysTick->CTRL;
		SysTick->CTRL &= ~(SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk);
		m_Saved.LOAD = SysTick->LOAD;
		m_Saved.VAL = SysTick->VAL;

		SysTick->VAL = 0;
		SysTick->LOAD = newTimeValue;
		SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
		SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;
	}

	void restore() { 
		SysTick->CTRL &= ~(SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk);
		SysTick->LOAD = m_Saved.LOAD;
		SysTick->VAL = m_Saved.VAL;
		SysTick->CTRL = m_Saved.CTRL;
	}
};

#endif

#endif
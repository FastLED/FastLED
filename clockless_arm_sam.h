#ifndef __INC_CLOCKLESS_ARM_SAM_H
#define __INC_CLOCKLESS_ARM_SAM_H

// Definition for a single channel clockless controller for the sam family of arm chips, like that used in the due and rfduino
// See clockless.h for detailed info on how the template parameters are used.

#if defined(__SAM3X8E__)

#define TOTAL ( (T1+2) + (T2+2) + (T3+2) )
#define T1_MARK (TOTAL - (T1+2))
#define T2_MARK (T1_MARK - (T2+2))

#define SCALE(S,V) scale8_video(S,V)
// #define SCALE(S,V) scale8(S,V)

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, bool FLIP = false, int WAIT_TIME = 500>
class ClocklessController : public CLEDController {
	typedef typename FastPinBB<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPinBB<DATA_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() { 
		FastPinBB<DATA_PIN>::setOutput();
		mPinMask = FastPinBB<DATA_PIN>::mask();
		mPort = FastPinBB<DATA_PIN>::port();
	}

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale = CRGB::White) {
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

		showRGBInternal<0, false>(nLeds, scale, (const byte*)&data);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 8 * (TOTAL));
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		do { TimeTick_Increment(); } while(--millisTaken > 0);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale = CRGB::White) { 
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);
		
		// Serial.print("Scale is "); 
		// Serial.print(scale.raw[0]); Serial.print(" ");
		// Serial.print(scale.raw[1]); Serial.print(" ");
		// Serial.print(scale.raw[2]); Serial.println(" ");
		// FastPinBB<DATA_PIN>::hi(); delay(1); FastPinBB<DATA_PIN>::lo();
		showRGBInternal<0, true>(nLeds, scale, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 8 * (TOTAL));
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		do { TimeTick_Increment(); } while(--millisTaken > 0);
		sei();
		mWait.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale = CRGB::White) { 
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

		showRGBInternal<1, true>(nLeds, scale, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 8 * (TOTAL));
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		do { TimeTick_Increment(); } while(--millisTaken > 0);
		sei();
		mWait.mark();
	}
#endif

// I hate using defines for these, should find a better representation at some point
#define _CTRL CTPTR[0]
#define _LOAD CTPTR[1]
#define _VAL CTPTR[2]

	__attribute__((always_inline)) static inline void wait_loop_start(register volatile uint32_t *CTPTR) {
		__asm__ __volatile__ (
			"L_%=: ldr.w r8, [%0]\n"
			"      tst.w r8, #65536\n"
			"		beq.n L_%=\n"
			: /* no outputs */
			: "r" (CTPTR)
			: "r8"
			);
	}

	template<int MARK> __attribute__((always_inline)) static inline void wait_loop_mark(register volatile uint32_t *CTPTR) {
		__asm__ __volatile__ (
			"L_%=: ldr.w r8, [%0, #8]\n"
			"      cmp.w r8, %1\n"
			"		bhi.n L_%=\n"
			: /* no outputs */
			: "r" (CTPTR), "I" (MARK)
			: "r8"
			);
	}

	__attribute__((always_inline)) static inline void mark_port(register data_ptr_t port, register int val) {
		__asm__ __volatile__ (
			"	str.w %0, [%1]\n"
			: /* no outputs */
			: "r" (val), "r" (port)
			);
	}
#define AT_BIT_START(X) wait_loop_start(CTPTR); X;
#define AT_MARK(X) wait_loop_mark<T1_MARK>(CTPTR); { X; }
#define AT_END(X) wait_loop_mark<T2_MARK>(CTPTR); { X; }

// #define AT_BIT_START(X) while(!(_CTRL & SysTick_CTRL_COUNTFLAG_Msk)); { X; }
// #define AT_MARK(X) while(_VAL > T1_MARK); { X; }
// #define AT_END(X) while(_VAL > T2_MARK); { X; }

//#define AT_MARK(X) delayclocks_until<T1_MARK>(_VAL); X; 
//#define AT_END(X) delayclocks_until<T2_MARK>(_VAL); X;


	template<int MARK> __attribute__((always_inline)) static inline void delayclocks_until(register byte b) { 
		__asm__ __volatile__ (
			"	   sub %0, %0, %1\n"
			"L_%=: subs %0, %0, #2\n"
			"      bcs.n L_%=\n"
			: /* no outputs */
			: "r" (b), "I" (MARK)
			: /* no clobbers */
			);

	}

	inline static void write8Bits(register volatile uint32_t *CTPTR, register data_ptr_t port, register uint8_t & b)  __attribute__ ((always_inline)) {
		// TODO: hand rig asm version of this method.  The timings are based on adjusting/studying GCC compiler ouptut.  This
		// will bite me in the ass at some point, I know it.
		for(register uint32_t i = 8; i > 0; i--) { 
			AT_BIT_START(*port=1);
			if(b&0x80) {} else { AT_MARK(*port=0); }
			b <<= 1;
			AT_END(*port=0);
		}
	}

#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )
	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	template<int SKIP, bool ADVANCE> static void showRGBInternal(register int nLeds, register CRGB scale, register const byte *rgbdata) {
		register data_ptr_t port asm("r7") = FastPinBB<DATA_PIN>::port(); FORCE_REFERENCE(port);
		register byte *data = (byte*)rgbdata;
		register uint8_t *end = data + (nLeds*3 + SKIP); 
		*port = 0;

		// Setup the pixel controller and load/scale the first byte 
		PixelController<RGB_ORDER> pixels(data, scale, true, ADVANCE, SKIP);
		register uint8_t b = pixels.loadAndScale0();

		// Setup and start the clock
		register volatile uint32_t *CTPTR asm("r6")= &SysTick->CTRL; FORCE_REFERENCE(CTPTR);
		_LOAD = TOTAL;
		_VAL = 0;
		_CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
		_CTRL |= SysTick_CTRL_ENABLE_Msk;

		// read to clear the loop flag
		_CTRL;
		while(nLeds-- > 0) { 
			pixels.stepDithering();

			write8Bits(CTPTR, port, b);

			b = pixels.loadAndScale1();
			write8Bits(CTPTR, port,b);

			b = pixels.loadAndScale2();
			write8Bits(CTPTR, port,b);

			b = pixels.advanceAndLoadAndScale0();
		};
	}
};

#endif

#endif

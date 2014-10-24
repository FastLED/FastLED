#ifndef __INC_CLOCKLESS_ARM_SAM_H
#define __INC_CLOCKLESS_ARM_SAM_H

// Definition for a single channel clockless controller for the sam family of arm chips, like that used in the due and rfduino
// See clockless.h for detailed info on how the template parameters are used.

#if defined(__SAM3X8E__)


#define TADJUST 0
#define TOTAL ( (T1+TADJUST) + (T2+TADJUST) + (T3+TADJUST) )
#define T1_MARK (TOTAL - (T1+TADJUST))
#define T2_MARK (T1_MARK - (T2+TADJUST))

#define SCALE(S,V) scale8_video(S,V)
// #define SCALE(S,V) scale8(S,V)

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 500>
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

protected:

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

		uint32_t clocks = showRGBInternal(pixels);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		do { TimeTick_Increment(); } while(--millisTaken > 0);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

		// Serial.print("Scale is ");
		// Serial.print(scale.raw[0]); Serial.print(" ");
		// Serial.print(scale.raw[1]); Serial.print(" ");
		// Serial.print(scale.raw[2]); Serial.println(" ");
		// FastPinBB<DATA_PIN>::hi(); delay(1); FastPinBB<DATA_PIN>::lo();
		uint32_t clocks = showRGBInternal(pixels);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		do { TimeTick_Increment(); } while(--millisTaken > 0);
		sei();
		mWait.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
		PixelController<RGB_ORDER> pixels(rgbdata, nLeds, scale, getDither());
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

		uint32_t clocks = showRGBInternal(pixels);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		do { TimeTick_Increment(); } while(--millisTaken > 0);
		sei();
		mWait.mark();
	}
#endif

#if 0
// Get the arm defs, register/macro defs from the k20
#define ARM_DEMCR               *(volatile uint32_t *)0xE000EDFC // Debug Exception and Monitor Control
#define ARM_DEMCR_TRCENA                (1 << 24)        // Enable debugging & monitoring blocks
#define ARM_DWT_CTRL            *(volatile uint32_t *)0xE0001000 // DWT control register
#define ARM_DWT_CTRL_CYCCNTENA          (1 << 0)                // Enable cycle count
#define ARM_DWT_CYCCNT          *(volatile uint32_t *)0xE0001004 // Cycle count register

	template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register data_ptr_t port, register uint8_t & b)  {
		for(register uint32_t i = BITS; i > 0; i--) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
			*port = 1;
			uint32_t flip_mark = next_mark - ((b&0x80) ? (T3) : (T2+T3));
			b <<= 1;
			while(ARM_DWT_CYCCNT < flip_mark);
			*port = 0;
		}
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static void showRGBInternal(PixelController<RGB_ORDER> pixels) {
		register data_ptr_t port = FastPinBB<DATA_PIN>::port();
		*port = 0;

		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		register uint8_t b = pixels.loadAndScale0();

	    // Get access to the clock
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;
		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		while(pixels.has(1)) {
			pixels.stepDithering();

			// Write first byte, read next byte
			writeBits<8+XTRA0>(next_mark, port, b);
			b = pixels.loadAndScale1();

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0>(next_mark, port, b);
			b = pixels.loadAndScale2();

			// Write third byte
			writeBits<8+XTRA0>(next_mark, port, b);
			b = pixels.advanceAndLoadAndScale0();
		};
	}
#else
// I hate using defines for these, should find a better representation at some point
#define _CTRL CTPTR[0]
#define _LOAD CTPTR[1]
#define _VAL CTPTR[2]
#define VAL (volatile uint32_t)(*((uint32_t*)(SysTick_BASE + 8)))

	template<int BITS>  __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register data_ptr_t port, register uint8_t & b) {
		for(register uint32_t i = BITS; i > 0; i--) {
			// wait to start the bit, then set the pin high
			while(VAL > next_mark);
			next_mark = (VAL-TOTAL);
			*port = 1;

			// how long we want to wait next depends on whether or not our bit is set to 1 or 0
			if(b&0x80) {
				// we're a 1, wait until there's less than T3 clocks left
				while((VAL - next_mark) > (T3));
			} else {
				// we're a 0, wait until there's less than (T2+T3+slop) clocks left in this bit
				while((VAL-next_mark) > (T2+T3+6+TADJUST+TADJUST));
			}
			*port=0;
			b <<= 1;
		}
	}

#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )
	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static uint32_t showRGBInternal(PixelController<RGB_ORDER> & pixels) {
		// Setup and start the clock
		register volatile uint32_t *CTPTR asm("r6")= &SysTick->CTRL; FORCE_REFERENCE(CTPTR);
		_LOAD = 0x00FFFFFF;
		_VAL = 0;
		_CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
		_CTRL |= SysTick_CTRL_ENABLE_Msk;

		register data_ptr_t port asm("r7") = FastPinBB<DATA_PIN>::port(); FORCE_REFERENCE(port);
		*port = 0;

		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		register uint8_t b = pixels.loadAndScale0();

		uint32_t next_mark = (VAL - (TOTAL));
		while(pixels.has(1)) {
			pixels.stepDithering();

			writeBits<8+XTRA0>(next_mark, port, b);

			b = pixels.loadAndScale1();
			writeBits<8+XTRA0>(next_mark, port,b);

			b = pixels.loadAndScale2();
			writeBits<8+XTRA0>(next_mark, port,b);

			b = pixels.advanceAndLoadAndScale0();
		};

		return 0x00FFFFFF - _VAL;
	}
#endif
};

#endif

#endif

 #ifndef __INC_BLOCK_CLOCKLESS_H
#define __INC_BLOCK_CLOCKLESS_H

#include "controller.h"
#include "lib8tion.h"
#include "led_sysdefs.h"
#include "delay.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Base template for clockless controllers.  These controllers have 3 control points in their cycle for each bit.  The first point
// is where the line is raised hi.  The second pointsnt is where the line is dropped low for a zero.  The third point is where the 
// line is dropped low for a one.  T1, T2, and T3 correspond to the timings for those three in clock cycles.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define PORT_MASK 0x77EFF3FE
#define SKIPLIST ~PORT_MASK

#if defined(__SAM3X8E__)
#define HAS_BLOCKLESS 1
template <int NUM_LANES, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int WAIT_TIME = 50>
class BlockClocklessController : public CLEDController {
	typedef typename FastPinBB<1>::port_ptr_t data_ptr_t;
	typedef typename FastPinBB<1>::port_t data_t;

	CMinWait<WAIT_TIME> mWait;
	uint32_t *m_pBuffer;

	void transformData(uint8_t *leddata, int num_leds, uint8_t scale = 255) {
		if(m_pBuffer == NULL) { 
			m_pBuffer = (uint32_t*)malloc(4 * 8 * 3 * num_leds);
		}

		uint32_t *outputdata = m_pBuffer;
		for(register int i = 0; i < num_leds; i++) {
			register byte rgboffset = RGB_BYTE0(RGB_ORDER);
			for(int rgb = 0; rgb < 3; rgb++) {
				register uint32_t mask = 0x01; // 0x01;
				register uint32_t output[8] = {0,0,0,0,0,0,0,0};

				// set the base address to skip through
				uint8_t *database = leddata + (3*i) + rgboffset;
				for(int j = 0; j < NUM_LANES; j++) {
					register uint8_t byte = ~scale8(*database, scale);
					if(byte & 0x80) { output[0] |= mask; }
					if(byte & 0x40) { output[1] |= mask; }
					if(byte & 0x20) { output[2] |= mask; }
					if(byte & 0x10) { output[3] |= mask; }
					if(byte & 0x08) { output[4] |= mask; }
					if(byte & 0x04) { output[5] |= mask; }
					if(byte & 0x02) { output[6] |= mask; }
					if(byte & 0x01) { output[7] |= mask; }

					// SKIPLIST is a 32 bit constant that contains the bit positions that are off limits, courtesy of port stupidities
					do { mask <<= 1; } while(SKIPLIST & mask);

					// move the data pointer forward a lane
					database += (num_leds * 3);

					// cycle between rgb ordering according to RGB order set (may need per-lane rgb ordering?  ugh ugh ugh, i hope not!)
					rgboffset = (rgboffset == RGB_BYTE0(RGB_ORDER)) ? RGB_BYTE1(RGB_ORDER) : (rgboffset == RGB_BYTE1(RGB_ORDER) ?  RGB_BYTE2(RGB_ORDER) : RGB_BYTE0(RGB_ORDER));

					// copy data out
					for(int j = 0; j < 8; j++) { *outputdata++ = output[j]; }
				}
			}
		}	
	}

public:
	virtual void init() { 
		//FastPinBB<DATA_PIN>::setOutput();
		uint8_t pins[] = { 33, 34, 35, 36, 37, 38, 39, 40, 41, 51, 50, 49, 48, 47, 46,45, 44, 9, 8, 7, 6, 5, 4, 3, 10, 72, 0 };
		int i = 0;
		while(pins[i]) { pinMode(pins[i++], OUTPUT); }
		m_pBuffer = NULL;
	}

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		mWait.wait();
		cli();
		SysClockSaver savedClock(T1 + T2 + T3);

		// showRGBInternal<0, false>(nLeds, scale, (const byte*)&data);

		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		savedClock.restore();
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, uint8_t scale = 255) { 
		transformData((uint8_t*)rgbdata, nLeds, scale);
		mWait.wait();
		cli();
		SysClockSaver savedClock(T1 + T2 + T3);
		
		// FastPinBB<DATA_PIN>::hi(); delay(1); FastPinBB<DATA_PIN>::lo();
		showRGBInternal<0, true>(nLeds);

		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		savedClock.restore();
		sei();
		mWait.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, uint8_t scale = 255) { 
		transformData((uint8_t*)rgbdata, nLeds, scale);

		mWait.wait();
		cli();
		SysClockSaver savedClock(T1 + T2 + T3);

		showRGBInternal<1, true>(nLeds, scale, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		savedClock.restore();
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

#define TOTAL (T1 + T2 + T3)

#define T1_MARK (TOTAL - T1)
#define T2_MARK (T1_MARK - T2)
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

#define FORCE_REFERENCE(var)  asm volatile( "" : : "r" (var) )

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	template<int SKIP, bool ADVANCE> void showRGBInternal(register int nLeds) {
		register uint32_t *data = m_pBuffer;
		register uint32_t *end = data + 8*(nLeds*3); 
		
		register volatile uint32_t *CTPTR asm("r6")= &SysTick->CTRL; FORCE_REFERENCE(CTPTR);
		
		// Setup and start the clock
		_LOAD = TOTAL;
		_VAL = 0;
		_CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
		_CTRL |= SysTick_CTRL_ENABLE_Msk;

		// read to clear the loop flag
		_CTRL;

		while(data < end) { 
			register uint32_t d = *data++;
			// turn everything in the port on at the start
			AT_BIT_START(REG_PIOC_SODR = PORT_MASK);

			// part way through early clear the ones that should be 0 bits
			AT_MARK(REG_PIOC_CODR = d);

			// now all the way through zero everyone
			AT_END(REG_PIOC_CODR= PORT_MASK);
		};
	}
};

#endif

#endif
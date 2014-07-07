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

// #define PORT_MASK 0x77EFF3FE // due
#define PORT_MASK 0x7FFFFFE
#define SKIPLIST ~PORT_MASK

#if defined(__SAM3X8E__)
#define HAS_BLOCKLESS 1
#define LANES 8

typedef union {
  uint8_t bytes[LANES];
  uint32_t raw[LANES/4];
  uint16_t shorts[LANES/2];
} Lines;

void transposeLines(Lines & out, Lines & in);

#define TADJUST 0
#define TOTAL ( (T1+TADJUST) + (T2+TADJUST) + (T3+TADJUST) )
#define T1_MARK (TOTAL - (T1+TADJUST))
#define T2_MARK (T1_MARK - (T2+TADJUST))
template <int NUM_LANES, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class InlineBlockClocklessController : public CLEDController {
	typedef typename FastPin<33>::port_ptr_t data_ptr_t;
	typedef typename FastPin<33>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() {
		//FastPinBB<DATA_PIN>::setOutput();
		// uint8_t pins[] = { 33, 34, 35, 36, 37, 38, 39, 40, 41, 51, 50, 49, 48, 47, 46,45, 44, 9, 8, 7, 6, 5, 4, 3, 10, 0}; // 72, 106, 0 }; // due
		uint8_t pins[] = { 33, 34, 35, 36, 37, 38, 39, 40, 41, 107, 106, 51, 50, 49, 48, 47, 46,45, 44, 105, 9, 8, 7, 6, 5, 4, 104, 3, 10, 109, 0}; // 72, 106, 0 }; // digix
    int i = 0;
		while(pins[i]) { pinMode(pins[i++], OUTPUT); }
	}

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
    MultiPixelController<LANES,PORT_MASK,RGB_ORDER> pixels(rgbdata,nLeds, scale, getDither() );
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

    uint32_t clocks = showRGBInternal(pixels, nLeds);

		long microsTaken = CLKS_TO_MICROS(clocks);
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		while(millisTaken-- > 0) { TimeTick_Increment(); }
		sei();
		mWait.mark();
	}

// #define ADV_RGB
#define ADV_RGB if(maskbit & PORT_MASK) { rgbdata += nLeds; } maskbit <<= 1;

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
    MultiPixelController<LANES,PORT_MASK,RGB_ORDER> pixels(rgbdata,nLeds, scale, getDither() );
		mWait.wait();
		cli();
		SysClockSaver savedClock(TOTAL);

		uint32_t clocks = showRGBInternal(pixels, nLeds);


		long microsTaken = CLKS_TO_MICROS(clocks);
		long millisTaken = (microsTaken / 1000);
		savedClock.restore();
		while(millisTaken-- > 0) { TimeTick_Increment(); }
		sei();
		// Adjust the timer
		// long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		// MS_COUNTER += (microsTaken / 1000);
		// sei();
		mWait.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
		mWait.wait();
		cli();

		showRGBInternal(PixelController<RGB_ORDER>(rgbdata, nLeds, scale, getDither()));


		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}
#endif

	typedef union {
		uint32_t word;
		struct {
			uint32_t a0:1;
			uint32_t a1:1;
			uint32_t a2:1;
			uint32_t a3:1;
			uint32_t a4:1;
			uint32_t a5:1;
			uint32_t a6:1;
			uint32_t a7:1;
			uint32_t b0:1;
			uint32_t b1:1;
			uint32_t b2:1;
			uint32_t b3:1;
			uint32_t b4:1;
			uint32_t b5:1;
			uint32_t b6:1;
			uint32_t b7:1;
			uint32_t c0:1;
			uint32_t c1:1;
			uint32_t c2:1;
			uint32_t c3:1;
			uint32_t c4:1;
			uint32_t c5:1;
			uint32_t c6:1;
			uint32_t c7:1;
			uint32_t d0:1;
			uint32_t d1:1;
			uint32_t d2:1;
			uint32_t d3:1;
			uint32_t d4:1;
			uint32_t d5:1;
			uint32_t d6:1;
			uint32_t d7:1;
		};
	} xtype;



// #define IFSKIP(N,X) if(PORT_MASK & 1<<X) { X; }
	__attribute__ ((always_inline)) inline static uint32_t bits(Lines & b) {
		xtype fuckery;
		fuckery.word = 0;
		fuckery.a0 = (b.raw[0] >> 31);
		fuckery.a1 = (b.raw[0] >> 23);
		fuckery.a2 = (b.raw[0] >> 15);
		fuckery.a3 = (b.raw[0] >> 7);
		b.raw[0] <<= 1;
		fuckery.a4 = (b.raw[1] >> 31);
		fuckery.a5 = (b.raw[1] >> 23);
		fuckery.a6 = (b.raw[1] >> 15);
		fuckery.a7 = (b.raw[1] >> 7);
		b.raw[1] <<= 1;
#if LANES > 8
		fuckery.b0 = (b.raw[2] >> 31);
		fuckery.b1 = (b.raw[2] >> 23);
		fuckery.b2 = (b.raw[2] >> 15);
		fuckery.b3 = (b.raw[2] >> 7);
		b.raw[2] <<= 1;
		fuckery.b4 = (b.raw[3] >> 31);
		fuckery.b5 = (b.raw[3] >> 23);
		fuckery.b6 = (b.raw[3] >> 15);
		fuckery.b7 = (b.raw[3] >> 7);
		b.raw[3] <<= 1;
#endif
#if (LANES > 16)
		fuckery.c0 = (b.raw[4] >> 31);
		fuckery.c1 = (b.raw[4] >> 23);
		fuckery.c2 = (b.raw[4] >> 15);
		fuckery.c3 = (b.raw[4] >> 7);
		b.raw[4] <<= 1;
		fuckery.c4 = (b.raw[5] >> 31);
		fuckery.c5 = (b.raw[5] >> 23);
		fuckery.c6 = (b.raw[5] >> 15);
		fuckery.c7 = (b.raw[5] >> 7);
		b.raw[5] <<= 1;
#if (LANES > 24)
		fuckery.d0 = (b.raw[6] >> 31);
		fuckery.d1 = (b.raw[6] >> 23);
		fuckery.d2 = (b.raw[6] >> 15);
		fuckery.d3 = (b.raw[6] >> 7);
    b.raw[6] <<= 1;
		fuckery.d4 = (b.raw[7] >> 31);
		fuckery.d5 = (b.raw[7] >> 23);
		fuckery.d6 = (b.raw[7] >> 15);
		fuckery.d7 = (b.raw[7] >> 7);
    b.raw[7] <<= 1;
#endif
#endif
    fuckery.word |= (fuckery.word << 7);
    fuckery.word |= (fuckery.word << 14);

		return ~fuckery.word;
	}

#define VAL *((uint32_t*)(SysTick_BASE + 8))

	template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register Lines & b, Lines & b2, MultiPixelController<LANES, PORT_MASK, RGB_ORDER> &pixels) { // , register uint32_t & b2)  {
		uint32_t flipper = bits(b);
    register uint8_t d = pixels.template getd<PX>(pixels);
    register uint8_t scale = pixels.template getscale<PX>(pixels);

		// Serial.print("flipper is "); Serial.println(flipper);
		for(uint32_t i = 0; i < LANES/(LANES/8); i++) {
			while(VAL > next_mark);

			next_mark = VAL - (TOTAL);
			*FastPin<33>::sport() = PORT_MASK;

			while((VAL-next_mark) > (T2+T3+6));
			*FastPin<33>::cport() = (flipper & PORT_MASK);

      flipper = bits(b);

			while((VAL - next_mark) > T3);
			*FastPin<33>::cport() = PORT_MASK;

b2.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
#if LANES > 8
i++; b2.bytes[i] = pixels.template loadAndScale<PX>(pixels, i,d,scale);
#if LANES > 16
i++; b2.bytes[i] = pixels.template loadAndScale<PX>(pixels, i,d,scale);
#if LANES > 24
i++; b2.bytes[i] = pixels.template loadAndScale<PX>(pixels, i,d,scale);
#endif
#endif
#endif

      // switch(PX) {
			// 	case 0: b2.bytes[i] = pixels.loadAndScale0(i); break;
			// 	case 1: b2.bytes[i] = pixels.loadAndScale1(i); break;
			// 	case 2: b2.bytes[i] = pixels.loadAndScale2(i); break;
			// }
      // switch(PX) {
      //   case 0: b2.bytes[i] = pixels.loadAndScale0(i,d,scale); break;
      //   case 1: b2.bytes[i] = pixels.loadAndScale1(i,d,scale); break;
      //   case 2: b2.bytes[i] = pixels.loadAndScale2(i,d,scale); break;
      // }
//   #if LANES > 8
// 			i++;
//       switch(PX) {
//         case 0: b2.bytes[i] = pixels.loadAndScale0(i,d,scale); break;
//         case 1: b2.bytes[i] = pixels.loadAndScale1(i,d,scale); break;
//         case 2: b2.bytes[i] = pixels.loadAndScale2(i,d,scale); break;
//       }
//   #endif
// #if LANES > 16
//       i++;
//       switch(PX) {
//         case 0: b2.bytes[i] = pixels.loadAndScale0(i,d,scale); break;
//         case 1: b2.bytes[i] = pixels.loadAndScale1(i,d,scale); break;
//         case 2: b2.bytes[i] = pixels.loadAndScale2(i,d,scale); break;
//       }
// #endif
// #if LANES > 24
//       i++;
//       switch(PX) {
//         case 0: b2.bytes[i] = pixels.loadAndScale0(i,d,scale); break;
//         case 1: b2.bytes[i] = pixels.loadAndScale1(i,d,scale); break;
//         case 2: b2.bytes[i] = pixels.loadAndScale2(i,d,scale); break;
//       }
// #endif
		}

    // for(uint32_t i = 0; i < LANES/2; i++) {
    //   while(VAL > next_mark);
    //
    //   next_mark = VAL - (TOTAL);
    //   *FastPin<33>::sport() = PORT_MASK;
    //
    //   while((VAL-next_mark) > (T2+T3+6));
    //   *FastPin<33>::cport() = (flipper & PORT_MASK);
    //   flipper = bits(b);
    //
    //
    //   while((VAL - next_mark) > T3);
    //   *FastPin<33>::cport() = PORT_MASK;
    // }

    // transposeLines(b, b2);
		// for(register uint32_t i = 0; i < XTRA0; i++) {
		// 	AT_BIT_START(*FastPin<33>::port() = 0xFFFF);
		// 	AT_MARK(*FastPin<33>::port() = 0);
		// 	AT_END(*FastPin<2>::port() = 0);
		// }
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static uint32_t showRGBInternal(MultiPixelController<LANES, PORT_MASK, RGB_ORDER> &allpixels, int nLeds) {
		// Serial.println("Entering show");
		// Setup the pixel controller and load/scale the first byte
		Lines b0,b1,b2;
		for(int i = 0; i < LANES/4; i++) {
			b0.raw[i] = b1.raw[i] = b2.raw[i] = 0;
		}
    allpixels.preStepFirstByteDithering();
		for(int i = 0; i < LANES; i++) {
			b0.bytes[i] = allpixels.loadAndScale0(i);
		}

		// Setup and start the clock
		register volatile uint32_t *CTPTR asm("r6")= &SysTick->CTRL; FORCE_REFERENCE(CTPTR);
		_LOAD = 0x00FFFFFF;
		_VAL = 0;
		_CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
		_CTRL |= SysTick_CTRL_ENABLE_Msk;

		VAL = 0;
		uint32_t next_mark = (VAL - (TOTAL));
		while(nLeds--) {
      allpixels.stepDithering();

			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(next_mark, b0, b1, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(next_mark, b1, b2, allpixels);

      allpixels.advanceData();
			// Write third byte
			writeBits<8+XTRA0,0>(next_mark, b2, b0, allpixels);
		}

		return 0x00FFFFFF - _VAL;
	}


};

#endif

#endif

#ifndef __INC_BLOCK_CLOCKLESS_ARM_K20_H
#define __INC_BLOCK_CLOCKLESS_ARM_K20_H

// Definition for a single channel clockless controller for the k20 family of chips, like that used in the teensy 3.0/3.1
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSY3)
#define HAS_BLOCKLESS 1
#define PORT_MASK 0x000000FF
#define SKIPLIST ~PORT_MASK
#define LANES 8

#include "kinetis.h"

template <uint8_t NUM_LANES, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 500>
class InlineBlockClocklessController : public CLEDController {
	typedef typename FastPin<2>::port_ptr_t data_ptr_t;
	typedef typename FastPin<2>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() {
		FastPin<2>::setOutput();
		FastPin<14>::setOutput();
		FastPin<7>::setOutput();
		FastPin<8>::setOutput();
		FastPin<6>::setOutput();
		FastPin<20>::setOutput();
		FastPin<21>::setOutput();
		FastPin<5>::setOutput();
		mPinMask = FastPin<2>::mask();
		mPort = FastPin<2>::port();
	}

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
		MultiPixelController<LANES,PORT_MASK,RGB_ORDER> pixels(rgbdata,nLeds, scale, getDither() );
		mWait.wait();
		cli();

		uint32_t clocks = showRGBInternal(pixels,nLeds);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) {
		MultiPixelController<LANES,PORT_MASK,RGB_ORDER> pixels(rgbdata,nLeds, scale, getDither() );
		mWait.wait();
		cli();

		uint32_t clocks = showRGBInternal(pixels,nLeds);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale) {
		mWait.wait();
		cli();

		uint32_t clocks = showRGBInternal(PixelController<RGB_ORDER>(rgbdata, nLeds, scale, getDither()));


		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS(clocks);
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}
#endif


#define USE_LINES
	typedef union {
		uint8_t bytes[8];
		uint32_t raw[2];
	} Lines;

	template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register Lines & b, Lines & b2, MultiPixelController<LANES, PORT_MASK, RGB_ORDER> &pixels) { // , register uint32_t & b2)  {
		transpose8(b.raw,b2.raw);
		register uint8_t d = pixels.template getd<PX>(pixels);
		register uint8_t scale = pixels.template getscale<PX>(pixels);

		for(register uint32_t i = 0; i < 8; i++) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
			*FastPin<2>::port() = 0xFF;
			// uint32_t flip_mark = next_mark - (T2+T3+2);

			while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+6));
			// while(ARM_DWT_CYCCNT < flip_mark);
			// uint32_t last_flip_mark = next_mark - (T3);
			*FastPin<2>::port() = ~b2.raw[i];

			while((next_mark - ARM_DWT_CYCCNT) > (T3));
			// while(ARM_DWT_CYCCNT < last_flip_mark);
			*FastPin<2>::port() = 0;

			b.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
		}
	}



	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
		static uint32_t showRGBInternal(MultiPixelController<LANES, PORT_MASK, RGB_ORDER> &allpixels, int nLeds) {
		// Get access to the clock
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;

		// Setup the pixel controller and load/scale the first byte
		allpixels.preStepFirstByteDithering();
		register Lines b0,b1;
		b0.raw[0] = b0.raw[1] = 0;
		b1.raw[0] = b1.raw[1] = 0;
		allpixels->preStepFirstByteDithering();
		for(int i = 0; i < 8; i++) {
			b0.bytes[i] = allpixels->loadAndScale0(i);
		}

		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		while(nLeds--) {
			allpixels.stepDithering();

			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(next_mark, b0, b1, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(next_mark, b0, b1, allpixels);

			// Write third byte
			writeBits<8+XTRA0,0>(next_mark, b0, b1, allpixels);
		};

		return ARM_DWT_CYCCNT;
	}
};
#endif

#endif

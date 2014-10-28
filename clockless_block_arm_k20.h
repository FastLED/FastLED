#ifndef __INC_BLOCK_CLOCKLESS_ARM_K20_H
#define __INC_BLOCK_CLOCKLESS_ARM_K20_H

// Definition for a single channel clockless controller for the k20 family of chips, like that used in the teensy 3.0/3.1
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSY3)
#define HAS_BLOCKLESS 1

#define PORTC_FIRST_PIN 15
#define PORTD_FIRST_PIN 2

#define PORT_MASK (((1<<LANES)-1) & ((FIRST_PIN==2) ? 0xFF : 0xFFF))

#include "kinetis.h"

template <uint8_t LANES, int FIRST_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class InlineBlockClocklessController : public CLEDController {
	typedef typename FastPin<FIRST_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<FIRST_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() {
		if(FIRST_PIN == PORTC_FIRST_PIN) { // PORTC
			switch(LANES) {
				case 12: FastPin<30>::setOutput();
				case 11: FastPin<29>::setOutput();
				case 10: FastPin<27>::setOutput();
				case 9: FastPin<28>::setOutput();
				case 8: FastPin<12>::setOutput();
				case 7: FastPin<11>::setOutput();
				case 6: FastPin<13>::setOutput();
				case 5: FastPin<10>::setOutput();
				case 4: FastPin<9>::setOutput();
				case 3: FastPin<23>::setOutput();
				case 2: FastPin<22>::setOutput();
				case 1: FastPin<15>::setOutput();
			}
		} else if(FIRST_PIN == PORTD_FIRST_PIN) { // PORTD
			switch(LANES) {
				case 8: FastPin<5>::setOutput();
				case 7: FastPin<21>::setOutput();
				case 6: FastPin<20>::setOutput();
				case 5: FastPin<6>::setOutput();
				case 4: FastPin<8>::setOutput();
				case 3: FastPin<7>::setOutput();
				case 2: FastPin<14>::setOutput();
				case 1: FastPin<2>::setOutput();
			}
		}
		mPinMask = FastPin<FIRST_PIN>::mask();
		mPort = FastPin<FIRST_PIN>::port();
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


	typedef union {
		uint8_t bytes[16];
		uint16_t shorts[8];
		uint32_t raw[4];
	} Lines;

	template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register Lines & b, MultiPixelController<LANES, PORT_MASK, RGB_ORDER> &pixels) { // , register uint32_t & b2)  {
		register Lines b2;
		if(LANES>8) {
			transpose8<1,2>(b.bytes,b2.bytes);
			transpose8<1,2>(b.bytes+8,b2.bytes+1);
		} else {
			transpose8x1(b.bytes,b2.bytes);
		}
		register uint8_t d = pixels.template getd<PX>(pixels);
		register uint8_t scale = pixels.template getscale<PX>(pixels);

		for(register uint32_t i = 0; (i < LANES) && (i < 8); i++) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3)-3;
			*FastPin<FIRST_PIN>::sport() = PORT_MASK;

			while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+6));
			if(LANES>8) {
				*FastPin<FIRST_PIN>::cport() = ((~b2.shorts[i]) & PORT_MASK);
			} else {
				*FastPin<FIRST_PIN>::cport() = ((~b2.bytes[7-i]) & PORT_MASK);
			}

			while((next_mark - ARM_DWT_CYCCNT) > (T3));
			*FastPin<FIRST_PIN>::cport() = PORT_MASK;

			b.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
			if(LANES>8 && ((i+8) < LANES)) {
				b.bytes[i+8] = pixels.template loadAndScale<PX>(pixels,i+8,d,scale);
			}
		}

		for(register uint32_t i = LANES; i < 8; i++) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3)-3;
			*FastPin<FIRST_PIN>::sport() = PORT_MASK;

			while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+6));
			*FastPin<FIRST_PIN>::cport() = ((~b2.bytes[7-i]) & PORT_MASK);

			while((next_mark - ARM_DWT_CYCCNT) > (T3));
			*FastPin<FIRST_PIN>::cport() = PORT_MASK;

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
		register Lines b0;

		allpixels.preStepFirstByteDithering();
		for(int i = 0; i < LANES; i++) {
			b0.bytes[i] = allpixels.loadAndScale0(i);
		}

		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		while(nLeds--) {
			allpixels.stepDithering();

			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(next_mark, b0, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(next_mark, b0, allpixels);
			allpixels.advanceData();

			// Write third byte
			writeBits<8+XTRA0,0>(next_mark, b0, allpixels);
		};

		return ARM_DWT_CYCCNT;
	}
};
#endif

#endif

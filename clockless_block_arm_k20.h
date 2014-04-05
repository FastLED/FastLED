#ifndef __INC_BLOCK_CLOCKLESS_ARM_K20_H
#define __INC_BLOCK_CLOCKLESS_ARM_K20_H

// Definition for a single channel clockless controller for the k20 family of chips, like that used in the teensy 3.0/3.1
// See clockless.h for detailed info on how the template parameters are used.
#if 1 || defined(FASTLED_TEENSY3)
#define HAS_BLOCKLESS 1

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
		FastPin<25>::setOutput();
		mPinMask = FastPin<2>::mask();
		mPort = FastPin<2>::port();
	}

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & rgbdata, int nLeds, CRGB scale) {
		mWait.wait();
		cli();

		showRGBInternal(PixelController<RGB_ORDER>(rgbdata, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/*/+nLeds/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/*/+nLeds/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/*/+nLeds/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/*/+nLeds/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/*/+nLeds/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/*/+(2*nLeds)/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/*/+(3*nLeds)/**/, nLeds, scale, getDither()));

		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale) { 
		mWait.wait();
		cli();

		showRGBInternal(PixelController<RGB_ORDER>(rgbdata, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/**/+(1*nLeds)/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/**/+(2*nLeds)/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/**/+(3*nLeds)/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/**/+(4*nLeds)/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/**/+(5*nLeds)/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/**/+(6*nLeds)/**/, nLeds, scale, getDither()),
						PixelController<RGB_ORDER>(rgbdata/**/+(7*nLeds)/**/, nLeds, scale, getDither()));
		
		// Adjust the timer
		long microsTaken = nLeds * CLKS_TO_MICROS(24 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
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


#define USE_LINES 
	typedef union { 
		uint8_t bytes[8];
		uint32_t raw[2];
	} Lines;

	__attribute__ ((always_inline)) inline static uint32_t bits(Lines & b) {
		xtype fuckery;
		fuckery.word = 0;
		fuckery.a0 = (b.raw[0] >> 31);
		fuckery.a1 = (b.raw[0] >> 23);
		fuckery.a2 = (b.raw[0] >> 15);
		fuckery.a3 = (b.raw[0] >> 7);
		fuckery.a4 = (b.raw[1] >> 31);
		fuckery.a5 = (b.raw[1] >> 23);
		fuckery.a6 = (b.raw[1] >> 15);
		fuckery.a7 = (b.raw[1] >> 7);
		// fuckery.b0 = (b.raw[2] >> 30);
		// fuckery.b1 = (b.raw[2] >> 22);
		// fuckery.b2 = (b.raw[2] >> 14);
		// fuckery.b3 = (b.raw[2] >> 6);
		// fuckery.b4 = (b.raw[3] >> 30);
		// fuckery.b5 = (b.raw[3] >> 22)
		// fuckery.b6 = (b.raw[3] >> 14);
		// fuckery.b7 = (b.raw[3] >> 6);
		// fuckery.c0 = (b.raw[4] >> 29);
		// fuckery.c1 = (b.raw[4] >> 21);
		// fuckery.c2 = (b.raw[4] >> 13);
		// fuckery.c3 = (b.raw[4] >> 5);
		// fuckery.c4 = (b.raw[5] >> 20);
		// fuckery.c5 = (b.raw[5] >> 21);
		// fuckery.c6 = (b.raw[5] >> 13);
		// fuckery.c7 = (b.raw[5] >> 5);
		// fuckery.d0 = (b.raw[6] >> 28);
		// fuckery.d1 = (b.raw[6] >> 19);
		// fuckery.d2 = (b.raw[6] >> 12);
		// fuckery.d3 = (b.raw[6] >> 4);
		// fuckery.d4 = (b.raw[7] >> 28);
		// fuckery.d5 = (b.raw[7] >> 19);
		// fuckery.d6 = (b.raw[7] >> 12);
		// fuckery.d7 = (b.raw[7] >> 4);
		return fuckery.word;
	}

	template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(register uint32_t & next_mark, register Lines & b, Lines & b2, PixelController<RGB_ORDER> *allpixels[8]) { // , register uint32_t & b2)  {
		uint32_t flipper = bits(b);
		for(register uint32_t i = 0; i < 8; i++) { 
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
			*FastPin<2>::port() = 0xFF;
			uint32_t flip_mark = next_mark - (T2+T3);
			
			while(ARM_DWT_CYCCNT < flip_mark);
			uint32_t last_flip_mark = next_mark - (T3+2);
			*FastPin<2>::port() = flipper;
			b.raw[0] <<= 1;
			b.raw[1] <<= 1;
			flipper = bits(b);

			// switch(PX) {
			// 	case 0: b2.bytes[i] = allpixels[i]->loadAndScale0(); break;
			// 	case 1: b2.bytes[i] = allpixels[i]->loadAndScale1(); break;
			// 	case 2: b2.bytes[i] = allpixels[i]->loadAndScale2(); break;
			// }
			while(ARM_DWT_CYCCNT < last_flip_mark);
			*FastPin<2>::port() = 0;
			// switch(PX) {
			// 	case 0: b2.bytes[i] = allpixels[i]->stepAdvanceAndLoadAndScale0(); break;
			// 	case 1: b2.bytes[i] = allpixels[i]->loadAndScale1(); break;
			// 	case 2: b2.bytes[i] = allpixels[i]->loadAndScale2(); break;
			// }
			switch(PX) {
				case 0: b2.bytes[i] = allpixels[i]->stepAdvanceAndLoadAndScale0(); break;
				case 1: b2.bytes[i] = allpixels[i]->loadAndScale1(); break;
				case 2: b2.bytes[i] = allpixels[i]->loadAndScale2(); break;
			}
		}
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	static void showRGBInternal(PixelController<RGB_ORDER> pixels, PixelController<RGB_ORDER> pixels2, PixelController<RGB_ORDER> pixels3, PixelController<RGB_ORDER> pixels4,
								PixelController<RGB_ORDER> pixels5,  PixelController<RGB_ORDER> pixels6, PixelController<RGB_ORDER> pixels7,  PixelController<RGB_ORDER> pixels8) {		

		// Setup the pixel controller and load/scale the first byte 
		PixelController<RGB_ORDER> *allpixels[8] = {&pixels,&pixels2,&pixels3,&pixels4,&pixels5,&pixels6,&pixels7,&pixels8};
		pixels.preStepFirstByteDithering();
		register Lines b0,b1,b2;
		b0.raw[0] = b0.raw[1] = 0;
		b1.raw[0] = b1.raw[1] = 0;
		b2.raw[0] = b2.raw[1] = 0;
		for(int i = 0; i < 8; i++) {
			allpixels[i]->preStepFirstByteDithering();
			b0.bytes[i] = allpixels[i]->loadAndScale0();
		}

	    // Get access to the clock 
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;
		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		while(pixels.has(1)) { 			
			pixels.stepDithering();

			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(next_mark, b0, b1, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(next_mark, b1, b2, allpixels); 

			// Write third byte
			writeBits<8+XTRA0,0>(next_mark, b2, b0, allpixels); 
		};
	}
};
#endif

#endif


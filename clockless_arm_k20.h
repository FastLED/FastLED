#ifndef __INC_CLOCKLESS_ARM_K20_H
#define __INC_CLOCKLESS_ARM_K20_H

// Definition for a single channel clockless controller for the k20 family of chips, like that used in the teensy 3.0/3.1
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSY3)
template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, bool FLIP = false, int WAIT_TIME = 500>
class ClocklessController : public CLEDController {
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() { 
		FastPin<DATA_PIN>::setOutput();
		mPinMask = FastPin<DATA_PIN>::mask();
		mPort = FastPin<DATA_PIN>::port();
	}

	virtual void clearLeds(int nLeds) {
		showColor(CRGB(0, 0, 0), nLeds, 0);
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale = CRGB::White) {
		mWait.wait();
		cli();

		showRGBInternal<0, false>(nLeds, scale, (const byte*)&data);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 8 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

	virtual void show(const struct CRGB *rgbdata, int nLeds, CRGB scale = CRGB::White) { 
		mWait.wait();
		cli();

		showRGBInternal<0, true>(nLeds, scale, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 8 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}

#ifdef SUPPORT_ARGB
	virtual void show(const struct CARGB *rgbdata, int nLeds, CRGB scale = CRGB::White) { 
		mWait.wait();
		cli();

		showRGBInternal<1, true>(nLeds, scale, (const byte*)rgbdata);

		// Adjust the timer
		long microsTaken = CLKS_TO_MICROS((long)nLeds * 8 * (T1 + T2 + T3));
		MS_COUNTER += (microsTaken / 1000);
		sei();
		mWait.mark();
	}
#endif

	inline static void write8Bits(register uint32_t & next_mark, register data_ptr_t port, register data_t hi, register data_t lo, register uint32_t & b)  __attribute__ ((always_inline)) {
		// TODO: hand rig asm version of this method.  The timings are based on adjusting/studying GCC compiler ouptut.  This
		// will bite me in the ass at some point, I know it.
		for(register uint32_t i = 8; i > 0; i--) { 
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
			FastPin<DATA_PIN>::fastset(port, hi);
			uint32_t flip_mark = next_mark - ((b&0x80) ? (T3) : (T2+T3));
			b <<= 1;
			while(ARM_DWT_CYCCNT < flip_mark);
			FastPin<DATA_PIN>::fastset(port, lo);
		}
	}

#define DITHER 1
#define DADVANCE 3

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then 
	// gcc will use register Y for the this pointer.
	template<int SKIP, bool ADVANCE> static void showRGBInternal(register int nLeds, register CRGB scale, register const byte *rgbdata) {
		register byte *data = (byte*)rgbdata;
		register data_t mask = FastPin<DATA_PIN>::mask();
		register data_ptr_t port = FastPin<DATA_PIN>::port();
		nLeds *= (3 + SKIP);
		register uint8_t *end = data + nLeds; 
		register data_t hi = *port | mask;
		register data_t lo = *port & ~mask;
		*port = lo;

		uint8_t E[3] = {0xFF,0xFF,0xFF};
		uint8_t D[3] = {0,0,0};

		static uint8_t Dstore[3] = {0,0,0};

		// compute the E values and seed D from the stored values
		for(register uint32_t i = 0; i < 3; i++) { 
			byte S = scale.raw[i];
			while(S>>=1) { E[i] >>=1; };
			D[i] = Dstore[i] & E[i];
		}

	    // Get access to the clock 
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;

		register uint32_t b;
		b = ((ADVANCE)?data:rgbdata)[SKIP + RGB_BYTE0(RGB_ORDER)];
		if(DITHER && b) b = qadd8(b, D[B0]);
		b = scale8(b, scale.raw[B0]);

		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
		while(data < end) { 

			// Write first byte, read next byte
			write8Bits(next_mark, port, hi, lo, b);

			D[B0] += DADVANCE; D[B0] &= E[B0];
			b = ((ADVANCE)?data:rgbdata)[SKIP + RGB_BYTE1(RGB_ORDER)];
			if(DITHER && b) b = qadd8(b, D[B1]);
			INLINE_SCALE(b, scale.raw[B1]);

			// Write second byte
			write8Bits(next_mark, port, hi, lo, b);

			D[B1] += DADVANCE; D[B1] &= E[B1];
			b = ((ADVANCE)?data:rgbdata)[SKIP + RGB_BYTE2(RGB_ORDER)];
			if(DITHER && b) b = qadd8(b, D[B2]);
			INLINE_SCALE(b, scale.raw[B2]);

			data += 3 + SKIP;

			// Write third byte
			write8Bits(next_mark, port, hi, lo, b);

			D[B2] += DADVANCE; D[B2] &= E[B2];
			b = ((ADVANCE)?data:rgbdata)[SKIP + RGB_BYTE0(RGB_ORDER)];
			if(DITHER && b) b = qadd8(b, D[B0]);
			INLINE_SCALE(b, scale.raw[B0]);
		};

		// Save the D values for cycling through next time
		Dstore[0] = D[0];
		Dstore[1] = D[1];
		Dstore[2] = D[2];
	}
};
#endif

#endif


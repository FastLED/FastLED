#ifndef __INC_CLOCKLESS_ARM_MXRT1062_H
#define __INC_CLOCKLESS_ARM_MXRT1062_H

FASTLED_NAMESPACE_BEGIN

// Definition for a single channel clockless controller for the teensy4
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSY4)

#define FASTLED_HAS_CLOCKLESS 1

#define _FASTLED_NS_TO_DWT(_NS) (((F_CPU_ACTUAL>>16)*(_NS)) / (1000000000UL>>16))

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
	uint32_t off[3];

public:
	static constexpr int __DATA_PIN() { return DATA_PIN; }
	static constexpr int __T1() { return T1; }
	static constexpr int __T2() { return T2; }
	static constexpr int __T3() { return T3; }
	static constexpr EOrder __RGB_ORDER() { return RGB_ORDER; }
	static constexpr int __XTRA0() { return XTRA0; }
	static constexpr bool __FLIP() { return FLIP; }
	static constexpr int __WAIT_TIME() { return WAIT_TIME; }

	virtual void init() {
		FastPin<DATA_PIN>::setOutput();
		mPinMask = FastPin<DATA_PIN>::mask();
		mPort = FastPin<DATA_PIN>::port();
    	FastPin<DATA_PIN>::lo();
	}

	virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
    	mWait.wait();
		if(!showRGBInternal(pixels)) {
      		sei(); delayMicroseconds(WAIT_TIME); cli();
      		showRGBInternal(pixels);
    	}
    	mWait.mark();
  	}

	template<int BITS> __attribute__ ((always_inline)) inline void writeBits(FASTLED_REGISTER uint32_t & next_mark, FASTLED_REGISTER uint32_t & b)  {
		for(FASTLED_REGISTER uint32_t i = BITS-1; i > 0; --i) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + off[0];
			FastPin<DATA_PIN>::hi();
			if(b&0x80) {
				while((next_mark - ARM_DWT_CYCCNT) > off[2]);
				FastPin<DATA_PIN>::lo();
			} else {
				while((next_mark - ARM_DWT_CYCCNT) > off[1]);
				FastPin<DATA_PIN>::lo();
			}
			b <<= 1;
		}

		while(ARM_DWT_CYCCNT < next_mark);
		next_mark = ARM_DWT_CYCCNT + off[0];
		FastPin<DATA_PIN>::hi();

		if(b&0x80) {
			while((next_mark - ARM_DWT_CYCCNT) > off[2]);
			FastPin<DATA_PIN>::lo();
		} else {
			while((next_mark - ARM_DWT_CYCCNT) > off[1]);
			FastPin<DATA_PIN>::lo();
		}
	}

	uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
		uint32_t start = ARM_DWT_CYCCNT;

		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		FASTLED_REGISTER uint32_t b = pixels.loadAndScale0();

		cli();

		off[0] = _FASTLED_NS_TO_DWT(T1+T2+T3);
		off[1] = _FASTLED_NS_TO_DWT(T2+T3);
		off[2] = _FASTLED_NS_TO_DWT(T3);

	uint32_t wait_off = _FASTLED_NS_TO_DWT((WAIT_TIME-INTERRUPT_THRESHOLD)*1000);

    	uint32_t next_mark = ARM_DWT_CYCCNT + off[0];

		while(pixels.has(1)) {
			pixels.stepDithering();
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			cli();
			// if interrupts took longer than 45µs, punt on the current frame
			if(ARM_DWT_CYCCNT > next_mark) {
				if((ARM_DWT_CYCCNT-next_mark) > wait_off) { sei(); return ARM_DWT_CYCCNT - start; }
			}
			#endif
			// Write first byte, read next byte
			writeBits<8+XTRA0>(next_mark, b);
			b = pixels.loadAndScale1();

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0>(next_mark, b);
			b = pixels.loadAndScale2();

			// Write third byte, read 1st byte of next pixel
			writeBits<8+XTRA0>(next_mark, b);
			b = pixels.advanceAndLoadAndScale0();
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			sei();
			#endif
		};

		sei();
		return ARM_DWT_CYCCNT - start;
	}
};
#endif

FASTLED_NAMESPACE_END

#endif

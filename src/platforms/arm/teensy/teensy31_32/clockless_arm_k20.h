#ifndef __INC_CLOCKLESS_ARM_K20_H
#define __INC_CLOCKLESS_ARM_K20_H

#include "fl/chipsets/timing_traits.h"
#include "fl/stl/vector.h"
#include "fastled_delay.h"

namespace fl {
// Definition for a single channel clockless controller for the k20 family of chips, like that used in the teensy 3.0/3.1
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSY3)

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
	// Extract timing values from struct and convert from nanoseconds to clock cycles
	// Formula: cycles = (nanoseconds * CPU_MHz + 500) / 1000
	// The +500 provides rounding to nearest integer
	static constexpr uint32_t T1 = (TIMING::T1 * (F_CPU / 1000000UL) + 500) / 1000;
	static constexpr uint32_t T2 = (TIMING::T2 * (F_CPU / 1000000UL) + 500) / 1000;
	static constexpr uint32_t T3 = (TIMING::T3 * (F_CPU / 1000000UL) + 500) / 1000;
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

	virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:
	virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
		mWait.wait();
		Rgbw rgbw = this->getRgbw();
		if(!showRGBInternal(pixels, rgbw)) {
			// showRGBInternal already called sei() before returning 0, so no need to call it again
			delayMicroseconds(WAIT_TIME);
			cli(); // Disable interrupts for retry
			showRGBInternal(pixels, rgbw);
		}
		mWait.mark();
	}

	template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(FASTLED_REGISTER uint32_t & next_mark, FASTLED_REGISTER data_ptr_t port, FASTLED_REGISTER data_t hi, FASTLED_REGISTER data_t lo, FASTLED_REGISTER uint8_t & b)  {
		for(FASTLED_REGISTER uint32_t i = BITS-1; i > 0; --i) {
			while(ARM_DWT_CYCCNT < next_mark);
			next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
			FastPin<DATA_PIN>::fastset(port, hi);
			if(b&0x80) {
				while((next_mark - ARM_DWT_CYCCNT) > (T3+(2*(F_CPU/24000000))));
				FastPin<DATA_PIN>::fastset(port, lo);
			} else {
				while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+(2*(F_CPU/24000000))));
				FastPin<DATA_PIN>::fastset(port, lo);
			}
			b <<= 1;
		}

		while(ARM_DWT_CYCCNT < next_mark);
		next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);
		FastPin<DATA_PIN>::fastset(port, hi);

		if(b&0x80) {
			while((next_mark - ARM_DWT_CYCCNT) > (T3+(2*(F_CPU/24000000))));
			FastPin<DATA_PIN>::fastset(port, lo);
		} else {
			while((next_mark - ARM_DWT_CYCCNT) > (T2+T3+(2*(F_CPU/24000000))));
			FastPin<DATA_PIN>::fastset(port, lo);
		}
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels, Rgbw rgbw) {
	    // Get access to the clock
		ARM_DEMCR    |= ARM_DEMCR_TRCENA;
		ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
		ARM_DWT_CYCCNT = 0;

		FASTLED_REGISTER data_ptr_t port = FastPin<DATA_PIN>::port();
		FASTLED_REGISTER data_t hi = *port | FastPin<DATA_PIN>::mask();
		FASTLED_REGISTER data_t lo = *port & ~FastPin<DATA_PIN>::mask();
		*port = lo;

		cli();

		uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

		// Detect RGBW mode using pattern from STM32 driver
		const bool is_rgbw = rgbw.active();

		// Unified loop using fixed-size buffer for both RGB and RGBW
		pixels.preStepFirstByteDithering();
		#if (FASTLED_ALLOW_INTERRUPTS == 1)
		bool first_pixel = true;
		#endif

		while(pixels.has(1)) {
			pixels.stepDithering();
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			// Only call cli() after the first pixel, since line 89 already disabled interrupts initially
			if (!first_pixel) {
				cli();
			}
			first_pixel = false;
			// if interrupts took longer than 45µs, punt on the current frame
			if(ARM_DWT_CYCCNT > next_mark) {
				if((ARM_DWT_CYCCNT-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return 0; }
			}

			hi = *port | FastPin<DATA_PIN>::mask();
			lo = *port & ~FastPin<DATA_PIN>::mask();
			#endif

			// Load bytes into fixed buffer (3 for RGB, 4 for RGBW)
			fl::vector_fixed<uint8_t, 4> bytes;
			if (is_rgbw) {
				uint8_t b0, b1, b2, b3;
				pixels.loadAndScaleRGBW(rgbw, &b0, &b1, &b2, &b3);
				bytes.push_back(b0);
				bytes.push_back(b1);
				bytes.push_back(b2);
				bytes.push_back(b3);
			} else {
				bytes.push_back(pixels.loadAndScale0());
				bytes.push_back(pixels.loadAndScale1());
				bytes.push_back(pixels.loadAndScale2());
			}

			// Write all bytes
			for (fl::size i = 0; i < bytes.size(); ++i) {
				writeBits<8+XTRA0>(next_mark, port, hi, lo, bytes[i]);
			}

			pixels.advanceData();
			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			sei();
			#endif
		}

		#if (FASTLED_ALLOW_INTERRUPTS == 0)
		// Only need final sei() if interrupts weren't re-enabled in the loop
		sei();
		#endif
		return ARM_DWT_CYCCNT;
	}
};
#endif
}  // namespace fl
#endif

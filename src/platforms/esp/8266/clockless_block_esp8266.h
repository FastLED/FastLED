#ifndef __INC_CLOCKLESS_BLOCK_ESP8266_H
#define __INC_CLOCKLESS_BLOCK_ESP8266_H

#include "fl/stl/stdint.h"
#include "fl/register.h"
#include "fl/math_macros.h"
#include "fl/fastpin.h"
#include "eorder.h"
#include "transpose8x1_noinline.h"
#include "fl/chipsets/timing_traits.h"
#include "fastled_delay.h"
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

#define FASTLED_HAS_BLOCKLESS 1

#define FIX_BITS(bits) (((bits & 0x0fL) << 12) | (bits & 0x30))

#define USED_LANES (FL_MIN(LANES, 6))
#define PORT_MASK (((1 << USED_LANES)-1) & 0x0000FFFFL)
#define PIN_MASK FIX_BITS(PORT_MASK)
namespace fl {
#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
extern u32 _frame_cnt;
extern u32 _retry_cnt;
#endif

template <u8 LANES, int FIRST_PIN, typename TIMING, EOrder RGB_ORDER = GRB, int XTRA0 = 0, bool FLIP = false>
class InlineBlockClocklessController : public CPixelLEDController<RGB_ORDER, LANES, PORT_MASK> {
	typedef typename FastPin<FIRST_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<FIRST_PIN>::port_t data_t;

	enum : u32 {
		T1 = TIMING::T1,
		T2 = TIMING::T2,
		T3 = TIMING::T3,
		WAIT_TIME = TIMING::RESET
	};

	CMinWait<WAIT_TIME> mWait;

public:
	virtual int size() { return CLEDController::size() * LANES; }

	virtual void showPixels(PixelController<RGB_ORDER, LANES, PORT_MASK> & pixels) {
		mWait.wait();
		/*u32 clocks = */
		int cnt=FASTLED_INTERRUPT_RETRY_COUNT;
		while(!showRGBInternal(pixels) && cnt--) {
      		os_intr_unlock();
			#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
			++_retry_cnt;
			#endif
			delayMicroseconds(WAIT_TIME * 10);
			os_intr_lock();
		}
		// #if FASTLED_ALLOW_INTTERUPTS == 0
		// Adjust the timer
		// long microsTaken = CLKS_TO_MICROS(clocks);
		// MS_COUNTER += (1 + (microsTaken / 1000));
		// #endif

		mWait.mark();
	}

	template<int PIN> static void initPin() {
		_ESPPIN<PIN, 1<<(PIN & 0xFF)>::setOutput();
	}

	virtual void init() {
		void (* funcs[])() ={initPin<12>, initPin<13>, initPin<14>, initPin<15>, initPin<4>, initPin<5>};

		for (u8 i = 0; i < USED_LANES; ++i) {
			funcs[i]();
		}
	}

	virtual u16 getMaxRefreshRate() const { return 400; }

	typedef union {
		u8 bytes[8];
		u16 shorts[4];
		u32 raw[2];
	} Lines;

#define ESP_ADJUST 0 // (2*(F_CPU/24000000))
#define ESP_ADJUST2 0
  	template<int BITS,int PX> __attribute__ ((always_inline)) inline static void writeBits(FASTLED_REGISTER u32 & last_mark, FASTLED_REGISTER Lines & b, PixelController<RGB_ORDER, LANES, PORT_MASK> &pixels) { // , FASTLED_REGISTER uint32_t & b2)  {
	  	Lines b2 = b;
		transpose8x1_noinline(b.bytes,b2.bytes);

		FASTLED_REGISTER u8 d = pixels.template getd<PX>(pixels);
		FASTLED_REGISTER u8 scale = pixels.template getscale<PX>(pixels);

		for(FASTLED_REGISTER u32 i = 0; i < USED_LANES; ++i) {
			while((__clock_cycles() - last_mark) < (T1+T2+T3));
			last_mark = __clock_cycles();
			*FastPin<FIRST_PIN>::sport() = PIN_MASK;

			u32 nword = (u32)(~b2.bytes[7-i]);
			while((__clock_cycles() - last_mark) < (T1-6));
			*FastPin<FIRST_PIN>::cport() = FIX_BITS(nword);

			while((__clock_cycles() - last_mark) < (T1+T2));
			*FastPin<FIRST_PIN>::cport() = PIN_MASK;

			b.bytes[i] = pixels.template loadAndScale<PX>(pixels,i,d,scale);
		}

		for(FASTLED_REGISTER u32 i = USED_LANES; i < 8; ++i) {
			while((__clock_cycles() - last_mark) < (T1+T2+T3));
			last_mark = __clock_cycles();
			*FastPin<FIRST_PIN>::sport() = PIN_MASK;

			u32 nword = (u32)(~b2.bytes[7-i]);
			while((__clock_cycles() - last_mark) < (T1-6));
			*FastPin<FIRST_PIN>::cport() = FIX_BITS(nword);

			while((__clock_cycles() - last_mark) < (T1+T2));
			*FastPin<FIRST_PIN>::cport() = PIN_MASK;
		}
	}

  	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static u32 FL_IRAM showRGBInternal(PixelController<RGB_ORDER, LANES, PORT_MASK> &allpixels) {

		// Setup the pixel controller and load/scale the first byte
		Lines b0;

		for(int i = 0; i < USED_LANES; ++i) {
			b0.bytes[i] = allpixels.loadAndScale0(i);
		}
		allpixels.preStepFirstByteDithering();

		os_intr_lock();
		u32 _start = __clock_cycles();
		u32 last_mark = _start;

		while(allpixels.has(1)) {
			// Write first byte, read next byte
			writeBits<8+XTRA0,1>(last_mark, b0, allpixels);

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0,2>(last_mark, b0, allpixels);
			allpixels.advanceData();

			// Write third byte
			writeBits<8+XTRA0,0>(last_mark, b0, allpixels);

		#if (FASTLED_ALLOW_INTERRUPTS == 1)
			os_intr_unlock();
		#endif

			allpixels.stepDithering();

		#if (FASTLED_ALLOW_INTERRUPTS == 1)
			os_intr_lock();
			// if interrupts took longer than 45µs, punt on the current frame
			if((i32)(__clock_cycles()-last_mark) > 0) {
				if((i32)(__clock_cycles()-last_mark) > (T1+T2+T3+((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US))) { os_intr_unlock(); return 0; }
			}
		#endif
		};

		os_intr_unlock();
		#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
		++_frame_cnt;
		#endif
		return __clock_cycles() - _start;
	}
};
}  // namespace fl

FL_DISABLE_WARNING_POP

#endif

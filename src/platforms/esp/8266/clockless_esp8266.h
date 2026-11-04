#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "eorder.h"
#include "fl/stl/compiler_control.h"
#include "fl/chipsets/timing_traits.h"
#include "fastled_delay.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/system/delay.h"
#include "platforms/esp/8266/clockless_esp8266_bits.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
namespace fl {

// Info on reading cycle counter from https://github.com/kbeckmann/nodemcu-firmware/blob/ws2812-dual/app/modules/ws2812.c
__attribute__ ((always_inline)) inline static u32 __clock_cycles() FL_NO_EXCEPT {
  u32 cyc;
  __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc));
  return cyc;
}

#define FL_CLOCKLESS_CONTROLLER_DEFINED 1

#ifndef FASTLED_ESP8266_CLOCKLESS_WAIT_TIME
#define FASTLED_ESP8266_CLOCKLESS_WAIT_TIME 85
#endif

template <int DATA_PIN, typename TIMING, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = FASTLED_ESP8266_CLOCKLESS_WAIT_TIME>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

	// Convert nanoseconds to CPU cycles (compile-time)
	// Formula: cycles = (ns * (F_CPU / 1MHz) + 500) / 1000
	// +500 for rounding to nearest cycle
	static constexpr u32 NS_TO_CYCLES(u32 ns) FL_NO_EXCEPT {
		return (ns * (F_CPU / 1000000UL) + 500) / 1000;
	}

	static constexpr u32 T1 = NS_TO_CYCLES(TIMING::T1);  // Convert nanoseconds → CPU cycles
	static constexpr u32 T2 = NS_TO_CYCLES(TIMING::T2);
	static constexpr u32 T3 = NS_TO_CYCLES(TIMING::T3);

	data_t mPinMask;
	data_ptr_t mPort;
	CMinWait<WAIT_TIME> mWait;
public:
	virtual void init() FL_NO_EXCEPT {
		FastPin<DATA_PIN>::setOutput();
		mPinMask = FastPin<DATA_PIN>::mask();
		mPort = FastPin<DATA_PIN>::port();
	}

	virtual u16 getMaxRefreshRate() const FL_NO_EXCEPT { return 400; }

protected:

	virtual void showPixels(PixelController<RGB_ORDER> & pixels) FL_NO_EXCEPT {
    mWait.wait();
		int cnt = FASTLED_INTERRUPT_RETRY_COUNT;
    while((showRGBInternal(pixels)==0) && cnt--) {
      #ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
      ++_retry_cnt;
      #endif
      fl::delayMicroseconds(WAIT_TIME);
    }
    mWait.mark();
  }

	static u32 showRGBInternal(PixelController<RGB_ORDER> pixels) FL_NO_EXCEPT {
		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		FASTLED_REGISTER u32 b = pixels.loadAndScale0();
		pixels.preStepFirstByteDithering();
		u32 start;
		
		// This function has multiple exits, so we'll use an object
		// with a destructor that releases the interrupt lock, regardless
		// of how we exit the function.  It also has methods for manually
		// unlocking and relocking interrupts temporarily.
		struct InterruptLock {
			InterruptLock() FL_NO_EXCEPT {
				os_intr_lock();
			}
			~InterruptLock() {
				os_intr_unlock();
			}
			void Unlock() FL_NO_EXCEPT {
				os_intr_unlock();
			}
			void Lock() FL_NO_EXCEPT {
				os_intr_lock();
			}
		};

		{ // Start of interrupt-locked block
			InterruptLock intlock;

			start = __clock_cycles();
			u32 last_mark = start;
			const esp8266_clockless::BitTiming timing = {T1, T2, T3};
			while(pixels.has(1)) {
				// Load the pixel before its first bit; the timed bits then run
				// entirely from IRAM.
				const u8 bytes[3] = {static_cast<u8>(b),
				                     pixels.loadAndScale1(),
				                     pixels.loadAndScale2()};
				if (!esp8266_clockless::sendPixel(last_mark, bytes, 8 + XTRA0,
				                                  DATA_PIN, timing)) {
					return 0;
				}

				#if (FASTLED_ALLOW_INTERRUPTS == 1)
				intlock.Unlock();
				#endif

				b = pixels.advanceAndLoadAndScale0();
				pixels.stepDithering();

				#if (FASTLED_ALLOW_INTERRUPTS == 1)
				intlock.Lock();
				// if interrupts took longer than 45µs, punt on the current frame
				if((i32)(__clock_cycles()-last_mark) > 0) {
					if((i32)(__clock_cycles()-last_mark) > (i32)(T1+T2+T3+((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US))) {
						return 0;
					}
				}
				#endif
			};
		}  // End of interrupt-locked block

    #ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
    ++_frame_cnt;
    #endif
		return __clock_cycles() - start;
	}
};
}  // namespace fl

FL_DISABLE_WARNING_POP

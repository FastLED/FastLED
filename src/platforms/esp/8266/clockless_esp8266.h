#pragma once

#include <stdint.h>
#include "eorder.h"
#include "fl/namespace.h"
#include "fl/register.h"

FASTLED_NAMESPACE_BEGIN

#ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
extern uint32_t _frame_cnt;
extern uint32_t _retry_cnt;
#endif

// Info on reading cycle counter from https://github.com/kbeckmann/nodemcu-firmware/blob/ws2812-dual/app/modules/ws2812.c
__attribute__ ((always_inline)) inline static uint32_t __clock_cycles() {
  uint32_t cyc;
  __asm__ __volatile__ ("rsr %0,ccount":"=a" (cyc));
  return cyc;
}

#define FASTLED_HAS_CLOCKLESS 1

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 85>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
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
		int cnt = FASTLED_INTERRUPT_RETRY_COUNT;
    while((showRGBInternal(pixels)==0) && cnt--) {
      #ifdef FASTLED_DEBUG_COUNT_FRAME_RETRIES
      ++_retry_cnt;
      #endif
      delayMicroseconds(WAIT_TIME);
    }
    mWait.mark();
  }

#define _ESP_ADJ (0)
#define _ESP_ADJ2 (0)

	template<int BITS> __attribute__ ((always_inline)) inline static bool writeBits(FASTLED_REGISTER uint32_t & last_mark, FASTLED_REGISTER uint32_t b)  {
    b <<= 24; b = ~b;
    for(FASTLED_REGISTER uint32_t i = BITS; i > 0; --i) {
      while((__clock_cycles() - last_mark) < (T1+T2+T3)) {
            ;
      }
      last_mark = __clock_cycles();
      FastPin<DATA_PIN>::hi();

      while((__clock_cycles() - last_mark) < T1) {
            ;
      }
      if(b & 0x80000000L) { FastPin<DATA_PIN>::lo(); }
      b <<= 1;

      while((__clock_cycles() - last_mark) < (T1+T2)) {
            ;
      }
      FastPin<DATA_PIN>::lo();

			// even with interrupts disabled, the NMI interupt seems to cause
			// timing issues here. abort the frame if one bit took to long. if the
			// last of the 24 bits has been sent already, it is too late
			// this fixes the flickering first pixel that started to occur with
			// framework version 3.0.0
			if ((__clock_cycles() - last_mark) >= (T1 + T2 + T3 - 5)) {
				return true;
			}
		}
		return false;
	}


	static uint32_t IRAM_ATTR showRGBInternal(PixelController<RGB_ORDER> pixels) {
		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		FASTLED_REGISTER uint32_t b = pixels.loadAndScale0();
		pixels.preStepFirstByteDithering();
		uint32_t start;
		
		// This function has multiple exits, so we'll use an object
		// with a destructor that releases the interrupt lock, regardless
		// of how we exit the function.  It also has methods for manually
		// unlocking and relocking interrupts temporarily.
		struct InterruptLock {
			InterruptLock() {
				os_intr_lock();
			}
			~InterruptLock() {
				os_intr_unlock();
			}
			void Unlock() {
				os_intr_unlock();
			}
			void Lock() {
				os_intr_lock();
			}
		};

		{ // Start of interrupt-locked block
			InterruptLock intlock;

			start = __clock_cycles();
			uint32_t last_mark = start;
			while(pixels.has(1)) {
				// Write first byte, read next byte
				if (writeBits<8+XTRA0>(last_mark, b)) {
					return 0;
				}
				b = pixels.loadAndScale1();

				// Write second byte, read 3rd byte
				if (writeBits<8+XTRA0>(last_mark, b)) {
					return 0;
				}
				b = pixels.loadAndScale2();

				// Write third byte, read 1st byte of next pixel
				if (writeBits<8+XTRA0>(last_mark, b)) {
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
				if((int32_t)(__clock_cycles()-last_mark) > 0) {
					if((int32_t)(__clock_cycles()-last_mark) > (T1+T2+T3+((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US))) {
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

FASTLED_NAMESPACE_END

#ifndef __INC_CLOCKLESS_APOLLO3_H
#define __INC_CLOCKLESS_APOLLO3_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_APOLLO3)

// Clockless support for the SparkFun Artemis / Ambiq Micro Apollo3 Blue
// Uses SysTick to govern the pulse timing

//*****************************************************************************
//
// Code taken from Ambiq Micro's am_hal_systick.c
// and converted to inline static for speed
//
//! @brief Get the current count value in the SYSTICK.
//!
//! This function gets the current count value in the systick timer.
//!
//! @return Current count value.
//
//*****************************************************************************
__attribute__ ((always_inline)) inline static uint32_t __am_hal_systick_count() {
	return SysTick->VAL;
}

#define FASTLED_HAS_CLOCKLESS 1

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

  	CMinWait<WAIT_TIME> mWait;

public:
	virtual void init() {
		// Initialize everything

		// Configure DATA_PIN for FastGPIO (settings are in fastpin_apollo3.h)
		FastPin<DATA_PIN>::setOutput();
		FastPin<DATA_PIN>::lo();

		// Make sure the system clock is running at the full 48MHz
		am_hal_clkgen_control(AM_HAL_CLKGEN_CONTROL_SYSCLK_MAX, 0);

		// Make sure interrupts are enabled
		//am_hal_interrupt_master_enable();

		// Enable SysTick Interrupts in the NVIC
		//NVIC_EnableIRQ(SysTick_IRQn);

		// SysTick is 24-bit and counts down (not up)

		// Stop the SysTick (just in case it is already running).
		// This clears the ENABLE bit in the SysTick Control and Status Register (SYST_CSR).
		// In Ambiq naming convention, the control register is SysTick->CTRL
		am_hal_systick_stop();

		// Call SysTick_Config
		// This is defined in core_cm4.h
		// It loads the specified LOAD value into the SysTick Reload Value Register (SYST_RVR)
		// In Ambiq naming convention, the reload register is SysTick->LOAD
		// It sets the SysTick interrupt priority
		// It clears the SysTick Current Value Register (SYST_CVR)
		// In Ambiq naming convention, the current value register is SysTick->VAL
		// Finally it sets these bits in the SysTick Control and Status Register (SYST_CSR):
		// CLKSOURCE: SysTick uses the processor clock
		// TICKINT: When the count reaches zero, the SysTick exception (interrupt) is changed to pending
		// ENABLE: Enables the counter
		// SysTick_Config returns 0 if successful. 1 indicates a failure (the LOAD value was invalid).
		SysTick_Config(0xFFFFFFUL); // The LOAD value needs to be 24-bit
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

	template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(FASTLED_REGISTER uint32_t & next_mark, FASTLED_REGISTER uint8_t & b)  {
		// SysTick counts down (not up) and is 24-bit
		for(FASTLED_REGISTER uint32_t i = BITS-1; i > 0; i--) { // We could speed this up by using Bit Banding
			while(__am_hal_systick_count() > next_mark) { ; } // Wait for the remainder of this cycle to complete
				// Calculate next_mark (the time of the next DATA_PIN transition) by subtracting T1+T2+T3
				// SysTick counts down (not up) and is 24-bit
				next_mark = (__am_hal_systick_count() - (T1+T2+T3)) & 0xFFFFFFUL;
				FastPin<DATA_PIN>::hi();
				if(b&0x80) {
					// "1 code" = longer pulse width
					while((__am_hal_systick_count() - next_mark) > (T3+(3*(F_CPU/24000000)))) { ; }
					FastPin<DATA_PIN>::lo();
				} else {
					// "0 code" = shorter pulse width
					while((__am_hal_systick_count() - next_mark) > (T2+T3+(4*(F_CPU/24000000)))) { ; }
					FastPin<DATA_PIN>::lo();
				}
				b <<= 1;
		}

		while(__am_hal_systick_count() > next_mark) { ; }// Wait for the remainder of this cycle to complete
		// Calculate next_mark (the time of the next DATA_PIN transition) by subtracting T1+T2+T3
		// SysTick counts down (not up) and is 24-bit
		next_mark = (__am_hal_systick_count() - (T1+T2+T3)) & 0xFFFFFFUL;
		FastPin<DATA_PIN>::hi();
		if(b&0x80) {
			// "1 code" = longer pulse width
			while((__am_hal_systick_count() - next_mark) > (T3+(2*(F_CPU/24000000)))) { ; }
			FastPin<DATA_PIN>::lo();
		} else {
			// "0 code" = shorter pulse width
			while((__am_hal_systick_count() - next_mark) > (T2+T3+(4*(F_CPU/24000000)))) { ; }
			FastPin<DATA_PIN>::lo();
		}
	}

	// This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
	// gcc will use register Y for the this pointer.
	static uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {

		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		FASTLED_REGISTER uint8_t b = pixels.loadAndScale0();

		cli();

		// Calculate next_mark (the time of the next DATA_PIN transition) by subtracting T1+T2+T3
		// SysTick counts down (not up) and is 24-bit
		// The subtraction could underflow (wrap round) so let's mask the result to 24 bits
		FASTLED_REGISTER uint32_t next_mark = (__am_hal_systick_count() - (T1+T2+T3)) & 0xFFFFFFUL;

		while(pixels.has(1)) { // Keep going for as long as we have pixels
			pixels.stepDithering();

			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			cli();

			// Have we already missed the next_mark?
			if(__am_hal_systick_count() < next_mark) {
				// If we have exceeded next_mark by an excessive amount, then bail (return 0)
				if((next_mark - __am_hal_systick_count()) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return 0; }
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
		}; // end of while(pixels.has(1))

		// Unfortunately SysTick relies on an interrupt to reload it once it reaches zero
		// and having interrupts disabled for most of the above means the interrupt doesn't get serviced.
		// So we had better reload it here instead...
		am_hal_systick_load(0xFFFFFFUL);

		sei();
		return (1);
	}

};


#endif

FASTLED_NAMESPACE_END

#endif

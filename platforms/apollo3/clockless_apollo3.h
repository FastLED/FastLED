#ifndef __INC_CLOCKLESS_APOLLO3_H
#define __INC_CLOCKLESS_APOLLO3_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_APOLLO3)

#include "ap3_analog.h"

#define FASTLED_HAS_CLOCKLESS 1

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
	typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
	typedef typename FastPin<DATA_PIN>::port_t data_t;

  CMinWait<WAIT_TIME> mWait;

public:
	virtual void init() {
		FastPin<DATA_PIN>::setOutput();
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

	template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(register uint8_t & b)  {
    uint32_t clk = AM_HAL_CTIMER_HFRC_12MHZ;
    uint32_t fw = 0;
    uint32_t th = 0;
    fw = (T1+T2+T3) * 10000 / 12;

		for(register uint32_t i = BITS-1; i > 0; i--) {
			if(b&0x80) {
        th = T3 * 10000 / 12;
			} else {
        th = (T2 + T3) * 1000 / 12;
			}
      ap3_pwm_output_once(DATA_PIN, th, fw, clk);
			b <<= 1;
		}

    if(b&0x80) {
      th = T3 * 10000 / 12;
    } else {
      th = (T2 + T3) * 10000 / 12;
    }
    ap3_pwm_output_once(DATA_PIN, th, fw, clk);
	}

	static uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
		// Setup the pixel controller and load/scale the first byte
		pixels.preStepFirstByteDithering();
		register uint8_t b = pixels.loadAndScale0();

		cli();

		while(pixels.has(1)) {
			pixels.stepDithering();

			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			cli();
			#endif

			// Write first byte, read next byte
			writeBits<8+XTRA0>(b);
			b = pixels.loadAndScale1();

			// Write second byte, read 3rd byte
			writeBits<8+XTRA0>(b);
			b = pixels.loadAndScale2();

			// Write third byte, read 1st byte of next pixel
			writeBits<8+XTRA0>(b);
			b = pixels.advanceAndLoadAndScale0();

			#if (FASTLED_ALLOW_INTERRUPTS == 1)
			sei();
			#endif
		};

		sei();
		return 1;
	}

  static bool ap3_pwm_is_running(uint32_t ui32TimerNumber, uint32_t ui32TimerSegment)
  {
    volatile uint32_t *pui32ConfigReg;
    bool is_enabled = false;

    //
    // Find the correct control register.
    //
    pui32ConfigReg = (uint32_t *)CTIMERADDRn(CTIMER, ui32TimerNumber, CTRL0);

    //
    // Begin critical section while config registers are read and modified.
    //
    AM_CRITICAL_BEGIN

    //
    // Read the current value.
    //
    uint32_t ui32ConfigVal = *pui32ConfigReg;

    //
    // Check the "enable bit"
    //
    if (ui32ConfigVal & (CTIMER_CTRL0_TMRA0EN_Msk | CTIMER_CTRL0_TMRB0EN_Msk))
    {
        is_enabled = true;
    }

    //
    // Done with critical section.
    //
    AM_CRITICAL_END

    return is_enabled;
  }

  static void ap3_pwm_wait_for_pulse(uint32_t timer, uint32_t segment, uint32_t output, uint32_t margin)
  {

      volatile uint32_t *pui32CompareReg;
      volatile uint32_t ctimer_val;
      uint32_t cmpr0;

      // Only wait if the ctimer is running to avoid a deadlock
      if (ap3_pwm_is_running(timer, segment))
      {

          // Get the comapre register address
          if (segment == AM_HAL_CTIMER_TIMERA)
          {
              if (output == AM_HAL_CTIMER_OUTPUT_NORMAL)
              {
                  pui32CompareReg = (uint32_t *)CTIMERADDRn(CTIMER, timer, CMPRA0);
              }
              else
              {
                  pui32CompareReg = (uint32_t *)CTIMERADDRn(CTIMER, timer, CMPRAUXA0);
              }
          }
          else
          {
              if (output == AM_HAL_CTIMER_OUTPUT_NORMAL)
              {
                  pui32CompareReg = (uint32_t *)CTIMERADDRn(CTIMER, timer, CMPRB0);
              }
              else
              {
                  pui32CompareReg = (uint32_t *)CTIMERADDRn(CTIMER, timer, CMPRAUXB0);
              }
          }

          // Get the compare value
          cmpr0 = ((uint32_t)(*(pui32CompareReg)) & 0x0000FFFF);

          if (cmpr0)
          { // Only wait when cmpr0 is greater than 0 to avoid an infinite while loop
              // Wait for the timer value to be less than the compare value so that it is safe to change
              ctimer_val = am_hal_ctimer_read(timer, segment);
              while ((ctimer_val + 0) >= cmpr0)
              {
                  ctimer_val = am_hal_ctimer_read(timer, segment);
              }
          }
      }
  }

  #define CTXPADNUM(ctx) ((CTx_tbl[ctx] >> 0) & 0x3f)
  #define CTXPADFNC(ctx) ((CTx_tbl[ctx] >> 8) & 0x7)
  #define CTX(pad, fn) ((fn << 8) | (pad << 0))
  #define OUTC(timB, timN, N2) ((N2 << 4) | (timB << 3) | (timN << 0))
  #define OUTCTIMN(ctx, n) (outcfg_tbl[ctx][n] & (0x7 << 0))
  #define OUTCTIMB(ctx, n) (outcfg_tbl[ctx][n] & (0x1 << 3))
  #define OUTCO2(ctx, n) (outcfg_tbl[ctx][n] & (0x1 << 4))

  static ap3_err_t ap3_pwm_output_once(uint8_t pin, uint32_t th, uint32_t fw, uint32_t clk)
  {
    static const uint16_t CTx_tbl[32] =
    {
            CTX(12, 2), CTX(25, 2), CTX(13, 2), CTX(26, 2), CTX(18, 2), // 0 - 4
            CTX(27, 2), CTX(19, 2), CTX(28, 2), CTX(5, 7), CTX(29, 2),  // 5 - 9
            CTX(6, 5), CTX(30, 2), CTX(22, 2), CTX(31, 2), CTX(23, 2),  // 10 - 14
            CTX(32, 2), CTX(42, 2), CTX(4, 6), CTX(43, 2), CTX(7, 7),   // 15 - 19
            CTX(44, 2), CTX(24, 5), CTX(45, 2), CTX(33, 6), CTX(46, 2), // 20 - 24
            CTX(39, 2), CTX(47, 2), CTX(35, 5), CTX(48, 2), CTX(37, 7), // 25 - 29
            CTX(49, 2), CTX(11, 2)                                      // 30 - 31
    };

    static const uint8_t outcfg_tbl[32][4] =
        {
            {OUTC(0, 0, 0), OUTC(1, 2, 1), OUTC(0, 5, 1), OUTC(0, 6, 0)}, // CTX0:  A0OUT,  B2OUT2, A5OUT2, A6OUT
            {OUTC(0, 0, 1), OUTC(0, 0, 0), OUTC(0, 5, 0), OUTC(1, 7, 1)}, // CTX1:  A0OUT2, A0OUT,  A5OUT,  B7OUT2
            {OUTC(1, 0, 0), OUTC(1, 1, 1), OUTC(1, 6, 1), OUTC(0, 7, 0)}, // CTX2:  B0OUT,  B1OUT2, B6OUT2, A7OUT
            {OUTC(1, 0, 1), OUTC(1, 0, 0), OUTC(0, 1, 0), OUTC(0, 6, 0)}, // CTX3:  B0OUT2, B0OUT,  A1OUT,  A6OUT
            {OUTC(0, 1, 0), OUTC(0, 2, 1), OUTC(0, 5, 1), OUTC(1, 5, 0)}, // CTX4:  A1OUT,  A2OUT2, A5OUT2, B5OUT
            {OUTC(0, 1, 1), OUTC(0, 1, 0), OUTC(1, 6, 0), OUTC(0, 7, 0)}, // CTX5:  A1OUT2, A1OUT,  B6OUT,  A7OUT
            {OUTC(1, 1, 0), OUTC(0, 1, 0), OUTC(1, 5, 1), OUTC(1, 7, 0)}, // CTX6:  B1OUT,  A1OUT,  B5OUT2, B7OUT
            {OUTC(1, 1, 1), OUTC(1, 1, 0), OUTC(1, 5, 0), OUTC(0, 7, 0)}, // CTX7:  B1OUT2, B1OUT,  B5OUT,  A7OUT
            {OUTC(0, 2, 0), OUTC(0, 3, 1), OUTC(0, 4, 1), OUTC(1, 6, 0)}, // CTX8:  A2OUT,  A3OUT2, A4OUT2, B6OUT
            {OUTC(0, 2, 1), OUTC(0, 2, 0), OUTC(0, 4, 0), OUTC(1, 0, 0)}, // CTX9:  A2OUT2, A2OUT,  A4OUT,  B0OUT
            {OUTC(1, 2, 0), OUTC(1, 3, 1), OUTC(1, 4, 1), OUTC(0, 6, 0)}, // CTX10: B2OUT,  B3OUT2, B4OUT2, A6OUT
            {OUTC(1, 2, 1), OUTC(1, 2, 0), OUTC(1, 4, 0), OUTC(1, 5, 1)}, // CTX11: B2OUT2, B2OUT,  B4OUT,  B5OUT2
            {OUTC(0, 3, 0), OUTC(1, 1, 0), OUTC(1, 0, 1), OUTC(1, 6, 1)}, // CTX12: A3OUT,  B1OUT,  B0OUT2, B6OUT2
            {OUTC(0, 3, 1), OUTC(0, 3, 0), OUTC(0, 6, 0), OUTC(1, 4, 1)}, // CTX13: A3OUT2, A3OUT,  A6OUT,  B4OUT2
            {OUTC(1, 3, 0), OUTC(1, 1, 0), OUTC(1, 7, 1), OUTC(0, 7, 0)}, // CTX14: B3OUT,  B1OUT,  B7OUT2, A7OUT
            {OUTC(1, 3, 1), OUTC(1, 3, 0), OUTC(0, 7, 0), OUTC(0, 4, 1)}, // CTX15: B3OUT2, B3OUT,  A7OUT,  A4OUT2
            {OUTC(0, 4, 0), OUTC(0, 0, 0), OUTC(0, 0, 1), OUTC(1, 3, 1)}, // CTX16: A4OUT,  A0OUT,  A0OUT2, B3OUT2
            {OUTC(0, 4, 1), OUTC(1, 7, 0), OUTC(0, 4, 0), OUTC(0, 1, 1)}, // CTX17: A4OUT2, B7OUT,  A4OUT,  A1OUT2
            {OUTC(1, 4, 0), OUTC(1, 0, 0), OUTC(0, 0, 0), OUTC(0, 3, 1)}, // CTX18: B4OUT,  B0OUT,  A0OUT,  A3OUT2
            {OUTC(1, 4, 1), OUTC(0, 2, 0), OUTC(1, 4, 0), OUTC(1, 1, 1)}, // CTX19: B4OUT2, A2OUT,  B4OUT,  B1OUT2
            {OUTC(0, 5, 0), OUTC(0, 1, 0), OUTC(0, 1, 1), OUTC(1, 2, 1)}, // CTX20: A5OUT,  A1OUT,  A1OUT2, B2OUT2
            {OUTC(0, 5, 1), OUTC(0, 1, 0), OUTC(1, 5, 0), OUTC(0, 0, 1)}, // CTX21: A5OUT2, A1OUT,  B5OUT,  A0OUT2
            {OUTC(1, 5, 0), OUTC(0, 6, 0), OUTC(0, 1, 0), OUTC(0, 2, 1)}, // CTX22: B5OUT,  A6OUT,  A1OUT,  A2OUT2
            {OUTC(1, 5, 1), OUTC(0, 7, 0), OUTC(0, 5, 0), OUTC(1, 0, 1)}, // CTX23: B5OUT2, A7OUT,  A5OUT,  B0OUT2
            {OUTC(0, 6, 0), OUTC(0, 2, 0), OUTC(0, 1, 0), OUTC(1, 1, 1)}, // CTX24: A6OUT,  A2OUT,  A1OUT,  B1OUT2
            {OUTC(1, 4, 1), OUTC(1, 2, 0), OUTC(0, 6, 0), OUTC(0, 2, 1)}, // CTX25: B4OUT2, B2OUT,  A6OUT,  A2OUT2
            {OUTC(1, 6, 0), OUTC(1, 2, 0), OUTC(0, 5, 0), OUTC(0, 1, 1)}, // CTX26: B6OUT,  B2OUT,  A5OUT,  A1OUT2
            {OUTC(1, 6, 1), OUTC(0, 1, 0), OUTC(1, 6, 0), OUTC(1, 2, 1)}, // CTX27: B6OUT2, A1OUT,  B6OUT,  B2OUT2
            {OUTC(0, 7, 0), OUTC(0, 3, 0), OUTC(0, 5, 1), OUTC(1, 0, 1)}, // CTX28: A7OUT,  A3OUT,  A5OUT2, B0OUT2
            {OUTC(1, 5, 1), OUTC(0, 1, 0), OUTC(0, 7, 0), OUTC(0, 3, 1)}, // CTX29: B5OUT2, A1OUT,  A7OUT,  A3OUT2
            {OUTC(1, 7, 0), OUTC(1, 3, 0), OUTC(0, 4, 1), OUTC(0, 0, 1)}, // CTX30: B7OUT,  B3OUT,  A4OUT2, A0OUT2
            {OUTC(1, 7, 1), OUTC(0, 6, 0), OUTC(1, 7, 0), OUTC(1, 3, 1)}, // CTX31: B7OUT2, A6OUT,  B7OUT,  B3OUT2
    };

      // handle configuration, if necessary
      ap3_err_t retval = AP3_OK;

      if (fw > 0)
      { // reduce fw so that the user's desired value is the period
          fw--;
      }

      ap3_gpio_pad_t pad = ap3_gpio_pin2pad(pin);
      if ((pad == AP3_GPIO_PAD_UNUSED) || (pad >= AP3_GPIO_MAX_PADS))
      {
          return AP3_INVALID_ARG;
      }

      uint32_t timer = 0;
      uint32_t segment = AM_HAL_CTIMER_TIMERA;
      uint32_t output = AM_HAL_CTIMER_OUTPUT_NORMAL;

      uint8_t ctx = 0;
      for (ctx = 0; ctx < 32; ctx++)
      {
          if (CTXPADNUM(ctx) == pad)
          {
              break;
          }
      }
      if (ctx >= 32)
      {
          return AP3_ERR; // could not find pad in CTx table
      }
      // Now use CTx index to get configuration information

      // Now, for the given pad, determine the above values
      if ((pad == 39) || (pad == 37))
      {
          // pads 39 and 37 must be handled differently to avoid conflicting with other pins
          if (pad == 39)
          {
              // 39
              timer = 6;
              segment = AM_HAL_CTIMER_TIMERA;
              output = AM_HAL_CTIMER_OUTPUT_SECONDARY;
          }
          else
          {
              // 37
              timer = 7;
              segment = AM_HAL_CTIMER_TIMERA;
              output = AM_HAL_CTIMER_OUTPUT_SECONDARY;
          }
      }
      else
      { // Use the 0th index of the outcfg_tbl to select the functions
          timer = OUTCTIMN(ctx, 0);
          if (OUTCTIMB(ctx, 0))
          {
              segment = AM_HAL_CTIMER_TIMERB;
          }
          if (OUTCO2(ctx, 0))
          {
              output = AM_HAL_CTIMER_OUTPUT_SECONDARY;
          }
      }

      // Ensure that th is not greater than the fw
      if (th > fw)
      {
          th = fw;
      }

      // Test for AM_HAL_CTIMER_OUTPUT_FORCE0 or AM_HAL_CTIMER_OUTPUT_FORCE1
      bool set_periods = true;
      if ((th == 0) || (fw == 0))
      {
          output = AM_HAL_CTIMER_OUTPUT_FORCE0;
          set_periods = false; // disable setting periods when going into a forced mode
      }
      else if (th == fw)
      {
          output = AM_HAL_CTIMER_OUTPUT_FORCE1;
          set_periods = false; // disable setting periods when going into a forced mode
      }

      // Wait until after high pulse to change the state (avoids inversion)
      ap3_pwm_wait_for_pulse(timer, segment, output, 10);

      // Configure the pin
      am_hal_ctimer_output_config(timer,
                                  segment,
                                  pad,
                                  output,
                                  AM_HAL_GPIO_PIN_DRIVESTRENGTH_12MA); //

      // Configure the pulse mode with our clock source
      am_hal_ctimer_config_single(timer,
                                  segment,
                                  // (AM_HAL_CTIMER_FN_PWM_REPEAT | AP3_ANALOG_CLK | AM_HAL_CTIMER_INT_ENABLE) );
                                  (AM_HAL_CTIMER_FN_PWM_ONCE | clk));

      if (set_periods)
      {
          // If this pad uses secondary output:
          if (output == AM_HAL_CTIMER_OUTPUT_SECONDARY)
          {
              // Need to explicitly enable compare registers 2/3
              uint32_t *pui32ConfigReg = NULL;
              pui32ConfigReg = (uint32_t *)CTIMERADDRn(CTIMER, timer, AUX0);
              uint32_t ui32WriteVal = AM_REGVAL(pui32ConfigReg);
              uint32_t ui32ConfigVal = (1 << CTIMER_AUX0_TMRA0EN23_Pos); // using CTIMER_AUX0_TMRA0EN23_Pos because for now this number is common to all CTimer instances
              if (segment == AM_HAL_CTIMER_TIMERB)
              {
                  ui32ConfigVal = ((ui32ConfigVal & 0xFFFF) << 16);
              }
              ui32WriteVal = (ui32WriteVal & ~(segment)) | ui32ConfigVal;
              AM_REGVAL(pui32ConfigReg) = ui32WriteVal;

              // then set the duty cycle with the 'aux' function
              am_hal_ctimer_aux_period_set(timer, segment, fw, th);
          }
          else
          {
              // Otherwise simply set the primary duty cycle
              am_hal_ctimer_period_set(timer, segment, fw, th);
          }

          am_hal_ctimer_start(timer, segment); // Start the timer only when there are periods to compare to
      }

      return AP3_OK;
  }
};


#endif

FASTLED_NAMESPACE_END

#endif

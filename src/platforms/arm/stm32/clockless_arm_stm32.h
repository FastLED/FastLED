#ifndef __INC_CLOCKLESS_ARM_STM32_H
#define __INC_CLOCKLESS_ARM_STM32_H

FASTLED_NAMESPACE_BEGIN
// Definition for a single channel clockless controller for the stm32 family of chips, like that used in the spark core
// See clockless.h for detailed info on how the template parameters are used.

#define FASTLED_HAS_CLOCKLESS 1

#if defined(STM32F2XX)
// The photon runs faster than the others
#define ADJ 8
#else
#define ADJ 20
#endif

template <int DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
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
        if(!showRGBInternal(pixels)) {
            sei(); delayMicroseconds(WAIT_TIME); cli();
            showRGBInternal(pixels);
        }
        mWait.mark();
    }

#define _CYCCNT (*(volatile uint32_t*)(0xE0001004UL))

    template<int BITS> __attribute__ ((always_inline)) inline static void writeBits(REGISTER uint32_t & next_mark, REGISTER data_ptr_t port, REGISTER data_t hi, REGISTER data_t lo, REGISTER uint8_t & b)  {
        for(REGISTER uint32_t i = BITS-1; i > 0; --i) {
            while(_CYCCNT < (T1+T2+T3-ADJ));
            FastPin<DATA_PIN>::fastset(port, hi);
            _CYCCNT = 4;
            if(b&0x80) {
                while(_CYCCNT < (T1+T2-ADJ));
                FastPin<DATA_PIN>::fastset(port, lo);
            } else {
                while(_CYCCNT < (T1-ADJ/2));
                FastPin<DATA_PIN>::fastset(port, lo);
            }
            b <<= 1;
        }

        while(_CYCCNT < (T1+T2+T3-ADJ));
        FastPin<DATA_PIN>::fastset(port, hi);
        _CYCCNT = 4;

        if(b&0x80) {
            while(_CYCCNT < (T1+T2-ADJ));
            FastPin<DATA_PIN>::fastset(port, lo);
        } else {
            while(_CYCCNT < (T1-ADJ/2));
            FastPin<DATA_PIN>::fastset(port, lo);
        }
    }

    // This method is made static to force making register Y available to use for data on AVR - if the method is non-static, then
    // gcc will use register Y for the this pointer.
    static uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
        // Get access to the clock
        CoreDebug->DEMCR  |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
        DWT->CYCCNT = 0;

        REGISTER data_ptr_t port = FastPin<DATA_PIN>::port();
        REGISTER data_t hi = *port | FastPin<DATA_PIN>::mask();;
        REGISTER data_t lo = *port & ~FastPin<DATA_PIN>::mask();;
        *port = lo;

        // Setup the pixel controller and load/scale the first byte
        pixels.preStepFirstByteDithering();
        REGISTER uint8_t b = pixels.loadAndScale0();

        cli();

        uint32_t next_mark = (T1+T2+T3);

        DWT->CYCCNT = 0;
        while(pixels.has(1)) {
            pixels.stepDithering();
            #if (FASTLED_ALLOW_INTERRUPTS == 1)
            cli();
            // if interrupts took longer than 45µs, punt on the current frame
            if(DWT->CYCCNT > next_mark) {
                if((DWT->CYCCNT-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return 0; }
            }

            hi = *port | FastPin<DATA_PIN>::mask();
            lo = *port & ~FastPin<DATA_PIN>::mask();
            #endif

            // Write first byte, read next byte
            writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
            b = pixels.loadAndScale1();

            // Write second byte, read 3rd byte
            writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
            b = pixels.loadAndScale2();

            // Write third byte, read 1st byte of next pixel
            writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
            b = pixels.advanceAndLoadAndScale0();
            #if (FASTLED_ALLOW_INTERRUPTS == 1)
            sei();
            #endif
        };

        sei();
        return DWT->CYCCNT;
    }
};

FASTLED_NAMESPACE_END

#endif

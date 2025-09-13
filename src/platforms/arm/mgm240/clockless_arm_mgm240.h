#pragma once

// Include required FastLED headers
#include "fl/namespace.h"
#include "eorder.h"
#include "fastled_delay.h"

FASTLED_NAMESPACE_BEGIN

// Enable clockless LED support for MGM240
#define FASTLED_HAS_CLOCKLESS 1

// For now, use a simple fallback clockless implementation
// This won't be optimal for timing but will allow compilation
// TODO: Implement proper Cortex-M33 DWT-based timing controller

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
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

    // Use simple bit-bang timing for now
    // TODO: Implement proper ARM DWT cycle-accurate timing
    static uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
        if(pixels.size() == 0) {
            return 1; // success
        }

        cli(); // Disable interrupts for timing-critical section

        while(pixels.has(1)) {
            uint8_t b0 = pixels.loadAndScale0();
            uint8_t b1 = pixels.loadAndScale1();
            uint8_t b2 = pixels.loadAndScale2();

            // Send bits MSB first - WS2812 protocol
            for(int i = 7; i >= 0; i--) {
                writeBit((b0 >> i) & 1);
            }
            for(int i = 7; i >= 0; i--) {
                writeBit((b1 >> i) & 1);
            }
            for(int i = 7; i >= 0; i--) {
                writeBit((b2 >> i) & 1);
            }

            pixels.advanceData();
            pixels.stepDithering();
        }

        sei(); // Re-enable interrupts
        return 1; // success
    }

private:
    static void writeBit(uint8_t bit) {
        // WS2812 timing: T0H=400ns, T0L=850ns, T1H=800ns, T1L=450ns
        // At 39MHz, each cycle is ~25.6ns
        // T0H ≈ 15 cycles, T1H ≈ 31 cycles, T0L ≈ 33 cycles, T1L ≈ 17 cycles

        FastPin<DATA_PIN>::hi();
        if(bit) {
            // '1' bit: longer high time (~800ns)
            for(volatile int i = 0; i < 8; i++) { __asm__("nop"); }
        } else {
            // '0' bit: shorter high time (~400ns)
            for(volatile int i = 0; i < 4; i++) { __asm__("nop"); }
        }
        FastPin<DATA_PIN>::lo();

        // Low time varies inversely with high time
        if(bit) {
            // '1' bit: shorter low time (~450ns)
            for(volatile int i = 0; i < 4; i++) { __asm__("nop"); }
        } else {
            // '0' bit: longer low time (~850ns)
            for(volatile int i = 0; i < 8; i++) { __asm__("nop"); }
        }
    }
};

FASTLED_NAMESPACE_END
#pragma once

// Include required FastLED headers
#include "fl/namespace.h"
#include "eorder.h"
#include "fastled_delay.h"

FASTLED_NAMESPACE_BEGIN

// ARM Cortex-M33 DWT (Data Watchpoint and Trace) registers for cycle-accurate timing
#define ARM_DEMCR               (*(volatile uint32_t *)0xE000EDFC) // Debug Exception and Monitor Control
#define ARM_DEMCR_TRCENA                (1 << 24)        // Enable debugging & monitoring blocks
#define ARM_DWT_CTRL            (*(volatile uint32_t *)0xE0001000) // DWT control register
#define ARM_DWT_CTRL_CYCCNTENA          (1 << 0)                // Enable cycle count
#define ARM_DWT_CYCCNT          (*(volatile uint32_t *)0xE0001004) // Cycle count register

// Enable clockless LED support for MGM240
#define FASTLED_HAS_CLOCKLESS 1

/// @brief ARM Cortex-M33 clockless LED controller for MGM240
///
/// This implementation provides cycle-accurate timing for driving clockless LEDs
/// using the ARM DWT (Data Watchpoint and Trace) unit. The controller is generic
/// and supports any LED chipset through the T1/T2/T3 timing parameters.
///
/// Key features:
/// - DWT-based cycle-accurate timing (no compiler-dependent delays)
/// - FreeRTOS task scheduler safety
/// - Atomic GPIO operations using Silicon Labs DOUTSET/DOUTCLR registers
/// - Support for all FastLED chipsets: WS2812, SK6812, WS2815, etc.
/// - Interrupt-aware operation with configurable thresholds
///
/// @tparam DATA_PIN Arduino pin number for LED data line
/// @tparam T1 High time for '1' bit in CPU cycles
/// @tparam T2 High time for '0' bit in CPU cycles
/// @tparam T3 Low time for both bits in CPU cycles
/// @tparam RGB_ORDER Color ordering (e.g., GRB for WS2812)
/// @tparam XTRA0 Extra bits per color channel (0-4 typically)
///   Adds additional bits beyond the standard 8 bits per color channel.
///   Used by some LED chipsets or for timing adjustments:
///   - 0: Standard 8 bits per channel (most chipsets: WS2812, SK6812)
///   - 1-4: Extra bits for special chipsets or timing fine-tuning
///   Total bits per channel = 8 + XTRA0
/// @tparam FLIP Bit order flip flag
/// @tparam WAIT_TIME Minimum wait time between updates (microseconds)

template <uint8_t DATA_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = RGB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class ClocklessController : public CPixelLEDController<RGB_ORDER> {
    typedef typename FastPin<DATA_PIN>::port_ptr_t data_ptr_t;
    typedef typename FastPin<DATA_PIN>::port_t data_t;

    data_t mPinMask;
    data_ptr_t mPort;
    CMinWait<WAIT_TIME> mWait;

public:
    /// @brief Initialize the LED controller
    /// Sets up the data pin as output and caches pin mask and port address
    virtual void init() {
        FastPin<DATA_PIN>::setOutput();
        mPinMask = FastPin<DATA_PIN>::mask();
        mPort = FastPin<DATA_PIN>::port();
    }

    /// @brief Get maximum refresh rate in Hz
    /// @return Maximum safe refresh rate (400 Hz for most applications)
    virtual uint16_t getMaxRefreshRate() const { return 400; }

protected:
    /// @brief Output pixel data to LED strip
    /// @param pixels Pixel controller containing RGB data and scaling
    virtual void showPixels(PixelController<RGB_ORDER> & pixels) {
        mWait.wait();
        if(!showRGBInternal(pixels)) {
            // If timing was interrupted, wait and retry once
            sei(); delayMicroseconds(WAIT_TIME); cli();
            showRGBInternal(pixels);
        }
        mWait.mark();
    }

    /// @brief Write multiple bits using cycle-accurate timing
    /// @tparam BITS Number of bits to write (typically 8+XTRA0)
    ///   When XTRA0 > 0, writes additional bits beyond the standard 8 per channel.
    ///   Extra bits are sent as '0' bits for timing or protocol requirements.
    /// @param next_mark Reference to next timing mark (DWT cycle count)
    /// @param port GPIO port pointer for fast bit manipulation
    /// @param hi Port value with data pin high
    /// @param lo Port value with data pin low
    /// @param b Reference to byte containing bits to send (MSB first)
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

    /// @brief Internal RGB data output with DWT cycle-accurate timing
    /// @param pixels Pixel controller with RGB data
    /// @return DWT cycle count when completed (0 if interrupted)
    static uint32_t showRGBInternal(PixelController<RGB_ORDER> pixels) {
        // Enable ARM DWT cycle counter for precise timing
        ARM_DEMCR    |= ARM_DEMCR_TRCENA;
        ARM_DWT_CTRL |= ARM_DWT_CTRL_CYCCNTENA;
        ARM_DWT_CYCCNT = 0;

        FASTLED_REGISTER data_ptr_t port = FastPin<DATA_PIN>::port();
        FASTLED_REGISTER data_t hi = *port | FastPin<DATA_PIN>::mask();
        FASTLED_REGISTER data_t lo = *port & ~FastPin<DATA_PIN>::mask();
        *port = lo;

        // Setup the pixel controller and load/scale the first byte
        pixels.preStepFirstByteDithering();
        FASTLED_REGISTER uint8_t b = pixels.loadAndScale0();

        cli();
        uint32_t next_mark = ARM_DWT_CYCCNT + (T1+T2+T3);

        while(pixels.has(1)) {
            pixels.stepDithering();
            #if (FASTLED_ALLOW_INTERRUPTS == 1)
            cli();
            // if interrupts took longer than 45µs, punt on the current frame
            if(ARM_DWT_CYCCNT > next_mark) {
                if((ARM_DWT_CYCCNT-next_mark) > ((WAIT_TIME-INTERRUPT_THRESHOLD)*CLKS_PER_US)) { sei(); return 0; }
            }

            hi = *port | FastPin<DATA_PIN>::mask();
            lo = *port & ~FastPin<DATA_PIN>::mask();
            #endif
            // Write first byte (R/G/B + XTRA0 extra bits), read next byte
            writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
            b = pixels.loadAndScale1();

            // Write second byte (R/G/B + XTRA0 extra bits), read 3rd byte
            writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
            b = pixels.loadAndScale2();

            // Write third byte (R/G/B + XTRA0 extra bits), read 1st byte of next pixel
            writeBits<8+XTRA0>(next_mark, port, hi, lo, b);
            b = pixels.advanceAndLoadAndScale0();
            #if (FASTLED_ALLOW_INTERRUPTS == 1)
            sei();
            #endif
        };

        sei();
        return ARM_DWT_CYCCNT;
    }

};

FASTLED_NAMESPACE_END
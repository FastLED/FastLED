#ifndef __INC_BLOCK_CLOCKLESS_ARM_MXRT1062_H
#define __INC_BLOCK_CLOCKLESS_ARM_MXRT1062_H

FASTLED_NAMESPACE_BEGIN

// Definition for a single channel clockless controller for the teensy4
// See clockless.h for detailed info on how the template parameters are used.
#if defined(FASTLED_TEENSY4)

#define __FL_T4_MASK ((1<<(LANES))-1)
template <uint8_t LANES, int FIRST_PIN, int T1, int T2, int T3, EOrder RGB_ORDER = GRB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 50>
class FlexibleInlineBlockClocklessController : public CPixelLEDController<RGB_ORDER, LANES, __FL_T4_MASK> {
    uint8_t m_bitOffsets[16];
    uint8_t m_nActualLanes;
    uint8_t m_nLowBit;
    uint8_t m_nHighBit;
    uint32_t m_nWriteMask;
    uint8_t m_nOutBlocks;
    uint32_t m_offsets[3];
    CMinWait<WAIT_TIME> mWait;

public:
    virtual int size() { return CLEDController::size() * m_nActualLanes; }

    // For each pin, if we've hit our lane count, break, otherwise set the pin to output,
    // store the bit offset in our offset array, add this pin to the write mask, and if this
    // pin ends a block sequence, then break out of the switch as well
    #define _BLOCK_PIN(P) case P: {                             \
        if(m_nActualLanes == LANES) break;                      \
        FastPin<P>::setOutput();                                \
        m_bitOffsets[m_nActualLanes++] = FastPin<P>::pinbit();  \
        m_nWriteMask |= FastPin<P>::mask();                     \
        if( P == 27 || P == 7 || P == 30) break;                \
    }

    virtual void init() {
        // pre-initialize
        memset(m_bitOffsets,0,16);
        m_nActualLanes = 0;
        m_nLowBit = 33;
        m_nHighBit = 0;
        m_nWriteMask = 0;

        // setup the bits and data tracking for parallel output
        switch(FIRST_PIN) {
            // GPIO6 block output
            _BLOCK_PIN( 1);
            _BLOCK_PIN( 0);
            _BLOCK_PIN(24);
            _BLOCK_PIN(25);
            _BLOCK_PIN(19);
            _BLOCK_PIN(18);
            _BLOCK_PIN(14);
            _BLOCK_PIN(15);
            _BLOCK_PIN(17);
            _BLOCK_PIN(16);
            _BLOCK_PIN(22);
            _BLOCK_PIN(23);
            _BLOCK_PIN(20);
            _BLOCK_PIN(21);
            _BLOCK_PIN(26);
            _BLOCK_PIN(27);
            // GPIO7 block output
            _BLOCK_PIN(10);
            _BLOCK_PIN(12);
            _BLOCK_PIN(11);
            _BLOCK_PIN(13);
            _BLOCK_PIN( 6);
            _BLOCK_PIN( 9);
            _BLOCK_PIN(32);
            _BLOCK_PIN( 8);
            _BLOCK_PIN( 7);
            // GPIO 37 block output
            _BLOCK_PIN(37);
            _BLOCK_PIN(36);
            _BLOCK_PIN(35);
            _BLOCK_PIN(34);
            _BLOCK_PIN(39);
            _BLOCK_PIN(38);
            _BLOCK_PIN(28);
            _BLOCK_PIN(31);
            _BLOCK_PIN(30);
        }

        for(int i = 0; i < m_nActualLanes; ++i) {
            if(m_bitOffsets[i] < m_nLowBit) { m_nLowBit = m_bitOffsets[i]; }
            if(m_bitOffsets[i] > m_nHighBit) { m_nHighBit = m_bitOffsets[i]; }
        }

        m_nOutBlocks = (m_nHighBit + 8)/8;

    }

    virtual uint16_t getMaxRefreshRate() const { return 400; }

    virtual void showPixels(PixelController<RGB_ORDER, LANES, __FL_T4_MASK> & pixels) {
        mWait.wait();
    #if FASTLED_ALLOW_INTERRUPTS == 0
        uint32_t clocks = showRGBInternal(pixels);
        // Adjust the timer
        long microsTaken = CLKS_TO_MICROS(clocks);
        MS_COUNTER += (1 + (microsTaken / 1000));
    #else
        showRGBInternal(pixels);
    #endif
		mWait.mark();
	}

  typedef union {
    uint8_t bytes[32];
    uint8_t bg[4][8];
    uint16_t shorts[16];
    uint32_t raw[8];
  } _outlines;


  template<int BITS,int PX> __attribute__ ((always_inline)) inline void writeBits(register uint32_t & next_mark, register _outlines & b, PixelController<RGB_ORDER, LANES, __FL_T4_MASK> &pixels) {
        _outlines b2;
        transpose8x1(b.bg[3], b2.bg[3]);
        transpose8x1(b.bg[2], b2.bg[2]);
        transpose8x1(b.bg[1], b2.bg[1]);
        transpose8x1(b.bg[0], b2.bg[0]);

        register uint8_t d = pixels.template getd<PX>(pixels);
        register uint8_t scale = pixels.template getscale<PX>(pixels);

        int x = 0;
        for(uint32_t i = 8; i > 0;) {
            --i;
            while(ARM_DWT_CYCCNT < next_mark);
            *FastPin<FIRST_PIN>::sport() = m_nWriteMask;
            next_mark = ARM_DWT_CYCCNT + m_offsets[0];

            uint32_t out = (b2.bg[3][i] << 24) | (b2.bg[2][i] << 16) | (b2.bg[1][i] << 8) | b2.bg[0][i];

            out = ((~out) & m_nWriteMask);
            while((next_mark - ARM_DWT_CYCCNT) > m_offsets[1]);
            *FastPin<FIRST_PIN>::cport() = out;

            out = m_nWriteMask;
            while((next_mark - ARM_DWT_CYCCNT) > m_offsets[2]);
            *FastPin<FIRST_PIN>::cport() = out;

            // Read and store up to two bytes
            if (x < m_nActualLanes) {
                b.bytes[m_bitOffsets[x]] = pixels.template loadAndScale<PX>(pixels, x, d, scale);
                ++x;
                if (x < m_nActualLanes) {
                    b.bytes[m_bitOffsets[x]] = pixels.template loadAndScale<PX>(pixels, x, d, scale);
                    ++x;
                }
            }
        }
    }

    uint32_t showRGBInternal(PixelController<RGB_ORDER,LANES, __FL_T4_MASK> &allpixels) {
        allpixels.preStepFirstByteDithering();
        _outlines b0;
        uint32_t start = ARM_DWT_CYCCNT;

        for(int i = 0; i < m_nActualLanes; ++i) {
            b0.bytes[m_bitOffsets[i]] = allpixels.loadAndScale0(i);
        }

        cli();

        m_offsets[0] = _FASTLED_NS_TO_DWT(T1+T2+T3);
        m_offsets[1] = _FASTLED_NS_TO_DWT(T2+T3);
        m_offsets[2] = _FASTLED_NS_TO_DWT(T3);
        uint32_t wait_off = _FASTLED_NS_TO_DWT((WAIT_TIME-INTERRUPT_THRESHOLD));

        uint32_t next_mark = ARM_DWT_CYCCNT + m_offsets[0];

        while(allpixels.has(1)) {
            allpixels.stepDithering();
        #if (FASTLED_ALLOW_INTERRUPTS == 1)
            cli();
            // if interrupts took longer than 45µs, punt on the current frame
            if(ARM_DWT_CYCCNT > next_mark) {
                if((ARM_DWT_CYCCNT-next_mark) > wait_off) { sei(); return ARM_DWT_CYCCNT - start; }
            }
        #endif
            // Write first byte, read next byte
            writeBits<8+XTRA0,1>(next_mark, b0, allpixels);

            // Write second byte, read 3rd byte
            writeBits<8+XTRA0,2>(next_mark, b0, allpixels);
            allpixels.advanceData();

            // Write third byte
            writeBits<8+XTRA0,0>(next_mark, b0, allpixels);
        #if (FASTLED_ALLOW_INTERRUPTS == 1)
            sei();
        #endif
        }

        sei();

        return ARM_DWT_CYCCNT - start;
    }
};

template<template<uint8_t DATA_PIN, EOrder RGB_ORDER> class CHIPSET, uint8_t DATA_PIN, int NUM_LANES, EOrder RGB_ORDER=GRB>
class __FIBCC : public FlexibleInlineBlockClocklessController<NUM_LANES,DATA_PIN,CHIPSET<DATA_PIN,RGB_ORDER>::__T1(),CHIPSET<DATA_PIN,RGB_ORDER>::__T2(),CHIPSET<DATA_PIN,RGB_ORDER>::__T3(),RGB_ORDER,CHIPSET<DATA_PIN,RGB_ORDER>::__XTRA0(),CHIPSET<DATA_PIN,RGB_ORDER>::__FLIP(),CHIPSET<DATA_PIN,RGB_ORDER>::__WAIT_TIME()> {};

#define __FASTLED_HAS_FIBCC 1

#endif //defined(FASTLED_TEENSY4)

FASTLED_NAMESPACE_END

#endif

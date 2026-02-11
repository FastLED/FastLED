#ifndef __INC_BLOCK_CLOCKLESS_ARM_MXRT1062_H
#define __INC_BLOCK_CLOCKLESS_ARM_MXRT1062_H

#include "fl/chipsets/timing_traits.h"
#include "fastled_delay.h"
#include "platforms/arm/teensy/is_teensy.h"

namespace fl {
// Definition for a single channel clockless controller for the teensy4
// See clockless.h for detailed info on how the template parameters are used.

#if defined(FL_IS_TEENSY_4X)

#define __FL_T4_MASK ((1<<(LANES))-1)
template <u8 LANES, int FIRST_PIN, typename TIMING, EOrder RGB_ORDER = GRB, int XTRA0 = 0, bool FLIP = false, int WAIT_TIME = 280>
class FlexibleInlineBlockClocklessController : public CPixelLEDController<RGB_ORDER, LANES, __FL_T4_MASK> {
    static constexpr u32 T1 = TIMING::T1;
    static constexpr u32 T2 = TIMING::T2;
    static constexpr u32 T3 = TIMING::T3;
    u8 m_bitOffsets[16];
    u8 m_nActualLanes;
    u8 m_nLowBit;
    u8 m_nHighBit;
    u32 m_nWriteMask;
    u8 m_nOutBlocks;
    u32 m_offsets[3];
    u32 MS_COUNTER;
    CMinWait<WAIT_TIME> mWait;

public:
    virtual int size() { return CLEDController::size() * m_nActualLanes; }

    // For each pin, if we've hit our lane count, break, otherwise set the pin to output,
    // store the bit offset in our offset array, add this pin to the write mask, and if this
    // pin ends a block sequence, then break out of the switch as well
    #define _BLOCK_PIN(P) case P: {                             \
        if(m_nActualLanes == LANES) break;                      \
        fl::FastPin<P>::setOutput();                            \
        m_bitOffsets[m_nActualLanes++] = fl::FastPin<P>::pinbit();  \
        m_nWriteMask |= fl::FastPin<P>::mask();                 \
        if( P == 27 || P == 7 || P == 30) break;                \
    }

    virtual void init() {
        // pre-initialize
        fl::memset(m_bitOffsets,0,16);
        m_nActualLanes = 0;
        m_nLowBit = 33;
        m_nHighBit = 0;
        m_nWriteMask = 0;
	MS_COUNTER = 0;

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

    virtual u16 getMaxRefreshRate() const { return 400; }

    virtual void showPixels(PixelController<RGB_ORDER, LANES, __FL_T4_MASK> & pixels) {
        mWait.wait();
    #if FASTLED_ALLOW_INTERRUPTS == 0
        u32 clocks = showRGBInternal(pixels);
        // Adjust the timer
        long microsTaken = CLKS_TO_MICROS(clocks);
        MS_COUNTER += (1 + (microsTaken / 1000));
    #else
        showRGBInternal(pixels);
    #endif
		mWait.mark();
	}

  typedef union {
    u8 bytes[32];
    u8 bg[4][8];
    u16 shorts[16];
    u32 raw[8];
  } _outlines;


  template<int BITS,int PX> __attribute__ ((always_inline)) inline void writeBits(FASTLED_REGISTER u32 & next_mark, FASTLED_REGISTER _outlines & b, PixelController<RGB_ORDER, LANES, __FL_T4_MASK> &pixels) {
        _outlines b2;
        transpose8x1(b.bg[3], b2.bg[3]);
        transpose8x1(b.bg[2], b2.bg[2]);
        transpose8x1(b.bg[1], b2.bg[1]);
        transpose8x1(b.bg[0], b2.bg[0]);

        FASTLED_REGISTER u8 d = pixels.template getd<PX>(pixels);
        FASTLED_REGISTER u8 scale = pixels.template getscale<PX>(pixels);

        int x = 0;
        for(u32 i = 8; i > 0;) {
            --i;
            while(ARM_DWT_CYCCNT < next_mark);
            *fl::FastPin<FIRST_PIN>::sport() = m_nWriteMask;
            next_mark = ARM_DWT_CYCCNT + m_offsets[0];

            u32 out = (b2.bg[3][i] << 24) | (b2.bg[2][i] << 16) | (b2.bg[1][i] << 8) | b2.bg[0][i];

            out = ((~out) & m_nWriteMask);
            while((next_mark - ARM_DWT_CYCCNT) > m_offsets[1]);
            *fl::FastPin<FIRST_PIN>::cport() = out;

            out = m_nWriteMask;
            while((next_mark - ARM_DWT_CYCCNT) > m_offsets[2]);
            *fl::FastPin<FIRST_PIN>::cport() = out;

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

    u32 showRGBInternal(PixelController<RGB_ORDER,LANES, __FL_T4_MASK> &allpixels) {
        allpixels.preStepFirstByteDithering();
        _outlines b0;
        u32 start = ARM_DWT_CYCCNT;

        for(int i = 0; i < m_nActualLanes; ++i) {
            b0.bytes[m_bitOffsets[i]] = allpixels.loadAndScale0(i);
        }

        cli();

        m_offsets[0] = _FASTLED_NS_TO_DWT(T1+T2+T3);
        m_offsets[1] = _FASTLED_NS_TO_DWT(T2+T3);
        m_offsets[2] = _FASTLED_NS_TO_DWT(T3);
        u32 wait_off = _FASTLED_NS_TO_DWT((WAIT_TIME-INTERRUPT_THRESHOLD));

        u32 next_mark = ARM_DWT_CYCCNT + m_offsets[0];

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

template<template<u8 DATA_PIN, fl::EOrder RGB_ORDER> class CHIPSET, u8 DATA_PIN, int NUM_LANES, fl::EOrder RGB_ORDER=GRB>
class __FIBCC : public FlexibleInlineBlockClocklessController<NUM_LANES,DATA_PIN,typename CHIPSET<DATA_PIN,RGB_ORDER>::__TIMING,RGB_ORDER,CHIPSET<DATA_PIN,RGB_ORDER>::__XTRA0(),CHIPSET<DATA_PIN,RGB_ORDER>::__FLIP(),CHIPSET<DATA_PIN,RGB_ORDER>::__WAIT_TIME()> {};

#define __FASTLED_HAS_FIBCC 1

#endif //defined(FASTLED_TEENSY4)
}  // namespace fl
#endif

#ifndef __INC_CONTROLLER_H
#define __INC_CONTROLLER_H

#include "led_sysdefs.h"
#include "pixeltypes.h"

#define RO(X) RGB_BYTE(RGB_ORDER, X)
#define RGB_BYTE(RO,X) (((RO)>>(3*(2-(X)))) & 0x3)

#define RGB_BYTE0(RO) ((RO>>6) & 0x3) 
#define RGB_BYTE1(RO) ((RO>>3) & 0x3) 
#define RGB_BYTE2(RO) ((RO) & 0x3)

// operator byte *(struct CRGB[] arr) { return (byte*)arr; }


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// LED Controller interface definition
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Base definition for an LED controller.  Pretty much the methods that every LED controller object will make available.  
/// Note that the showARGB method is not impelemented for all controllers yet.   Note also the methods for eventual checking
/// of background writing of data (I'm looking at you, teensy 3.0 DMA controller!).  If you want to pass LED controllers around
/// to methods, make them references to this type, keeps your code saner.  However, most people won't be seeing/using these objects
/// directly at all
class CLEDController { 
public:
	// initialize the LED controller
	virtual void init() = 0;

	// reset any internal state to a clean point
	virtual void reset() { init(); } 

	// clear out/zero out the given number of leds.
	virtual void clearLeds(int nLeds) = 0;

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale = CRGB::White) = 0;

	// note that the uint8_ts will be in the order that you want them sent out to the device. 
	// nLeds is the number of RGB leds being written to
	virtual void show(const struct CRGB *data, int nLeds, CRGB scale = CRGB::White) = 0;

#ifdef SUPPORT_ARGB
	// as above, but every 4th uint8_t is assumed to be alpha channel data, and will be skipped
	virtual void show(const struct CARGB *data, int nLeds, CRGB scale = CRGB::White) = 0;
#endif
	
	// is the controller ready to write data out
	virtual bool ready() { return true; }

	// wait until the controller is ready to write data out 
	virtual void wait() { return; }

};

// Pixel controller class.  This is the class that we use to centralize pixel access in a block of data, including
// support for things like RGB reordering, scaling, dithering, skipping (for ARGB data), and eventually, we will 
// centralize 8/12/16 conversions here as well.
template<EOrder RGB_ORDER>
struct PixelController {
        uint8_t d[3];
        uint8_t e[3];
        const uint8_t *mData; 
        CRGB & mScale;
        uint8_t mAdvance;

        PixelController(const uint8_t *d, CRGB & s, bool dodithering, bool doadvance=0, uint8_t skip=0) : mData(d), mScale(s) {
                enable_dithering(dodithering);

                mData += skip;

                mAdvance = 0;
                if(doadvance) { mAdvance = 3 + skip; }
        }

        void init_dithering() {
                static byte R = 0;
                R++;

                // fast reverse bits in a byte
                byte Q = 0;
                if(R & 0x01) { Q |= 0x80; }
                if(R & 0x02) { Q |= 0x40; }
                if(R & 0x04) { Q |= 0x20; }
                if(R & 0x08) { Q |= 0x10; }
                if(R & 0x10) { Q |= 0x08; }
                if(R & 0x20) { Q |= 0x04; }
                if(R & 0x40) { Q |= 0x02; }
                if(R & 0x80) { Q |= 0x01; }

                // setup the seed d and e values
                for(int i = 0; i < 3; i++) {                        byte s = mScale.raw[i];
                        e[i] = s ? (256/s) + 1 : 0;
                        d[i] = scale8(Q, e[i]);
                        if(e[i]) e[i]--;
                }

                d[0] = e[0] - d[0];
        }

        // toggle dithering enable
        void enable_dithering(bool enable) {
                if(enable) { init_dithering(); }
                else { d[0]=d[1]=d[2]=e[0]=e[1]=e[2]=0; }
        }

        // advance the data pointer forward
         __attribute__((always_inline)) inline void advanceData() { mData += mAdvance; }

        // step the dithering forward
         __attribute__((always_inline)) inline void stepDithering() {
                d[0] = e[0] - d[0];
                d[1] = e[1] - d[1];
                d[2] = e[2] - d[2];
        }

        template<int SLOT>  __attribute__((always_inline)) inline static uint8_t loadByte(PixelController & pc) { return pc.mData[RO(SLOT)]; }
        template<int SLOT>  __attribute__((always_inline)) inline static uint8_t dither(PixelController & pc, uint8_t b) { return qadd8(b, pc.d[RO(SLOT)]); }
        template<int SLOT>  __attribute__((always_inline)) inline static uint8_t scale(PixelController & pc, uint8_t b) { return scale8(b, pc.mScale.raw[RO(SLOT)]); }

        // composite shortcut functions for loading, dithering, and scaling
        template<int SLOT>  __attribute__((always_inline)) inline static uint8_t loadAndScale(PixelController & pc) { return scale<SLOT>(pc, pc.dither<SLOT>(pc, pc.loadByte<SLOT>(pc))); }
        template<int SLOT>  __attribute__((always_inline)) inline static uint8_t advanceAndLoadAndScale(PixelController & pc) { pc.advanceData(); return pc.loadAndScale<SLOT>(pc); }

		// Helper functions to get around gcc stupidities
		__attribute__((always_inline)) inline uint8_t loadAndScale0() { return loadAndScale<0>(*this); }    
		__attribute__((always_inline)) inline uint8_t loadAndScale1() { return loadAndScale<1>(*this); }    
		__attribute__((always_inline)) inline uint8_t loadAndScale2() { return loadAndScale<2>(*this); }    
		__attribute__((always_inline)) inline uint8_t advanceAndLoadAndScale0() { return advanceAndLoadAndScale<0>(*this); }    
        __attribute__((always_inline)) inline uint8_t stepAdvanceAndLoadAndScale0() { stepDithering(); return advanceAndLoadAndScale<0>(*this); }    
};

#endif
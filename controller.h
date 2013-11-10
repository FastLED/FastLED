#ifndef __INC_CONTROLLER_H
#define __INC_CONTROLLER_H

#include <avr/io.h>
#include "pixeltypes.h"


#define RGB_BYTE0(X) ((X>>6) & 0x3) 
#define RGB_BYTE1(X) ((X>>3) & 0x3) 
#define RGB_BYTE2(X) ((X) & 0x3)

// operator byte *(struct CRGB[] arr) { return (byte*)arr; }


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// LED Controller interface definition
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Base definition for an LED controller.  Pretty much the methods that every LED controller object will make available.  
/// Note that the showARGB method is not impelemented for all controllers yet.   Note also the methods for eventual checking
/// of background writing of data (I'm looking at you, teensy 3.0 DMA controller!).  If you want to pass LED controllers around
/// to methods, make them references to this type, keeps your code saner.
class CLEDController { 
public:
	// initialize the LED controller
	virtual void init() = 0;

	// reset any internal state to a clean point
	virtual void reset() { init(); } 

	// clear out/zero out the given number of leds.
	virtual void clearLeds(int nLeds) = 0;

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) = 0;

	// note that the uint8_ts will be in the order that you want them sent out to the device. 
	// nLeds is the number of RGB leds being written to
	virtual void show(const struct CRGB *data, int nLeds, uint8_t scale = 255) = 0;

#ifdef SUPPORT_ARGB
	// as above, but every 4th uint8_t is assumed to be alpha channel data, and will be skipped
	virtual void show(const struct CARGB *data, int nLeds, uint8_t scale = 255) = 0;
#endif
	
	// is the controller ready to write data out
	virtual bool ready() { return true; }

	// wait until the controller is ready to write data out 
	virtual void wait() { return; }

};

#endif
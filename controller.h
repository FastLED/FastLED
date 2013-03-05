#ifndef __INC_CONTROLLER_H
#define __INC_CONTROLLER_H

#include <avr/io.h>

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

	// note that the uint8_ts will be in the order that you want them sent out to the device. 
	// nLeds is the number of RGB leds being written to
	virtual void showRGB(uint8_t *data, int nLeds) = 0;

#ifdef SUPPORT_ARGB
	// as above, but every 4th uint8_t is assumed to be alpha channel data, and will be skipped
	virtual void showARGB(uint8_t *data, int nLeds) = 0;
#endif
	
	// is the controller ready to write data out
	virtual bool ready() { return true; }

	// wait until the controller is ready to write data out 
	virtual void wait() { return; }

};

#endif
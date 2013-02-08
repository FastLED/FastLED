#ifndef __INC_CONTROLLER_H
#define __INC_CONTROLLER_H

#include <avr/io.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 
// LED Controller interface definition
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


class CLEDController { 
public:
	// initialize the LED controller
	virtual void init() = 0;

	// reset any internal state to a clean point
	virtual void reset() { init(); } 

	// note that the uint8_ts will be in the order that you want them sent out to the device. 
	// nLeds is the number of RGB leds being written to
	virtual void showRGB(uint8_t *data, int nLeds) = 0;

	// as above, but every 4th uint8_t is assumed to be alpha channel data, and will be skipped
	virtual void showARGB(uint8_t *data, int nLeds) = 0;

	// is the controller ready to write data out
	virtual bool ready() { return true; }

	// wait until the controller is ready to write data out 
	virtual void wait() { return; }
};

#endif
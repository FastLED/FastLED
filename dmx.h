#ifndef __INC_DMX_H
#define __INC_DMX_H

//#ifdef DmxSimple_H
//#if USE_DMX_SIMPLE
#ifdef FASTSPI_USE_DMX_SIMPLE
#include<DmxSimple.h>
// note - dmx simple must be included before FastSPI for this code to be enabled
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB> class DMXController : public CLEDController {
public:
	// initialize the LED controller
	virtual void init() { DmxSimple.usePin(DATA_PIN); }

	// reset any internal state to a clean point
	virtual void reset() { init(); } 

	// clear out/zero out the given number of leds.
	virtual void clearLeds(int nLeds) {
		int count = min(nLeds * 3, DMX_SIZE);
		for(int iChannel = 1; iChannel <= count; iChannel++) { DmxSimple.write(iChannel, 0); }
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		int count = min(nLeds, DMX_SIZE / 3);
		int iChannel = 1;
		for(int i = 0; i < count; i++) {
			DmxSimple.write(iChannel++, scale8(data[RGB_BYTE0(RGB_ORDER)], scale));
			DmxSimple.write(iChannel++, scale8(data[RGB_BYTE1(RGB_ORDER)], scale));
			DmxSimple.write(iChannel++, scale8(data[RGB_BYTE2(RGB_ORDER)], scale));
		}
	}

	// note that the uint8_ts will be in the order that you want them sent out to the device. 
	// nLeds is the number of RGB leds being written to
	virtual void show(const struct CRGB *data, int nLeds, uint8_t scale = 255) {
		int count = min(nLeds, DMX_SIZE / 3);
		int iChannel = 1;
		for(int i = 0; i < count; i++) {
			DmxSimple.write(iChannel++, scale8(data[i][RGB_BYTE0(RGB_ORDER)], scale));
			DmxSimple.write(iChannel++, scale8(data[i][RGB_BYTE1(RGB_ORDER)], scale));
			DmxSimple.write(iChannel++, scale8(data[i][RGB_BYTE2(RGB_ORDER)], scale));
		}

	}

#ifdef SUPPORT_ARGB
	// as above, but every 4th uint8_t is assumed to be alpha channel data, and will be skipped
	virtual void show(const struct CARGB *data, int nLeds, uint8_t scale = 255) = 0;
#endif
	
	// is the controller ready to write data out
	virtual bool ready() { return true; }

	// wait until the controller is ready to write data out 
	virtual void wait() { return; }

};

#elif defined(FASTSPI_USE_DMX_SERIAL)

template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB> class DMXController : public CLEDController {
public:
	// initialize the LED controller
	virtual void init() { DMXSerial.init(DMXController); }

	// reset any internal state to a clean point
	virtual void reset() { init(); } 

	// clear out/zero out the given number of leds.
	virtual void clearLeds(int nLeds) {
		int count = min(nLeds * 3, DMX_SIZE);
		for(int iChannel = 0; iChannel < count; iChannel++) { DmxSimple.write(iChannel, 0); }
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, uint8_t scale = 255) {
		int count = min(nLeds, DMX_SIZE / 3);
		int iChannel = 0;
		for(int i = 0; i < count; i++) {
			DMXSerial.write(iChannel++, scale8(data[RGB_BYTE0(RGB_ORDER)], scale));
			DMXSerial.write(iChannel++, scale8(data[RGB_BYTE1(RGB_ORDER)], scale));
			DMXSerial.write(iChannel++, scale8(data[RGB_BYTE2(RGB_ORDER)], scale));
		}
	}

	// note that the uint8_ts will be in the order that you want them sent out to the device. 
	// nLeds is the number of RGB leds being written to
	virtual void show(const struct CRGB *data, int nLeds, uint8_t scale = 255) {
		int count = min(nLeds, DMX_SIZE / 3);
		int iChannel = 0;
		for(int i = 0; i < count; i++) {
			DMXSerial.write(iChannel++, scale8(data[i][RGB_BYTE0(RGB_ORDER)], scale));
			DMXSerial.write(iChannel++, scale8(data[i][RGB_BYTE1(RGB_ORDER)], scale));
			DMXSerial.write(iChannel++, scale8(data[i][RGB_BYTE2(RGB_ORDER)], scale));
		}

	}

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

#endif
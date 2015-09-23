#ifndef __INC_DMX_H
#define __INC_DMX_H


#ifdef DmxSimple_h
#include<DmxSimple.h>
#define HAS_DMX_SIMPLE

///@ingroup chipsets
///@{
FASTLED_NAMESPACE_BEGIN

// note - dmx simple must be included before FastSPI for this code to be enabled
template <uint8_t DATA_PIN, EOrder RGB_ORDER = RGB> class DMXSimpleController : public CLEDController {
public:
	// initialize the LED controller
	virtual void init() { DmxSimple.usePin(DATA_PIN); }

	// clear out/zero out the given number of leds.
	virtual void clearLeds(int nLeds) {
		int count = min(nLeds * 3, DMX_SIZE);
		for(int iChannel = 1; iChannel <= count; iChannel++) { DmxSimple.write(iChannel, 0); }
	}

protected:
	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) {
		int count = min(nLeds, DMX_SIZE / 3);
		int iChannel = 1;
		for(int i = 0; i < count; i++) {
			DmxSimple.write(iChannel++, scale8(data[RGB_BYTE0(RGB_ORDER)], scale.raw[RGB_BYTE0(RGB_ORDER)]));
			DmxSimple.write(iChannel++, scale8(data[RGB_BYTE1(RGB_ORDER)], scale.raw[RGB_BYTE1(RGB_ORDER)]));
			DmxSimple.write(iChannel++, scale8(data[RGB_BYTE2(RGB_ORDER)], scale.raw[RGB_BYTE2(RGB_ORDER)]));
		}
	}

	// note that the uint8_ts will be in the order that you want them sent out to the device.
	// nLeds is the number of RGB leds being written to
	virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
		int count = min(nLeds, DMX_SIZE / 3);
		int iChannel = 1;
		for(int i = 0; i < count; i++) {
			DmxSimple.write(iChannel++, scale8(data[i][RGB_BYTE0(RGB_ORDER)], scale.raw[RGB_BYTE0(RGB_ORDER)]));
			DmxSimple.write(iChannel++, scale8(data[i][RGB_BYTE1(RGB_ORDER)], scale.raw[RGB_BYTE1(RGB_ORDER)]));
			DmxSimple.write(iChannel++, scale8(data[i][RGB_BYTE2(RGB_ORDER)], scale.raw[RGB_BYTE2(RGB_ORDER)]));
		}

	}

#ifdef SUPPORT_ARGB
	// as above, but every 4th uint8_t is assumed to be alpha channel data, and will be skipped
	virtual void show(const struct CARGB *data, int nLeds, uint8_t scale = 255) = 0;
#endif
};

FASTLED_NAMESPACE_END

#endif

#ifdef DmxSerial_h
#include<DMXSerial.h>

FASTLED_NAMESPACE_BEGIN

template <EOrder RGB_ORDER = RGB> class DMXSerialController : public CLEDController {
public:
	// initialize the LED controller
	virtual void init() { DMXSerial.init(DMXController); }

	// clear out/zero out the given number of leds.
	virtual void clearLeds(int nLeds) {
		int count = min(nLeds * 3, DMXSERIAL_MAX);
		for(int iChannel = 0; iChannel < count; iChannel++) { DMXSerial.write(iChannel, 0); }
	}

	// set all the leds on the controller to a given color
	virtual void showColor(const struct CRGB & data, int nLeds, CRGB scale) {
		int count = min(nLeds, DMXSERIAL_MAX / 3);
		int iChannel = 0;
		for(int i = 0; i < count; i++) {
			DMXSerial.write(iChannel++, scale8(data[RGB_BYTE0(RGB_ORDER)], scale.raw[RGB_BYTE0(RGB_ORDER)]));
			DMXSerial.write(iChannel++, scale8(data[RGB_BYTE1(RGB_ORDER)], scale.raw[RGB_BYTE1(RGB_ORDER)]));
			DMXSerial.write(iChannel++, scale8(data[RGB_BYTE2(RGB_ORDER)], scale.raw[RGB_BYTE2(RGB_ORDER)]));
		}
	}

	// note that the uint8_ts will be in the order that you want them sent out to the device.
	// nLeds is the number of RGB leds being written to
	virtual void show(const struct CRGB *data, int nLeds, CRGB scale) {
		int count = min(nLeds, DMXSERIAL_MAX / 3);
		int iChannel = 0;
		for(int i = 0; i < count; i++) {
			DMXSerial.write(iChannel++, scale8(data[i][RGB_BYTE0(RGB_ORDER)], scale.raw[RGB_BYTE0(RGB_ORDER)]));
			DMXSerial.write(iChannel++, scale8(data[i][RGB_BYTE1(RGB_ORDER)], scale.raw[RGB_BYTE1(RGB_ORDER)]));
			DMXSerial.write(iChannel++, scale8(data[i][RGB_BYTE2(RGB_ORDER)], scale.raw[RGB_BYTE2(RGB_ORDER)]));
		}

	}

#ifdef SUPPORT_ARGB
	// as above, but every 4th uint8_t is assumed to be alpha channel data, and will be skipped
	virtual void show(const struct CARGB *data, int nLeds, uint8_t scale = 255) = 0;
#endif
};

FASTLED_NAMESPACE_END
///@}

#define HAS_DMX_SERIAL
#endif

#endif

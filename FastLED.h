#ifndef __INC_FASTSPI_LED2_H
#define __INC_FASTSPI_LED2_H

#include "controller.h"
#include "fastpin.h"
#include "fastspi.h"
#include "clockless.h"
#include "clockless_trinket.h"
#include "lib8tion.h"
#include "hsv2rgb.h"
#include "chipsets.h"
#include "dmx.h"

enum ESPIChipsets {
	LPD8806,
	WS2801,
	SM16716
};

enum EClocklessChipsets {
	DMX,
	TM1809,
	TM1804,
	TM1803,
	WS2811,
	WS2812,
	WS2812B,
	WS2811_400,
	NEOPIXEL,
	UCS1903
};

#define NUM_CONTROLLERS 8

class CFastLED {
	struct CControllerInfo { 
		CLEDController *pLedController;
		const struct CRGB *pLedData;
		int nLeds;
		int nOffset;
	};

	CControllerInfo	m_Controllers[NUM_CONTROLLERS];
	int m_nControllers;
	uint8_t m_nScale;

public:
	CFastLED();

	CLEDController *addLeds(CLEDController *pLed, const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0);

	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN > CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) { 
		switch(CHIPSET) { 
			case LPD8806: { static LPD8806Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2801: { static WS2801Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SM16716: { static SM16716Controller<DATA_PIN, CLOCK_PIN> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}

	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER > CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) { 
		switch(CHIPSET) { 
			case LPD8806: { static LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2801: { static WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SM16716: { static SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}
	
	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER, uint8_t SPI_DATA_RATE > CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) { 
		switch(CHIPSET) { 
			case LPD8806: { static LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2801: { static WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
			case SM16716: { static SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE> c; return addLeds(&c, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}

#ifdef SPI_DATA
	template<ESPIChipsets CHIPSET> CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) { 
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB>(data, nLedsOrOffset, nLedsIfOffset);
	}	

	template<ESPIChipsets CHIPSET, EOrder RGB_ORDER> CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) { 
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB_ORDER>(data, nLedsOrOffset, nLedsIfOffset);
	}	

	template<ESPIChipsets CHIPSET, EOrder RGB_ORDER, uint8_t SPI_DATA_RATE> CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) { 
		return addLeds<CHIPSET, SPI_DATA, SPI_CLOCK, RGB_ORDER, SPI_DATA_RATE>(data, nLedsOrOffset, nLedsIfOffset);
	}	

#endif

	template<EClocklessChipsets CHIPSET, uint8_t DATA_PIN> 
	CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) { 
#ifdef FASTSPI_USE_DMX_SIMPLE
			case DMX: { static DMXController<DATA_PIN> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
#endif
			case TM1809: { static TM1809Controller800Khz<DATA_PIN> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case TM1803: { static TM1803Controller400Khz<DATA_PIN> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case UCS1903: { static UCS1903Controller400Khz<DATA_PIN> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2812: 
			case WS2812B:
			case WS2811: { static WS2811Controller800Khz<DATA_PIN> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case NEOPIXEL: { static WS2811Controller800Khz<DATA_PIN, GRB> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2811_400: { static WS2811Controller400Khz<DATA_PIN> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}

	template<EClocklessChipsets CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER> 
	CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) { 
#ifdef FASTSPI_USE_DMX_SIMPLE
			case DMX: {static  DMXController<DATA_PIN, RGB_ORDER> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
#endif
			case TM1809: { static TM1809Controller800Khz<DATA_PIN, RGB_ORDER> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case TM1803: { static TM1803Controller400Khz<DATA_PIN, RGB_ORDER> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case UCS1903: { static UCS1903Controller400Khz<DATA_PIN, RGB_ORDER> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2812: 
			case WS2812B:
			case NEOPIXEL:
			case WS2811: { static WS2811Controller800Khz<DATA_PIN, RGB_ORDER> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
			case WS2811_400: { static WS2811Controller400Khz<DATA_PIN, RGB_ORDER> controller; return addLeds(&controller, data, nLedsOrOffset, nLedsIfOffset); }
		}
	}

	void setBrightness(uint8_t scale) { m_nScale = scale; }
	uint8_t getBrightness() { return m_nScale; }

	/// Update all our controllers with the current led colors, using the passed in brightness
	void show(uint8_t scale);

	/// Update all our controllers with the current led colors
	void show() { show(m_nScale); }

	void clear(boolean writeData = false);

	void showColor(const struct CRGB & color, uint8_t scale);

	void showColor(const struct CRGB & color) { showColor(color, m_nScale); }

};

extern CFastLED & FastSPI_LED;
extern CFastLED & FastSPI_LED2;
extern CFastLED & FastLED;
extern CFastLED LEDS;

#endif

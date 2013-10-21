#ifndef __INC_FASTSPI_LED2_H
#define __INC_FASTSPI_LED2_H

#include "controller.h"
#include "fastpin.h"
#include "fastspi.h"
#include "clockless.h"
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

class CFastSPI_LED2 {
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
	CFastSPI_LED2();

	CLEDController *addLeds(CLEDController *pLed, const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0);

	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN > CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) { 
		switch(CHIPSET) { 
			case LPD8806: return addLeds(new LPD8806Controller<DATA_PIN, CLOCK_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
			case WS2801: return addLeds(new WS2801Controller<DATA_PIN, CLOCK_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
			case SM16716: return addLeds(new SM16716Controller<DATA_PIN, CLOCK_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
		}
	}

	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER > CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) { 
		switch(CHIPSET) { 
			case LPD8806: return addLeds(new LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
			case WS2801: return addLeds(new WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
			case SM16716: return addLeds(new SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		}
	}
	
	template<ESPIChipsets CHIPSET,  uint8_t DATA_PIN, uint8_t CLOCK_PIN, EOrder RGB_ORDER, uint8_t SPI_DATA_RATE > CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) { 
		switch(CHIPSET) { 
			case LPD8806: return addLeds(new LPD8806Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE>(), data, nLedsOrOffset, nLedsIfOffset);
			case WS2801: return addLeds(new WS2801Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE>(), data, nLedsOrOffset, nLedsIfOffset);
			case SM16716: return addLeds(new SM16716Controller<DATA_PIN, CLOCK_PIN, RGB_ORDER, SPI_DATA_RATE>(), data, nLedsOrOffset, nLedsIfOffset);
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
			case DMX: return addLeds(new DMXController<DATA_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
#endif
			case TM1804:
			case TM1809: return addLeds(new TM1809Controller800Khz<DATA_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
			case TM1803: return addLeds(new TM1803Controller400Khz<DATA_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
			case UCS1903: return addLeds(new UCS1903Controller400Khz<DATA_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
			case WS2812: 
			case WS2812B:
			case NEOPIXEL:
			case WS2811: return addLeds(new WS2811Controller800Khz<DATA_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
			case WS2811_400: return addLeds(new WS2811Controller400Khz<DATA_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
		}
	}

	template<EClocklessChipsets CHIPSET, uint8_t DATA_PIN, EOrder RGB_ORDER> 
	CLEDController *addLeds(const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0) {
		switch(CHIPSET) { 
#ifdef FASTSPI_USE_DMX_SIMPLE
			case DMX: return addLeds(new DMXController<DATA_PIN, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
#endif
			case TM1809: return addLeds(new TM1809Controller800Khz<DATA_PIN, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
			case TM1803: return addLeds(new TM1803Controller400Khz<DATA_PIN, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
			case UCS1903: return addLeds(new UCS1903Controller400Khz<DATA_PIN, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
			case WS2812: 
			case WS2812B:
			case NEOPIXEL:
			case WS2811: return addLeds(new WS2811Controller800Khz<DATA_PIN, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
			case WS2811_400: return addLeds(new WS2811Controller400Khz<DATA_PIN, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		}
	}

	void setBrightness(uint8_t scale) { m_nScale = scale; }
	uint8_t getBrightness() { return m_nScale; }

	/// Update all our controllers with the current led colors, using the passed in brightness
	void show(uint8_t scale);

	/// Update all our controllers with the current led colors
	void show() { show(m_nScale); }

	void clear(boolean includeLedData = true);

	void showColor(const struct CRGB & color, uint8_t scale);

	void showColor(const struct CRGB & color) { showColor(color, m_nScale); }

};

extern CFastSPI_LED2 & FastSPI_LED;
extern CFastSPI_LED2 & FastSPI_LED2;
extern CFastSPI_LED2 & FastLED;
extern CFastSPI_LED2 LEDS;

#endif

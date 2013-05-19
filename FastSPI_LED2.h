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
	TM1803,
	WS2811,
	UCS1903
};

class CFastSPI_LED2 {
	struct CControllerInfo { 
		CLEDController *pLedController;
		const struct CRGB *pLedData;
		int nLeds;
		int nOffset;
	};

	CControllerInfo	m_Controllers[10];
	int m_nControllers;

public:
	CFastSPI_LED2();

	CLEDController *addLeds(CLEDController *pLed, const struct CRGB *data, int nLedsOrOffset, int nLedsIfOffset = 0);

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
			case TM1809: return addLeds(new TM1809Controller800Khz<DATA_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
			case TM1803: return addLeds(new TM1803Controller400Khz<DATA_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
			case UCS1903: return addLeds(new UCS1903Controller400Khz<DATA_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
			case WS2811: return addLeds(new WS2811Controller800Khz<DATA_PIN>(), data, nLedsOrOffset, nLedsIfOffset);
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
			case WS2811: return addLeds(new WS2811Controller800Khz<DATA_PIN, RGB_ORDER>(), data, nLedsOrOffset, nLedsIfOffset);
		}
	}

	void show(uint8_t scale = 255);

	void clear(boolean includeLedData = true);

	void showColor(const struct CRGB & color, uint8_t scale = 255);
};

extern CFastSPI_LED2 & FastSPI_LED;
extern CFastSPI_LED2 & FastSPI_LED2;
extern CFastSPI_LED2 & FastLED;
extern CFastSPI_LED2 LEDS;

#endif
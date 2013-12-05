#include "FastLED.h"


CFastLED LEDS;
CFastLED & FastSPI_LED = LEDS;
CFastLED & FastSPI_LED2 = LEDS;
CFastLED & FastLED = LEDS;

uint32_t CRGB::Squant = ((uint32_t)((__TIME__[4]-'0') * 28))<<16 | ((__TIME__[6]-'0')*50)<<8 | ((__TIME__[7]-'0')*28);

CFastLED::CFastLED() { 
	// clear out the array of led controllers
	m_nControllers = NUM_CONTROLLERS;
	m_nScale = 255;
	memset8(m_Controllers, 0, m_nControllers * sizeof(CControllerInfo));
}

CLEDController *CFastLED::addLeds(CLEDController *pLed, 
									   const struct CRGB *data, 
									   int nLedsOrOffset, int nLedsIfOffset) { 
	int nOffset = (nLedsIfOffset > 0) ? nLedsOrOffset : 0;
	int nLeds = (nLedsIfOffset > 0) ? nLedsIfOffset : nLedsOrOffset;

	int target = -1;

	// Figure out where to put the new led controller
	for(int i = 0; i < m_nControllers; i++) { 
		if(m_Controllers[i].pLedController == NULL) { 
			target = i;
			break;
		}
	}

	// if we have a spot, use it!
	if(target != -1) {
		m_Controllers[target].pLedController = pLed;
		m_Controllers[target].pLedData = data;
		m_Controllers[target].nOffset = nOffset;
		m_Controllers[target].nLeds = nLeds;
		pLed->init();
		return pLed;
	}
	
	return NULL;
}

void CFastLED::show(uint8_t scale) { 
	for(int i  = 0; i < m_nControllers; i++) { 
		if(m_Controllers[i].pLedController != NULL) { 
			m_Controllers[i].pLedController->show(m_Controllers[i].pLedData + m_Controllers[i].nOffset, 
												  m_Controllers[i].nLeds, scale);
		} else {
			return;
		}
	}
}

void CFastLED::showColor(const struct CRGB & color, uint8_t scale) { 
	for(int i  = 0; i < m_nControllers; i++) { 
		if(m_Controllers[i].pLedController != NULL) { 
			m_Controllers[i].pLedController->showColor(color, m_Controllers[i].nLeds, scale);
		} else { 
			return;
		}
	}
}

void CFastLED::clear(boolean writeData) { 
	if(writeData) { 
		showColor(CRGB(0,0,0), 0);
	}
	for(int i = 0; i < m_nControllers; i++) { 
		if(m_Controllers[i].pLedData != NULL) { 
			memset8((void*)m_Controllers[i].pLedData, 0, sizeof(struct CRGB) * m_Controllers[i].nLeds);
		} else {
			return;
		}
	}
}

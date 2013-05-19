#include "FastSPI_LED2.h"

CFastSPI_LED2 LEDS;
CFastSPI_LED2 & FastSPI_LED = LEDS;
CFastSPI_LED2 & FastSPI_LED2 = LEDS;
CFastSPI_LED2 & FastLED = LEDS;


CFastSPI_LED2::CFastSPI_LED2() { 
	// clear out the array of led controllers
	m_nControllers = 10;
	memset(m_Controllers, 0, m_nControllers * sizeof(CControllerInfo));
}

CLEDController *CFastSPI_LED2::addLeds(CLEDController *pLed, 
									   const struct CRGB *data, 
									   int nLedsOrOffset, int nLedsIfOffset) { 
	int nOffset = (nLedsIfOffset > 0) ? nLedsOrOffset : 0;
	int nLeds = (nLedsIfOffset > 0) ? nLedsIfOffset : nLedsOrOffset;

	for(int i = 0; i < m_nControllers; i++) { 
		if(m_Controllers[i].pLedController == NULL) { 
			m_Controllers[i].pLedController = pLed;
			m_Controllers[i].pLedData = data;
			m_Controllers[i].nOffset = nOffset;
			m_Controllers[i].nLeds = nLeds;
			pLed->init();
			return pLed;
		}
	}

	return NULL;
}

void CFastSPI_LED2::show(uint8_t scale) { 
	for(int i  = 0; i < m_nControllers; i++) { 
		if(m_Controllers[i].pLedController != NULL) { 
			m_Controllers[i].pLedController->show(m_Controllers[i].pLedData + m_Controllers[i].nOffset, 
												  m_Controllers[i].nLeds, scale);
		} else {
			return;
		}
	}
}

void CFastSPI_LED2::showColor(const struct CRGB & color, uint8_t scale) { 
	for(int i  = 0; i < m_nControllers; i++) { 
		if(m_Controllers[i].pLedController != NULL) { 
			m_Controllers[i].pLedController->showColor(color, m_Controllers[i].nLeds, scale);
		} else { 
			return;
		}
	}
}

void CFastSPI_LED2::clear(boolean includeLedData) { 
	showColor(CRGB(0,0,0), 0);
	if(includeLedData) { 
		for(int i = 0; i < m_nControllers; i++) { 
			if(m_Controllers[i].pLedData != NULL) { 
				memset((void*)m_Controllers[i].pLedData, 0, sizeof(struct CRGB) * m_Controllers[i].nLeds);
			} else {
				return;
			}
		}
	}
}
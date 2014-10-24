#include "FastLED.h"

#if defined(__SAM3X8E__)
volatile uint32_t fuckit;
#endif

void *pSmartMatrix = NULL;

CFastLED LEDS;
CFastLED & FastSPI_LED = LEDS;
CFastLED & FastSPI_LED2 = LEDS;
CFastLED & FastLED = LEDS;

CLEDController *CLEDController::m_pHead = NULL;
CLEDController *CLEDController::m_pTail = NULL;

// uint32_t CRGB::Squant = ((uint32_t)((__TIME__[4]-'0') * 28))<<16 | ((__TIME__[6]-'0')*50)<<8 | ((__TIME__[7]-'0')*28);

CFastLED::CFastLED() {
	// clear out the array of led controllers
	// m_nControllers = 0;
	m_Scale = 255;
	m_nFPS = 0;
}

CLEDController &CFastLED::addLeds(CLEDController *pLed,
									   struct CRGB *data,
									   int nLedsOrOffset, int nLedsIfOffset) {
	int nOffset = (nLedsIfOffset > 0) ? nLedsOrOffset : 0;
	int nLeds = (nLedsIfOffset > 0) ? nLedsIfOffset : nLedsOrOffset;

	pLed->init();
	pLed->setLeds(data + nOffset, nLeds);
	return *pLed;
}

void CFastLED::show(uint8_t scale) {
	CLEDController *pCur = CLEDController::head();
	while(pCur) {
		uint8_t d = pCur->getDither();
		if(m_nFPS < 100) { pCur->setDither(0); }
		pCur->showLeds(scale);
		pCur->setDither(d);
		pCur = pCur->next();
	}
	countFPS();
}

int CFastLED::count() {
    int x = 0;
	CLEDController *pCur = CLEDController::head();
	while( pCur) {
        x++;
		pCur = pCur->next();
	}
    return x;
}

CLEDController & CFastLED::operator[](int x) {
	CLEDController *pCur = CLEDController::head();
	while(x-- && pCur) {
		pCur = pCur->next();
	}
	if(pCur == NULL) {
		return *(CLEDController::head());
	} else {
		return *pCur;
	}
}

void CFastLED::showColor(const struct CRGB & color, uint8_t scale) {
	CLEDController *pCur = CLEDController::head();
	while(pCur) {
		uint8_t d = pCur->getDither();
		if(m_nFPS < 100) { pCur->setDither(0); }
		pCur->showColor(color, scale);
		pCur->setDither(d);
		pCur = pCur->next();
	}
	countFPS();
}

void CFastLED::clear(boolean writeData) {
	if(writeData) {
		showColor(CRGB(0,0,0), 0);
	}
    clearData();
}

void CFastLED::clearData() {
	CLEDController *pCur = CLEDController::head();
	while(pCur) {
		pCur->clearLedData();
		pCur = pCur->next();
	}
}

void CFastLED::delay(unsigned long ms) {
	unsigned long start = millis();
	while((millis()-start) < ms) {
		::delay(1);
		show();
	}
}

void CFastLED::setTemperature(const struct CRGB & temp) {
	CLEDController *pCur = CLEDController::head();
	while(pCur) {
		pCur->setTemperature(temp);
		pCur = pCur->next();
	}
}

void CFastLED::setCorrection(const struct CRGB & correction) {
	CLEDController *pCur = CLEDController::head();
	while(pCur) {
		pCur->setCorrection(correction);
		pCur = pCur->next();
	}
}

void CFastLED::setDither(uint8_t ditherMode)  {
	CLEDController *pCur = CLEDController::head();
	while(pCur) {
		pCur->setDither(ditherMode);
		pCur = pCur->next();
	}
}

extern int noise_min;
extern int noise_max;

void CFastLED::countFPS(int nFrames) {
  static int br = 0;
  static uint32_t lastframe = 0; // millis();

  if(br++ >= nFrames) {
		uint32_t now = millis();
		now -= lastframe;
		m_nFPS = (br * 1000) / now;
    br = 0;
    lastframe = millis();
  }
}

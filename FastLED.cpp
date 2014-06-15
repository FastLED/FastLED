#include "FastLED.h"

#if defined(__SAM3X8E__)
volatile uint32_t fuckit;
#endif

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
		pCur->showLeds(scale);
		pCur = pCur->next();
	}
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
		pCur->showColor(color, scale);
		pCur = pCur->next();
	}
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
	if(Serial) {
	  static uint32_t br = 0;
	  static uint32_t lastframe = millis();

	  br++;
	  if(br == nFrames) {
	    uint32_t now = millis() - lastframe;
	    uint32_t fps = (br * 1000) / now;
			/*Serial.print('('); Serial.print(noise_min); Serial.print(','); Serial.print(noise_max); Serial.print(") "); */
	    Serial.print(now); Serial.print("ms for "); Serial.print(br); Serial.print(" frames, aka ");
	    Serial.print(fps); Serial.println(" fps. ");
	    br = 0;
	    lastframe = millis();
	  }
	}
}

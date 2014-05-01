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
									   const struct CRGB *data,
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


template<int m, int n> void transpose8(unsigned char A[8], unsigned char B[8]) {
	uint32_t x, y, t;

	// Load the array and pack it into x and y.
  	y = *(unsigned int*)(A);
	x = *(unsigned int*)(A+4);

	// x = (A[0]<<24)   | (A[m]<<16)   | (A[2*m]<<8) | A[3*m];
	// y = (A[4*m]<<24) | (A[5*m]<<16) | (A[6*m]<<8) | A[7*m];

	t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
	t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);

	t = (x ^ (x >>14)) & 0x0000CCCC;  x = x ^ t ^ (t <<14);
	t = (y ^ (y >>14)) & 0x0000CCCC;  y = y ^ t ^ (t <<14);

	t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
	y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
	x = t;

	B[7*n] = y; y >>= 8;
  B[3*n] = x; x >>= 8;

	B[6*n] = y; y >>= 8;
	B[2*n] = x; x >>= 8;

	B[5*n] = y; y >>= 8;
	B[n] = x; x >>= 8;

	B[4*n] = y;
	B[0] = x;
	// B[0]=x>>24;    B[n]=x>>16;    B[2*n]=x>>8;  B[3*n]=x>>0;
	// B[4*n]=y>>24;  B[5*n]=y>>16;  B[6*n]=y>>8;  B[7*n]=y>>0;
}

void transposeLines(Lines & out, Lines & in) {
	transpose8<1,2>(in.bytes, out.bytes);
	transpose8<1,2>(in.bytes + 8, out.bytes + 1);
}

#ifdef CC3200
#include "FastLED.h"
#include "extras/driverlib/systick.h"
//#include "arduino_compat_cc3200.h"

extern volatile boolean stay_asleep;

static void (*SysTickCbFuncs[8])(uint32_t ui32TimeMS);

#define SYSTICKHZ               1000
#define SYSTICKMS               (1000 / SYSTICKHZ)

static unsigned long milliseconds = 0;
#define SYSTICK_INT_PRIORITY    0x80

unsigned long micros(void){
	return (milliseconds * 1000) + ( ((F_CPU / SYSTICKHZ) - MAP_SysTickValueGet()) / (F_CPU/1000000));
}

unsigned long millis(void){
	return milliseconds;
}

void delayMicroseconds(unsigned int us){
	// Systick timer rolls over every 1000000/SYSTICKHZ microseconds 
	if (us > (1000000UL / SYSTICKHZ - 1)) {
		delay(us / 1000);  // delay milliseconds
		us = us % 1000;     // handle remainder of delay
	};

	// 24 bit timer - mask off undefined bits
	unsigned long startTime = HWREG(NVIC_ST_CURRENT) & NVIC_ST_CURRENT_M;

	unsigned long ticks = (unsigned long)us * (F_CPU/1000000UL);
	volatile unsigned long elapsedTime;

	if (ticks > startTime) {
		ticks = (ticks + (NVIC_ST_CURRENT_M - (unsigned long)F_CPU / SYSTICKHZ)) & NVIC_ST_CURRENT_M;
	}

	do {
		elapsedTime = (startTime-(HWREG(NVIC_ST_CURRENT) & NVIC_ST_CURRENT_M )) & NVIC_ST_CURRENT_M;
	} while(elapsedTime <= ticks);
}

void delay(uint32_t millis){
	unsigned long i;
	for(i=0; i<millis*2; i++){
		delayMicroseconds(500);
	}
}

void registerSysTickCb(void (*userFunc)(uint32_t)){
	uint8_t i;
	for (i=0; i<8; i++) {
		if(!SysTickCbFuncs[i]) {
			SysTickCbFuncs[i] = userFunc;
			break;
		}
	}
}

void SysTickIntHandler(void){
	milliseconds++;

	uint8_t i;
	for (i=0; i<8; i++) {
		if (SysTickCbFuncs[i])
			SysTickCbFuncs[i](SYSTICKMS);
	}
}

#endif //defined CC3200
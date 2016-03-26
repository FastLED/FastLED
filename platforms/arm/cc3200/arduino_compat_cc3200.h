#ifndef __INC_ARDUINO_COMPAT_CC3200_h
#define __INC_ARDUINO_COMPAT_CC3200_h

#include "systick.h"
#include "hw_nvic.h"

#define HIGH		1
#define LOW			0
#define INPUT		0
#define OUTPUT		1
#define INPUT_PULLUP	2
#define LSBFIRST	0
#define MSBFIRST	1
#define _BV(n)		(1<<(n))
#define CHANGE		4
#define FALLING		2
#define RISING		3

// Arduino compatibility macros
#define digitalPinToPort(pin) (pin)
#define digitalPinToBitMask(pin) (digital_pin_to_info_PGM[(pin)].mask)
#define portOutputRegister(pin) ((digital_pin_to_info_PGM[(pin)].reg + 0))
#define portSetRegister(pin)    ((digital_pin_to_info_PGM[(pin)].reg + 4))
#define portClearRegister(pin)  ((digital_pin_to_info_PGM[(pin)].reg + 8))
#define portToggleRegister(pin) ((digital_pin_to_info_PGM[(pin)].reg + 12))
#define portInputRegister(pin)  ((digital_pin_to_info_PGM[(pin)].reg + 16))
#define portModeRegister(pin)   ((digital_pin_to_info_PGM[(pin)].reg + 20))
#define portConfigRegister(pin) ((digital_pin_to_info_PGM[(pin)].config))
#define digitalPinToPortReg(pin) (portOutputRegister(pin))

typedef uint8_t byte;
#ifdef __cplusplus
typedef bool boolean;
#else
typedef uint8_t boolean;
#define false 0
#define true (!false)
#endif

//random Arduino functions & vars
#define PI 3.1415926535897932384626433832795
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105

#ifndef M_PI
#define M_PI 3.1415926535897932384626433832795
#endif
#ifndef M_SQRT2
#define M_SQRT2 1.4142135623730950488016887
#endif

//replace stdlib abs if it's already defined. Not sure why arduino does this, but...
#ifdef abs
#undef abs
#endif
//NOTE: the auto keyword is specific to C++11, if compiling using C instead use typeof(x) instead
#define abs(x) ({ \
  auto _x = (x); \
  (_x > 0) ? _x : -_x; \
})
#define min(a, b) ({ \
  auto _a = (a); \
  auto _b = (b); \
  (_a < _b) ? _a : _b; \
})
#define max(a, b) ({ \
  auto _a = (a); \
  auto _b = (b); \
  (_a > _b) ? _a : _b; \
})
#define abs(x) ({ \
  auto _x = (x); \
  (_x > 0) ? _x : -_x; \
})
#define constrain(amt, low, high) ({ \
  auto _amt = (amt); \
  auto _low = (low); \
  auto _high = (high); \
  (_amt < _low) ? _low : ((_amt > _high) ? _high : _amt); \
})
#define round(x) ({ \
  auto _x = (x); \
  (_x>=0) ? (long)(_x+0.5) : (long)(_x-0.5); \
})
#define radians(deg) ((deg)*DEG_TO_RAD)
#define degrees(rad) ((rad)*RAD_TO_DEG)
#define sq(x) ({ \
  typeof(x) _x = (x); \
  _x * _x; \
})
#define sei() __enable_irq()
#define cli() __disable_irq()
#define interrupts() __enable_irq()
#define noInterrupts() __disable_irq()

//forward declarations for timing functions. Timing functions taken from Energia library "wiring.c"
static void (*SysTickCbFuncs[8])(uint32_t ui32TimeMS);
void delay(uint32_t milliseconds);
void sleep(uint32_t milliseconds);
void sleepSeconds(uint32_t seconds);
void suspend(void);
extern volatile boolean stay_asleep;

#define SYSTICKMS               (1000 / SYSTICKHZ)
#define SYSTICKHZ               1000

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

#endif

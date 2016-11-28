#ifndef __INC_ARDUINO_COMPAT_CC3200_H
#define __INC_ARDUINO_COMPAT_CC3200_H

#ifndef ENERGIA	//Energia libraries will be included, ignore this file

#pragma message "Including Arduino compatibility functions"

//C Standard libraries
#include <stdint.h>
#include <stdlib.h>

//#include "led_sysdefs_arm_cc3200.h"

//following includes are from $(CC3200_SDK)/inc/
#include "extras/inc/hw_nvic.h"
#include "extras/inc/hw_gpio.h"
#include "extras/inc/hw_types.h"
#include "extras/inc/hw_ints.h"
#include "extras/inc/hw_timer.h"

//following 2 libs are from $(CC3200_SDK)/driverlib/
#include "extras/driverlib/rom_map.h"
#include "extras/driverlib/systick.h"

#ifdef __cplusplus
extern "C"
{
#endif


#define HIGH		1
#define LOW			0
#define INPUT		0
#define OUTPUT		1
#define INPUT_PULLUP	2
#define LSBFIRST	0
#define MSBFIRST	1
#define _BV(n)		(1<<(n))
#define CHANGE		4
#define FALLING		3
#define RISING		2

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
#ifndef true
#define true (!false)
#endif	//true
#endif

//random Arduino functions & vars
#define PI 			3.1415926535897932384626433832795
#define HALF_PI 	1.5707963267948966192313216916398
#define TWO_PI 		6.283185307179586476925286766559
#define DEG_TO_RAD 	0.017453292519943295769236907684886
#define RAD_TO_DEG 	57.295779513082320876798154814105

#ifndef M_PI
#define M_PI 		3.1415926535897932384626433832795
#endif
#ifndef M_SQRT2
#define M_SQRT2 	1.4142135623730950488016887
#endif

//replace stdlib abs if it's already defined. Not sure why arduino does this, but...
#ifdef abs
#undef abs
#endif
//NOTE: the auto keyword is specific to C++11, if compiling using C instead use typeof(x) instead
#if 0
#ifdef __cplusplus
template <class T> inline T abs(const T& val)
{
	return (val < 0) ? (-val) : val;
}
template <class T> inline T min(const T& val1, const T& val2)
{
	return (val1 < val2) ? val1 : val2;
}
template <class T> inline T max(const T& val1, const T& val2)
{
	return (val1 > val2) ? val1 : val2;
}
template <class T> inline T constrain(const T& amt, const T& low, const T& high)
{
	return (amt < low) ? low : ((amt > high) ? high : amt); 
}

#define round(x) ({ \
  auto _x = (x); \
  (_x>=0) ? (long)(_x+0.5) : (long)(_x-0.5); \
})
#else //compiling w/ C
#define abs(x) ({ \
  typeof(x) _x = (x); \
  (_x > 0) ? _x : -_x; \
})
#define min(a, b) ({ \
  typeof(a) _a = (a); \
  typeof(b) _b = (b); \
  (_a < _b) ? _a : _b; \
})
#define max(a, b) ({ \
  typeof(a) _a = (a); \
  typeof(b) _b = (b); \
  (_a > _b) ? _a : _b; \
})
#define constrain(amt, low, high) ({ \
  typeof(amt) _amt = (amt); \
  typeof(low) _low = (low); \
  typeof(high) _high = (high); \
  (_amt < _low) ? _low : ((_amt > _high) ? _high : _amt); \
})
#define round(x) ({ \
  typeof(x) _x = (x); \
  (_x>=0) ? (long)(_x+0.5) : (long)(_x-0.5); \
})
#endif //__cplusplus (for typeof/auto confusion)
#endif //0. Testing other def of arduino funcs

#if 1
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define round(x)     ((x)>=0?(long)((x)+0.5):(long)((x)-0.5))
#endif	//1. Other def of arduino funcs

#define radians(deg) ((deg)*DEG_TO_RAD)
#define degrees(rad) ((rad)*RAD_TO_DEG)
#define sq(x) (x * x)
#define sei() __enable_irq()
#define cli() __disable_irq()
#define interrupts() __enable_irq()
#define noInterrupts() __disable_irq()

//forward declarations for timing functions. Timing functions taken from Energia library "wiring.c"
unsigned long micros(void);
unsigned long millis(void);
void delay(uint32_t milliseconds);
void sleep(uint32_t milliseconds);
void sleepSeconds(uint32_t seconds);
void suspend(void);
void registerSysTickCb(void (*userFunc)(uint32_t));
void SysTickIntHandler(void);

#ifdef __cplusplus
}
#endif

#endif //ENERGIA
#endif //__INC_ARDUINO_COMPAT_CC3200_h

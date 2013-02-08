#ifndef __INC_FASTPIN_H
#define __INC_FASTPIN_H

#include<avr/io.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Pin access class - needs to tune for various platforms (naive fallback solution?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DDR(_PIN) ((_PIN > 16) ? DDRC : (_PIN > 8) ? DDRB : DDRD)
#define PORT(_PIN) ((_PIN > 16) ? PORTC : (_PIN > 8) ? PORTB : PORTD)  
#define _CYCLES(_PIN) ((_PIN > 24) ? 2 : 1)

template<uint8_t PIN> class Pin { 
	static uint8_t mPinMask;
	static volatile uint8_t *mPort;
	static void _init() { 
		mPinMask = 1 << (PIN & 0x07); // digitalPinToBitMask(PIN);
		mPort = & PORT(PIN);
	}
public:
	static void setOutput() { _init(); DDR(PIN) |= 1 << (PIN & 0x07); }
	static void setInput() { _init(); DDR(PIN) &= ~(1 << (PIN & 0x07)); }

	inline static void hi() { PORT(PIN) |= 1 << (PIN & 0x07); }
	inline static void lo() { PORT(PIN) &= ~(1 << (PIN & 0x07)); }

	inline static void hi(register volatile uint8_t *port) { PORT(PIN) |= 1 << (PIN & 0x07); }
	inline static void lo(register volatile uint8_t *port) { PORT(PIN) &= ~(1 << (PIN & 0x07)); }
	inline static void set(register uint8_t val) { PORT(PIN) = val; }

	inline static void fastset(register volatile uint8_t *port, register uint8_t val) { PORT(PIN) = val; }

	static uint8_t hival() { return PORT(PIN) | mPinMask;  }
	static uint8_t loval() { return PORT(PIN) & ~mPinMask; }
	static volatile uint8_t *port() { return mPort; }
	static uint8_t mask() { return mPinMask; }
};

template<uint8_t PIN> uint8_t Pin<PIN>::mPinMask;
template<uint8_t PIN> volatile uint8_t *Pin<PIN>::mPort;


#endif

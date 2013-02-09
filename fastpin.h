#ifndef __INC_FASTPIN_H
#define __INC_FASTPIN_H

#include<avr/io.h>
#include<Arduino.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Pin access class - needs to tune for various platforms (naive fallback solution?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DDR(_PIN) ((_PIN >= 16) ? DDRC : (_PIN >= 8) ? DDRB : DDRD)
#define PORT(_PIN) ((_PIN >= 16) ? PORTC : (_PIN >= 8) ? PORTB : PORTD)  
#define PINP(_PIN) ((_PIN >= 16) ? PINC : (_PIN >= 8) ? PINB : PIND)  
#define _CYCLES(_PIN) ((_PIN >= 24) ? 2 : 1)

template<uint8_t PIN> class Pin { 
	static uint8_t mPinMask;
	static volatile uint8_t *mPort;
	static void _init() { 
		mPinMask = digitalPinToBitMask(PIN);
		mPort = portOutputRegister(digitalPinToPort(PIN));
	}
public:
	inline static void setOutput() { _init(); pinMode(PIN, OUTPUT); }
	inline static void setInput() { _init(); pinMode(PIN, INPUT); }

	inline static void hi() { *mPort |= mPinMask; } 
	inline static void lo() { *mPort &= ~mPinMask; }

	inline static void hi(register volatile uint8_t * port) { *port |= mPinMask; } 
	inline static void lo(register volatile uint8_t * port) { *port &= ~mPinMask; } 
	inline static void set(register uint8_t val) { *mPort = val; }

	inline static void fastset(register volatile uint8_t * port, register uint8_t val) { *port  = val; }

	static uint8_t hival() { return *mPort | mPinMask;  }
	static uint8_t loval() { return *mPort & ~mPinMask; }
	static volatile uint8_t *  port() { return mPort; }
	static uint8_t mask() { return mPinMask; }
};

template<uint8_t PIN> uint8_t Pin<PIN>::mPinMask;
template<uint8_t PIN> volatile uint8_t *Pin<PIN>::mPort;

template<uint8_t PIN, uint8_t _MASK, typename _PORT, typename _DDR, typename _PIN> class _NUPIN { 
public:
	inline static void setOutput() { _DDR::r() |= _MASK; }
	inline static void setInput() { _DDR::r() &= ~_MASK; }

	inline static void hi() { _PORT::r() |= _MASK; }
	inline static void lo() { _PORT::r() &= ~_MASK; }
	inline static void set(register uint8_t val) { _PORT::r() = val; }

	inline static void hi(register volatile uint8_t *port) { hi(); }
	inline static void lo(register volatile uint8_t *port) { lo(); }
	inline static void fastset(register volatile uint8_t *port, register uint8_t val) { set(val); }

	inline static uint8_t hival() { return _PORT::r() | _MASK; }
	inline static uint8_t loval() { return _PORT::r() & ~_MASK; }
	inline static volatile uint8_t * port() { return &_PORT::r(); }
	inline static uint8_t mask() { return _MASK; }
};


typedef volatile uint8_t & reg8_t;
#define _R(T) struct __gen_struct_ ## T
#define _RD8(T) struct __gen_struct_ ## T { static inline reg8_t r() { return T; }};
#define _IO(L) _RD8(DDR ## L); _RD8(PORT ## L); _RD8(PIN ## L);


#define _DEFPIN(PIN, MASK, L) template<> class Pin<PIN> : public _NUPIN<PIN, MASK, _R(PORT ## L), _R(DDR ## L), _R(PIN ## L)> {};

///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Pin definitions for AVR.  If there are pin definitions supplied below for the platform being built on 
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
// Accelerated port definitions for arduino avrs
_IO(D); _IO(B); _IO(C);
_DEFPIN( 0, 0x01, D); _DEFPIN( 1, 0x02, D); _DEFPIN( 2, 0x04, D); _DEFPIN( 3, 0x08, D);
_DEFPIN( 4, 0x10, D); _DEFPIN( 5, 0x20, D); _DEFPIN( 6, 0x40, D); _DEFPIN( 7, 0x80, D);
_DEFPIN( 8, 0x01, B); _DEFPIN( 9, 0x02, B); _DEFPIN(10, 0x04, B); _DEFPIN(11, 0x08, B);
_DEFPIN(12, 0x10, B); _DEFPIN(13, 0x20, B); _DEFPIN(14, 0x40, B); _DEFPIN(15, 0x80, B);
_DEFPIN(16, 0x01, C); _DEFPIN(17, 0x02, C); _DEFPIN(18, 0x04, C); _DEFPIN(19, 0x08, C);
_DEFPIN(20, 0x10, C); _DEFPIN(21, 0x20, C); _DEFPIN(22, 0x40, C); _DEFPIN(23, 0x80, C);

// Leonardo, teensy, blinkm
#elif defined(__AVR_ATmega32U4__)

_IO(B); _IO(C); _IO(D); _IO(E); _IO(F); 

_DEFPIN(0, 1, B); _DEFPIN(1, 2, B); _DEFPIN(2, 4, B); _DEFPIN(3, 8, B); 
_DEFPIN(4, 128, B); _DEFPIN(5, 1, D); _DEFPIN(6, 2, D); _DEFPIN(7, 4, D); 
_DEFPIN(8, 8, D); _DEFPIN(9, 64, C); _DEFPIN(10, 128, C); _DEFPIN(11, 64, D); 
_DEFPIN(12, 128, D); _DEFPIN(13, 16, B); _DEFPIN(14, 32, B); _DEFPIN(15, 64, B); 
_DEFPIN(16, 128, F); _DEFPIN(17, 64, F); _DEFPIN(18, 32, F); _DEFPIN(19, 16, F); 
_DEFPIN(20, 2, F); _DEFPIN(21, 1, F); _DEFPIN(22, 16, D); _DEFPIN(23, 32, D); 
#else

#pragma message "No pin/port mappings found, pin access will be slightly slower.  See fastpin.h for info."

#endif


//template<1> class NUPUN<1, 0x01, struct_PORTD, struct_DDRD, struct_PIND>;

// template<> class NUPIN<2> : public _NUPIN<1, 0x02, &DDRD, &PORTD, &PIND> {};

Pin<1> aPin;

#endif

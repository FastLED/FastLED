#ifndef __INC_FASTPIN_H
#define __INC_FASTPIN_H

#include<avr/io.h>

// Arduino.h needed for convinience functions digitalPinToPort/BitMask/portOutputRegister and the pinMode methods.
#include<Arduino.h>

#define NO_PIN 255 

// Class to ensure that a minimum amount of time has kicked since the last time run - and delay if not enough time has passed yet
// this should make sure that chipsets that have 
template<int WAIT> class CMinWait {
	long mLastMicros;
public:
	CMinWait() { mLastMicros = 0; }

	void wait() { 
		long diff = micros() - mLastMicros;
		if(diff < WAIT) { 
			delayMicroseconds(WAIT - diff);
		}
	}

	void mark() { mLastMicros = micros(); }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Pin access class - needs to tune for various platforms (naive fallback solution?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
#define _CYCLES(_PIN) (((_PIN >= 62 ) || (_PIN>=42 && _PIN<=49) || (_PIN>=14 && _PIN <=17) || (_PIN>=6 && _PIN <=9)) ? 2 : 1)
#else
#define _CYCLES(_PIN) ((_PIN >= 24) ? 2 : 1)
#endif

class Selectable {
public:
	virtual void select() = 0;
	virtual void release() = 0;
	virtual bool isSelected() = 0;
};

class Pin : public Selectable { 
	uint8_t mPinMask;
	uint8_t mPin;
	volatile uint8_t *mPort;

	void _init() { 
		mPinMask = digitalPinToBitMask(mPin);
		mPort = portOutputRegister(digitalPinToPort(mPin));
	}
public:
	Pin(int pin) : mPin(pin) { _init(); }

	typedef volatile uint8_t * port_ptr_t;
	typedef uint8_t port_t;

	inline void setOutput() { pinMode(mPin, OUTPUT); }
	inline void setInput() { pinMode(mPin, INPUT); }

	inline void hi() __attribute__ ((always_inline)) { *mPort |= mPinMask; } 
	inline void lo() __attribute__ ((always_inline)) { *mPort &= ~mPinMask; }

	inline void strobe() __attribute__ ((always_inline)) { hi(); lo(); }

	inline void hi(register port_ptr_t port) __attribute__ ((always_inline)) { *port |= mPinMask; } 
	inline void lo(register port_ptr_t port) __attribute__ ((always_inline)) { *port &= ~mPinMask; } 
	inline void set(register port_t val) __attribute__ ((always_inline)) { *mPort = val; }

	inline void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port  = val; }

	port_t hival() __attribute__ ((always_inline)) { return *mPort | mPinMask;  }
	port_t loval() __attribute__ ((always_inline)) { return *mPort & ~mPinMask; }
	port_ptr_t  port() __attribute__ ((always_inline)) { return mPort; }
	port_t mask() __attribute__ ((always_inline)) { return mPinMask; }

	virtual void select() { hi(); }
	virtual void release() { lo(); }
	virtual bool isSelected() { return (*mPort & mPinMask) == mPinMask; }
};

class OutputPin : public Pin {
public:
	OutputPin(int pin) : Pin(pin) { setOutput(); }
};

class InputPin : public Pin {
public:
	InputPin(int pin) : Pin(pin) { setInput(); }
};

/// The simplest level of Pin class.  This relies on runtime functions durinig initialization to get the port/pin mask for the pin.  Most
/// of the accesses involve references to these static globals that get set up.  This won't be the fastest set of pin operations, but it
/// will provide pin level access on pretty much all arduino environments.  In addition, it includes some methods to help optimize access in
/// various ways.  Namely, the versions of hi, lo, and fastset that take the port register as a passed in register variable (saving a global
/// dereference), since these functions are aggressively inlined, that can help collapse out a lot of extraneous memory loads/dereferences.
/// 
/// In addition, if, while writing a bunch of data to a pin, you know no other pins will be getting written to, you can get/cache a value of
/// the pin's port register and use that to do a full set to the register.  This results in one being able to simply do a store to the register,
/// vs. the load, and/or, and store that would be done normally.
///
/// There are platform specific instantiations of this class that provide direct i/o register access to pins for much higher speed pin twiddling.
///
/// Note that these classes are all static functions.  So the proper usage is Pin<13>::hi(); or such.  Instantiating objects is not recommended, 
/// as passing Pin objects around will likely -not- have the effect you're expecting.
template<uint8_t PIN> class FastPin { 
	static uint8_t sPinMask;
	static volatile uint8_t *sPort;
	static void _init() { 
		sPinMask = digitalPinToBitMask(PIN);
		sPort = portOutputRegister(digitalPinToPort(PIN));
	}
public:
	typedef volatile uint8_t * port_ptr_t;
	typedef uint8_t port_t;

	inline static void setOutput() { _init(); pinMode(PIN, OUTPUT); }
	inline static void setInput() { _init(); pinMode(PIN, INPUT); }

	inline static void hi() __attribute__ ((always_inline)) { *sPort |= sPinMask; } 
	inline static void lo() __attribute__ ((always_inline)) { *sPort &= ~sPinMask; }

	inline static void strobe() __attribute__ ((always_inline)) { hi(); lo(); }

	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { *port |= sPinMask; } 
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { *port &= ~sPinMask; } 
	inline static void set(register port_t val) __attribute__ ((always_inline)) { *sPort = val; }

	inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port  = val; }

	static port_t hival() __attribute__ ((always_inline)) { return *sPort | sPinMask;  }
	static port_t loval() __attribute__ ((always_inline)) { return *sPort & ~sPinMask; }
	static port_ptr_t  port() __attribute__ ((always_inline)) { return sPort; }
	static port_t mask() __attribute__ ((always_inline)) { return sPinMask; }
};

template<uint8_t PIN> uint8_t FastPin<PIN>::sPinMask;
template<uint8_t PIN> volatile uint8_t *FastPin<PIN>::sPort;

/// Class definition for a Pin where we know the port registers at compile time for said pin.  This allows us to make
/// a lot of optimizations, as the inlined hi/lo methods will devolve to a single io register write/bitset.  
template<uint8_t PIN, uint8_t _MASK, typename _PORT, typename _DDR, typename _PIN> class _AVRPIN { 
public:
	typedef volatile uint8_t * port_ptr_t;
	typedef uint8_t port_t;
	 
	inline static void setOutput() { _DDR::r() |= _MASK; }
	inline static void setInput() { _DDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { _PORT::r() |= _MASK; }
	inline static void lo() __attribute__ ((always_inline)) { _PORT::r() &= ~_MASK; }
	inline static void set(register uint8_t val) __attribute__ ((always_inline)) { _PORT::r() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { hi(); lo(); }
	
	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(register port_ptr_t port, register uint8_t val) __attribute__ ((always_inline)) { set(val); }

	inline static port_t hival() __attribute__ ((always_inline)) { return _PORT::r() | _MASK; }
	inline static port_t loval() __attribute__ ((always_inline)) { return _PORT::r() & ~_MASK; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_PORT::r(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

/// Template definition for teensy 3.0 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
template<uint8_t PIN, uint32_t _MASK, typename _PDOR, typename _PSOR, typename _PCOR, typename _PTOR, typename _PDIR, typename _PDDR> class _ARMPIN { 
public:
	typedef volatile uint32_t * port_ptr_t;
	typedef uint32_t port_t;

	inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
	inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { _PSOR::r() = _MASK; }
	inline static void lo() __attribute__ ((always_inline)) { _PCOR::r() = _MASK; }
	inline static void set(register port_t val) __attribute__ ((always_inline)) { _PDOR::r() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }
	
	inline static void toggle() __attribute__ ((always_inline)) { _PTOR::r() = _MASK; }

	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
	inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

	inline static port_t hival() __attribute__ ((always_inline)) { return _PDOR::r() | _MASK; }
	inline static port_t loval() __attribute__ ((always_inline)) { return _PDOR::r() & ~_MASK; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_PDOR::r(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

/// Template definition for teensy 3.0 style ARM pins using bit banding, providing direct access to the various GPIO registers.  GCC 
/// does a poor job of optimizing around these accesses so they are not being used just yet.
template<uint8_t PIN, int _BIT, typename _PDOR, typename _PSOR, typename _PCOR, typename _PTOR, typename _PDIR, typename _PDDR> class _ARMPIN_BITBAND { 
public:
	typedef volatile uint32_t * port_ptr_t;
	typedef uint32_t port_t;

	inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
	inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

	inline static void hi() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = 1; }
	inline static void lo() __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = 0; }
	inline static void set(register port_t val) __attribute__ ((always_inline)) { *_PDOR::template rx<_BIT>() = val; }

	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }
	
	inline static void toggle() __attribute__ ((always_inline)) { *_PTOR::template rx<_BIT>() = 1; }

	inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { *port = 1;  }
	inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { *port = 0; }
	inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

	inline static port_t hival() __attribute__ ((always_inline)) { return 1; }
	inline static port_t loval() __attribute__ ((always_inline)) { return 0; }
	inline static port_ptr_t port() __attribute__ ((always_inline)) { return _PDOR::template rx<_BIT>(); }
	inline static port_t mask() __attribute__ ((always_inline)) { return 1; }
};

/// AVR definitions for pins.  Getting around  the fact that I can't pass GPIO register addresses in as template arguments by instead creating
/// a custom type for each GPIO register with a single, static, aggressively inlined function that returns that specific GPIO register.  A similar
/// trick is used a bit further below for the ARM GPIO registers (of which there are far more than on AVR!)
typedef volatile uint8_t & reg8_t;
#define _R(T) struct __gen_struct_ ## T
#define _RD8(T) struct __gen_struct_ ## T { static inline reg8_t r() { return T; }};
#define _IO(L) _RD8(DDR ## L); _RD8(PORT ## L); _RD8(PIN ## L);
#define _DEFPIN_AVR(PIN, MASK, L) template<> class FastPin<PIN> : public _AVRPIN<PIN, MASK, _R(PORT ## L), _R(DDR ## L), _R(PIN ## L)> {};

// ARM definitions
#define GPIO_BITBAND_ADDR(reg, bit) (((uint32_t)&(reg) - 0x40000000) * 32 + (bit) * 4 + 0x42000000)
#define GPIO_BITBAND_PTR(reg, bit) ((uint32_t *)GPIO_BITBAND_ADDR((reg), (bit)))

typedef volatile uint32_t & reg32_t;
typedef volatile uint32_t * ptr_reg32_t;

#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline reg32_t r() { return T; } \
	template<int BIT> static __attribute__((always_inline)) inline ptr_reg32_t rx() { return GPIO_BITBAND_PTR(T, BIT); } };
#define _IO32(L) _RD32(GPIO ## L ## _PDOR); _RD32(GPIO ## L ## _PSOR); _RD32(GPIO ## L ## _PCOR); _RD32(GPIO ## L ## _PTOR); _RD32(GPIO ## L ## _PDIR); _RD32(GPIO ## L ## _PDDR);

#define _DEFPIN_ARM(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, 1 << BIT, _R(GPIO ## L ## _PDOR), _R(GPIO ## L ## _PSOR), _R(GPIO ## L ## _PCOR), \
																			_R(GPIO ## L ## _PTOR), _R(GPIO ## L ## _PDIR), _R(GPIO ## L ## _PDDR)> {}; 

// Don't use bit band'd pins for now, the compiler generates far less efficient code around them
// #define _DEFPIN_ARM(PIN, BIT, L) template<> class Pin<PIN> : public _ARMPIN_BITBAND<PIN, BIT, _R(GPIO ## L ## _PDOR), _R(GPIO ## L ## _PSOR), _R(GPIO ## L ## _PCOR),
// 																			_R(GPIO ## L ## _PTOR), _R(GPIO ## L ## _PDIR), _R(GPIO ## L ## _PDDR)> {}; 


///////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Pin definitions for AVR and ARM.  If there are pin definitions supplied below for the platform being 
// built on, then much higher speed access will be possible, namely with direct GPIO register accesses.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(FORCE_SOFTWARE_PINS)
#warning "Softwrae pin support forced pin access will be slightly slower.  See fastpin.h for info."
#define NO_HARDWARE_PIN_SUPPORT

#elif defined(__AVR_ATtiny85__) 
_IO(B);

_DEFPIN_AVR(0, 0x01, B); _DEFPIN_AVR(1, 0x02, B); _DEFPIN_AVR(2, 0x04, B); _DEFPIN_AVR(3, 0x08, B);
_DEFPIN_AVR(4, 0x10, B); _DEFPIN_AVR(5, 0x20, B);

#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
// Accelerated port definitions for arduino avrs
_IO(D); _IO(B); _IO(C);
_DEFPIN_AVR( 0, 0x01, D); _DEFPIN_AVR( 1, 0x02, D); _DEFPIN_AVR( 2, 0x04, D); _DEFPIN_AVR( 3, 0x08, D);
_DEFPIN_AVR( 4, 0x10, D); _DEFPIN_AVR( 5, 0x20, D); _DEFPIN_AVR( 6, 0x40, D); _DEFPIN_AVR( 7, 0x80, D);
_DEFPIN_AVR( 8, 0x01, B); _DEFPIN_AVR( 9, 0x02, B); _DEFPIN_AVR(10, 0x04, B); _DEFPIN_AVR(11, 0x08, B);
_DEFPIN_AVR(12, 0x10, B); _DEFPIN_AVR(13, 0x20, B); _DEFPIN_AVR(14, 0x01, C); _DEFPIN_AVR(15, 0x02, C);
_DEFPIN_AVR(16, 0x04, C); _DEFPIN_AVR(17, 0x08, C); _DEFPIN_AVR(18, 0x10, C); _DEFPIN_AVR(19, 0x20, C);

#define SPI_DATA 11
#define SPI_CLOCK 13
#define SPI_SELECT 10
#define AVR_HARDWARE_SPI

#elif defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
// megas

_IO(A); _IO(B); _IO(C); _IO(D); _IO(E); _IO(F); _IO(G); _IO(H); _IO(J); _IO(K); _IO(L);

_DEFPIN_AVR(0, 1, E); _DEFPIN_AVR(1, 2, E); _DEFPIN_AVR(2, 16, E); _DEFPIN_AVR(3, 32, E); 
_DEFPIN_AVR(4, 32, G); _DEFPIN_AVR(5, 8, E); _DEFPIN_AVR(6, 8, H); _DEFPIN_AVR(7, 16, H); 
_DEFPIN_AVR(8, 32, H); _DEFPIN_AVR(9, 64, H); _DEFPIN_AVR(10, 16, B); _DEFPIN_AVR(11, 32, B); 
_DEFPIN_AVR(12, 64, B); _DEFPIN_AVR(13, 128, B); _DEFPIN_AVR(14, 2, J); _DEFPIN_AVR(15, 1, J); 
_DEFPIN_AVR(16, 2, H); _DEFPIN_AVR(17, 1, H); _DEFPIN_AVR(18, 8, D); _DEFPIN_AVR(19, 4, D); 
_DEFPIN_AVR(20, 2, D); _DEFPIN_AVR(21, 1, D); _DEFPIN_AVR(22, 1, A); _DEFPIN_AVR(23, 2, A); 
_DEFPIN_AVR(24, 4, A); _DEFPIN_AVR(25, 8, A); _DEFPIN_AVR(26, 16, A); _DEFPIN_AVR(27, 32, A); 
_DEFPIN_AVR(28, 64, A); _DEFPIN_AVR(29, 128, A); _DEFPIN_AVR(30, 128, C); _DEFPIN_AVR(31, 64, C); 
_DEFPIN_AVR(32, 32, C); _DEFPIN_AVR(33, 16, C); _DEFPIN_AVR(34, 8, C); _DEFPIN_AVR(35, 4, C); 
_DEFPIN_AVR(36, 2, C); _DEFPIN_AVR(37, 1, C); _DEFPIN_AVR(38, 128, D); _DEFPIN_AVR(39, 4, G); 
_DEFPIN_AVR(40, 2, G); _DEFPIN_AVR(41, 1, G); _DEFPIN_AVR(42, 128, L); _DEFPIN_AVR(43, 64, L); 
_DEFPIN_AVR(44, 32, L); _DEFPIN_AVR(45, 16, L); _DEFPIN_AVR(46, 8, L); _DEFPIN_AVR(47, 4, L); 
_DEFPIN_AVR(48, 2, L); _DEFPIN_AVR(49, 1, L); _DEFPIN_AVR(50, 8, B); _DEFPIN_AVR(51, 4, B); 
_DEFPIN_AVR(52, 2, B); _DEFPIN_AVR(53, 1, B); _DEFPIN_AVR(54, 1, F); _DEFPIN_AVR(55, 2, F); 
_DEFPIN_AVR(56, 4, F); _DEFPIN_AVR(57, 8, F); _DEFPIN_AVR(58, 16, F); _DEFPIN_AVR(59, 32, F); 
_DEFPIN_AVR(60, 64, F); _DEFPIN_AVR(61, 128, F); _DEFPIN_AVR(62, 1, K); _DEFPIN_AVR(63, 2, K); 
_DEFPIN_AVR(64, 4, K); _DEFPIN_AVR(65, 8, K); _DEFPIN_AVR(66, 16, K); _DEFPIN_AVR(67, 32, K); 
_DEFPIN_AVR(68, 64, K); _DEFPIN_AVR(69, 128, K); 

#define SPI_DATA 51
#define SPI_CLOCK 52
#define SPI_SELECT 53
#define AVR_HARDWARE_SPI

// Leonardo, teensy, blinkm
#elif defined(__AVR_ATmega32U4__) && defined(CORE_TEENSY)

// teensy defs
_IO(B); _IO(C); _IO(D); _IO(E); _IO(F); 

_DEFPIN_AVR(0, 1, B); _DEFPIN_AVR(1, 2, B); _DEFPIN_AVR(2, 4, B); _DEFPIN_AVR(3, 8, B); 
_DEFPIN_AVR(4, 128, B); _DEFPIN_AVR(5, 1, D); _DEFPIN_AVR(6, 2, D); _DEFPIN_AVR(7, 4, D); 
_DEFPIN_AVR(8, 8, D); _DEFPIN_AVR(9, 64, C); _DEFPIN_AVR(10, 128, C); _DEFPIN_AVR(11, 64, D); 
_DEFPIN_AVR(12, 128, D); _DEFPIN_AVR(13, 16, B); _DEFPIN_AVR(14, 32, B); _DEFPIN_AVR(15, 64, B); 
_DEFPIN_AVR(16, 128, F); _DEFPIN_AVR(17, 64, F); _DEFPIN_AVR(18, 32, F); _DEFPIN_AVR(19, 16, F); 
_DEFPIN_AVR(20, 2, F); _DEFPIN_AVR(21, 1, F); _DEFPIN_AVR(22, 16, D); _DEFPIN_AVR(23, 32, D); 

#define SPI_DATA 2
#define SPI_CLOCK 1
#define SPI_SELECT 3
#define AVR_HARDWARE_SPI

#elif defined(__AVR_AT90USB646__) || defined(__AVR_AT90USB1286__)
// teensy++ 2 defs

_IO(A); _IO(B); _IO(C); _IO(D); _IO(E); _IO(F); 

_DEFPIN_AVR(0, 1, D); _DEFPIN_AVR(1, 2, D); _DEFPIN_AVR(2, 4, D); _DEFPIN_AVR(3, 8, D); 
_DEFPIN_AVR(4, 16, D); _DEFPIN_AVR(5, 32, D); _DEFPIN_AVR(6, 64, D); _DEFPIN_AVR(7, 128, D); 
_DEFPIN_AVR(8, 1, E); _DEFPIN_AVR(9, 2, E); _DEFPIN_AVR(10, 1, C); _DEFPIN_AVR(11, 2, C); 
_DEFPIN_AVR(12, 4, C); _DEFPIN_AVR(13, 8, C); _DEFPIN_AVR(14, 16, C); _DEFPIN_AVR(15, 32, C); 
_DEFPIN_AVR(16, 64, C); _DEFPIN_AVR(17, 128, C); _DEFPIN_AVR(18, 64, E); _DEFPIN_AVR(19, 128, E); 
_DEFPIN_AVR(20, 1, B); _DEFPIN_AVR(21, 2, B); _DEFPIN_AVR(22, 4, B); _DEFPIN_AVR(23, 8, B); 
_DEFPIN_AVR(24, 16, B); _DEFPIN_AVR(25, 32, B); _DEFPIN_AVR(26, 64, B); _DEFPIN_AVR(27, 128, B); 
_DEFPIN_AVR(28, 1, A); _DEFPIN_AVR(29, 2, A); _DEFPIN_AVR(30, 4, A); _DEFPIN_AVR(31, 8, A); 
_DEFPIN_AVR(32, 16, A); _DEFPIN_AVR(33, 32, A); _DEFPIN_AVR(34, 64, A); _DEFPIN_AVR(35, 128, A); 
_DEFPIN_AVR(36, 16, E); _DEFPIN_AVR(37, 32, E); _DEFPIN_AVR(38, 1, F); _DEFPIN_AVR(39, 2, F); 
_DEFPIN_AVR(40, 4, F); _DEFPIN_AVR(41, 8, F); _DEFPIN_AVR(42, 16, F); _DEFPIN_AVR(43, 32, F); 
_DEFPIN_AVR(44, 64, F); _DEFPIN_AVR(45, 128, F); 

#define SPI_DATA 22
#define SPI_CLOCK 21
#define SPI_SELECT 20
#define AVR_HARDWARE_SPI

#elif defined(__AVR_ATmega32U4__)

// leonard defs
_IO(B); _IO(C); _IO(D); _IO(E); _IO(F); 

_DEFPIN_AVR(0, 4, D); _DEFPIN_AVR(1, 8, D); _DEFPIN_AVR(2, 2, D); _DEFPIN_AVR(3, 1, D); 
_DEFPIN_AVR(4, 16, D); _DEFPIN_AVR(5, 64, C); _DEFPIN_AVR(6, 128, D); _DEFPIN_AVR(7, 64, E); 
_DEFPIN_AVR(8, 16, B); _DEFPIN_AVR(9, 32, B); _DEFPIN_AVR(10, 64, B); _DEFPIN_AVR(11, 128, B); 
_DEFPIN_AVR(12, 64, D); _DEFPIN_AVR(13, 128, C); _DEFPIN_AVR(14, 8, B); _DEFPIN_AVR(15, 2, B); 
_DEFPIN_AVR(16, 4, B); _DEFPIN_AVR(17, 1, B); _DEFPIN_AVR(18, 128, F); _DEFPIN_AVR(19, 64, F); 
_DEFPIN_AVR(20, 32, F); _DEFPIN_AVR(21, 16, F); _DEFPIN_AVR(22, 2, F); _DEFPIN_AVR(23, 0, F); 

#define SPI_DATA 16
#define SPI_CLOCK 15
#define AVR_HARDWARE_SPI

#elif defined(__MK20DX128__) && defined(CORE_TEENSY)

_IO32(A); _IO32(B); _IO32(C); _IO32(D); _IO32(E);

_DEFPIN_ARM(0, 16, B); _DEFPIN_ARM(1, 17, B); _DEFPIN_ARM(2, 0, D); _DEFPIN_ARM(3, 12, A);
_DEFPIN_ARM(4, 13, A); _DEFPIN_ARM(5, 7, D); _DEFPIN_ARM(6, 4, D); _DEFPIN_ARM(7, 2, D);
_DEFPIN_ARM(8, 3, D); _DEFPIN_ARM(9, 3, C); _DEFPIN_ARM(10, 4, C); _DEFPIN_ARM(11, 6, C);
_DEFPIN_ARM(12, 7, C); _DEFPIN_ARM(13, 5, C); _DEFPIN_ARM(14, 1, D); _DEFPIN_ARM(15, 0, C);
_DEFPIN_ARM(16, 0, B); _DEFPIN_ARM(17, 1, B); _DEFPIN_ARM(18, 3, B); _DEFPIN_ARM(19, 2, B);
_DEFPIN_ARM(20, 5, D); _DEFPIN_ARM(21, 6, D); _DEFPIN_ARM(22, 1, C); _DEFPIN_ARM(23, 2, C);
_DEFPIN_ARM(24, 5, A); _DEFPIN_ARM(25, 19, B); _DEFPIN_ARM(26, 1, E); _DEFPIN_ARM(27, 9, C);
_DEFPIN_ARM(28, 8, C); _DEFPIN_ARM(29, 10, C); _DEFPIN_ARM(30, 11, C); _DEFPIN_ARM(31, 0, E);
_DEFPIN_ARM(32, 18, B); _DEFPIN_ARM(33, 4, A);

#define SPI_DATA 11
#define SPI_CLOCK 13
#define ARM_HARDWARE_SPI

#elif defined(__SAM3X8E__)

DUE_IO32(A);
DUE_IO32(B);
DUE_IO32(C);
DUE_IO32(D);

_DEFPIN_DUE(0, 8, A); _DEFPIN_DUE(1, 9, A); _DEFPIN_DUE(2, 25, B); _DEFPIN_DUE(3, 28, C);
_DEFPIN_DUE(4, 26, C); _DEFPIN_DUE(5, 25, C); _DEFPIN_DUE(6, 24, C); _DEFPIN_DUE(7, 23, C);
_DEFPIN_DUE(8, 22, C); _DEFPIN_DUE(9, 21, C); _DEFPIN_DUE(10, 29, C); _DEFPIN_DUE(11, 7, D);
_DEFPIN_DUE(12, 8, D); _DEFPIN_DUE(13, 27, B); _DEFPIN_DUE(14, 4, D); _DEFPIN_DUE(15, 5, D);
_DEFPIN_DUE(16, 13, A); _DEFPIN_DUE(17, 12, A); _DEFPIN_DUE(18, 11, A); _DEFPIN_DUE(19, 10, A);
_DEFPIN_DUE(20, 12, B); _DEFPIN_DUE(21, 13, B); _DEFPIN_DUE(22, 26, B); _DEFPIN_DUE(23, 14, A);
_DEFPIN_DUE(24, 15, A); _DEFPIN_DUE(25, 0, D); _DEFPIN_DUE(26, 1, D); _DEFPIN_DUE(27, 2, D);
_DEFPIN_DUE(28, 3, D); _DEFPIN_DUE(29, 6, D); _DEFPIN_DUE(30, 9, D); _DEFPIN_DUE(31, 7, A);
_DEFPIN_DUE(32, 10, D); _DEFPIN_DUE(33, 1, C); _DEFPIN_DUE(34, 2, C); _DEFPIN_DUE(35, 3, C);
_DEFPIN_DUE(36, 4, C); _DEFPIN_DUE(37, 5, C); _DEFPIN_DUE(38, 6, C); _DEFPIN_DUE(39, 7, C);
_DEFPIN_DUE(40, 8, C); _DEFPIN_DUE(41, 9, C); _DEFPIN_DUE(42, 19, A); _DEFPIN_DUE(43, 20, A);
_DEFPIN_DUE(44, 19, C); _DEFPIN_DUE(45, 18, C); _DEFPIN_DUE(46, 17, C); _DEFPIN_DUE(47, 16, C);
_DEFPIN_DUE(48, 15, C); _DEFPIN_DUE(49, 14, C); _DEFPIN_DUE(50, 13, C); _DEFPIN_DUE(51, 12, C);
_DEFPIN_DUE(52, 21, B); _DEFPIN_DUE(53, 14, B); _DEFPIN_DUE(54, 16, A); _DEFPIN_DUE(55, 24, A);
_DEFPIN_DUE(56, 23, A); _DEFPIN_DUE(57, 22, A); _DEFPIN_DUE(58, 6, A); _DEFPIN_DUE(59, 4, A);
_DEFPIN_DUE(60, 3, A); _DEFPIN_DUE(61, 2, A); _DEFPIN_DUE(62, 17, B); _DEFPIN_DUE(63, 18, B);
_DEFPIN_DUE(64, 19, B); _DEFPIN_DUE(65, 20, B); _DEFPIN_DUE(66, 15, B); _DEFPIN_DUE(67, 16, B);
_DEFPIN_DUE(68, 1, A); _DEFPIN_DUE(69, 0, A); _DEFPIN_DUE(70, 17, A); _DEFPIN_DUE(71, 18, A);
_DEFPIN_DUE(72, 30, C); _DEFPIN_DUE(73, 21, A); _DEFPIN_DUE(74, 25, A); _DEFPIN_DUE(75, 26, A);
_DEFPIN_DUE(76, 27, A); _DEFPIN_DUE(77, 28, A); _DEFPIN_DUE(78, 23, B);

#else

#warning "No pin/port mappings found, pin access will be slightly slower.  See fastpin.h for info."
#define NO_HARDWARE_PIN_SUPPORT

#endif

#endif

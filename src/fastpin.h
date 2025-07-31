#pragma once

#ifndef __INC_FASTPIN_H
#define __INC_FASTPIN_H

#include "FastLED.h"
#include "fl/compiler_control.h"

#include "led_sysdefs.h"
#include "fl/unused.h"
#include "fl/int.h"
#include "fl/register.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#ifdef ESP32
// Get rid of the endless volatile warnings in ESP32
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wvolatile"
#endif

/// @file fastpin.h
/// Class base definitions for defining fast pin access

FASTLED_NAMESPACE_BEGIN

/// Constant for "not a pin"
/// @todo Unused, remove? 
#define NO_PIN 255

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Pin access class - needs to tune for various platforms (naive fallback solution?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Abstract class for "selectable" things
class Selectable {
public:
	#ifndef __AVR__
	virtual ~Selectable() {}
	#endif
	virtual void select() = 0;      ///< Select this object
	virtual void release() = 0;     ///< Release this object
	virtual bool isSelected() = 0;  ///< Check if this object is currently selected
};

#if defined(FASTLED_STUB_IMPL) || defined(__EMSCRIPTEN__)


class Pin : public Selectable {


	void _init() {
	}

public:
	Pin(int pin) { FL_UNUSED(pin); }


	void setPin(int pin) { FL_UNUSED(pin); }

	typedef volatile RwReg * port_ptr_t;
	typedef RwReg port_t;

	inline void setOutput() { /* NOOP */ }
	inline void setInput() { /* NOOP */ }
	inline void setInputPullup() { /* NOOP */ }


	inline void hi() __attribute__ ((always_inline)) {}
	/// Set the pin state to `LOW`
	inline void lo() __attribute__ ((always_inline)) {}


	inline void strobe() __attribute__ ((always_inline)) {  }
	inline void toggle() __attribute__ ((always_inline)) { }

	inline void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { FL_UNUSED(port); }
	inline void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { FL_UNUSED(port); }
	inline void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { FL_UNUSED(val); }

	inline void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { FL_UNUSED(port); FL_UNUSED(val); }

	port_t hival() __attribute__ ((always_inline)) { return 0; }
	port_t loval() __attribute__ ((always_inline)) { return 0; }
	port_ptr_t  port() __attribute__ ((always_inline)) {
		static volatile RwReg port = 0;
		return &port;
	}
	port_t mask() __attribute__ ((always_inline)) { return 0xff; }

	virtual void select() override { hi(); }
	virtual void release() override { lo(); }
	virtual bool isSelected() override { return true; }
};

class OutputPin : public Pin {
public:
	OutputPin(int pin) : Pin(pin) { setOutput(); }
};

class InputPin : public Pin {
public:
	InputPin(int pin) : Pin(pin) { setInput(); }
};

#elif !defined(FASTLED_NO_PINMAP)

/// Naive fallback solution for low level pin access
class Pin : public Selectable {
	volatile RwReg *mPort;    ///< Output register for the pin
	volatile RoReg *mInPort;  ///< Input register for the pin
	RwReg mPinMask;  ///< Bitmask for the pin within its register
	fl::u8 mPin;    ///< Arduino digital pin number

	/// Initialize the class by retrieving the register
	/// pointers and bitmask.
	void _init() {
		mPinMask = digitalPinToBitMask(mPin);
		mPort = (volatile RwReg*)portOutputRegister(digitalPinToPort(mPin));
		mInPort = (volatile RoReg*)portInputRegister(digitalPinToPort(mPin));
	}

public:
	/// Constructor
	/// @param pin Arduino digital pin number
	Pin(int pin) : mPin(pin) { _init(); }
	#ifndef __AVR__
	virtual ~Pin() {}  // Shut up the compiler warning, but don't steal bytes from AVR.
	#endif

	typedef volatile RwReg * port_ptr_t;  ///< type for a pin read/write register, volatile
	typedef RwReg port_t;  ///< type for a pin read/write register, non-volatile

	/// Set the pin mode as `OUTPUT`
	inline void setOutput() { pinMode(mPin, OUTPUT); }

	/// Set the pin mode as `INPUT`
	inline void setInput() { pinMode(mPin, INPUT); }

	inline void setInputPullup() { pinMode(mPin, INPUT_PULLUP); }

	/// Set the pin state to `HIGH`
	FL_DISABLE_WARNING_PUSH
	FL_DISABLE_WARNING_NULL_DEREFERENCE
	inline void hi() __attribute__ ((always_inline)) { *mPort |= mPinMask; }
	/// Set the pin state to `LOW`
	inline void lo() __attribute__ ((always_inline)) { *mPort &= ~mPinMask; }
	FL_DISABLE_WARNING_POP

	/// Toggle the pin twice to create a short pulse
	inline void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }
	/// Toggle the pin. 
	/// If the pin was high, set it low. If was low, set it high.
	inline void toggle() __attribute__ ((always_inline)) { *mInPort = mPinMask; }

	/// Set the same pin on another port to `HIGH`
	/// @param port the port to modify
	inline void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { *port |= mPinMask; }
	/// Set the same pin on another port to `LOW`
	/// @param port the port to modify
	inline void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { *port &= ~mPinMask; }
	/// Set the state of the output register
	/// @param val the state to set the output register to
	/// @note This function is not limited to the current pin! It modifies the entire register.
	inline void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *mPort = val; }

	/// Set the state of a port
	/// @param port the port to modify
	/// @param val the state to set the port to
	inline void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port  = val; }

	/// Gets the state of the port with this pin `HIGH`
	port_t hival() __attribute__ ((always_inline)) { return *mPort | mPinMask;  }
	/// Gets the state of the port with this pin `LOW`
	port_t loval() __attribute__ ((always_inline)) { return *mPort & ~mPinMask; }
	/// Get the output state of the port
	port_ptr_t  port() __attribute__ ((always_inline)) { return mPort; }
	/// Get the pin mask
	port_t mask() __attribute__ ((always_inline)) { return mPinMask; }

	/// @copydoc Pin::hi()
	virtual void select() override { hi(); }
	/// @copydoc Pin::lo()
	virtual void release() override { lo(); }
	/// Checks if the pin is currently `HIGH`
	virtual bool isSelected() override { return (*mPort & mPinMask) == mPinMask; }
};

/// I/O pin initially set to OUTPUT
class OutputPin : public Pin {
public:
	/// @copydoc Pin::Pin(int)
	OutputPin(int pin) : Pin(pin) { setOutput(); }
};

/// I/O pin initially set to INPUT
class InputPin : public Pin {
public:
	/// @copydoc Pin::Pin(int)
	InputPin(int pin) : Pin(pin) { setInput(); }
};

#else
// This is the empty code version of the raw pin class, method bodies should be filled in to Do The Right Thing[tm] when making this
// available on a new platform

class Pin : public Selectable {
	volatile RwReg *mPort;
	volatile RoReg *mInPort;
	RwReg mPinMask;
	fl::u8 mPin;

	RwReg mPortFake = 0;
	RoReg mInPortFake = 0;



	void _init() {
		// TODO: fill in init on a new platform
		mPinMask = 0;
		mPort = &mPortFake;
		mInPort = &mInPortFake;
	}

public:
	Pin(int pin) : mPin(pin) { _init(); }
	#ifndef __AVR__
	virtual ~Pin() {}  // Shut up the compiler warning, but don't steal bytes from AVR.
	#endif

	void setPin(int pin) { mPin = pin; _init(); }

	typedef volatile RwReg * port_ptr_t;
	typedef RwReg port_t;

	inline void setOutput() { /* TODO: Set pin output */ }
	inline void setInput() { /* TODO: Set pin input */ }
	inline void setInputPullup() { /* TODO: Set pin input pullup */ }

	/// Set the pin state to `HIGH`
	FL_DISABLE_WARNING_PUSH
	FL_DISABLE_WARNING_NULL_DEREFERENCE
	inline void hi() __attribute__ ((always_inline)) { *mPort |= mPinMask; }
	/// Set the pin state to `LOW`
	inline void lo() __attribute__ ((always_inline)) { *mPort &= ~mPinMask; }
	FL_DISABLE_WARNING_POP

	inline void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }
	inline void toggle() __attribute__ ((always_inline)) { *mInPort = mPinMask; }

	inline void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { *port |= mPinMask; }
	inline void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { *port &= ~mPinMask; }
	inline void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *mPort = val; }

	inline void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port  = val; }

	port_t hival() __attribute__ ((always_inline)) { return *mPort | mPinMask;  }
	port_t loval() __attribute__ ((always_inline)) { return *mPort & ~mPinMask; }
	port_ptr_t  port() __attribute__ ((always_inline)) { return mPort; }
	port_t mask() __attribute__ ((always_inline)) { return mPinMask; }

	virtual void select() override { hi(); }
	virtual void release() override { lo(); }
	virtual bool isSelected() override { return (*mPort & mPinMask) == mPinMask; }
};

class OutputPin : public Pin {
public:
	OutputPin(int pin) : Pin(pin) { setOutput(); }
};

class InputPin : public Pin {
public:
	InputPin(int pin) : Pin(pin) { setInput(); }
};

#endif

/// The simplest level of Pin class.  This relies on runtime functions during initialization to get the port/pin mask for the pin.  Most
/// of the accesses involve references to these static globals that get set up.  This won't be the fastest set of pin operations, but it
/// will provide pin level access on pretty much all Arduino environments.  In addition, it includes some methods to help optimize access in
/// various ways.  Namely, the versions of hi(), lo(), and fastset() that take the port register as a passed in register variable (saving a global
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
#ifdef FASTLED_FORCE_SOFTWARE_PINS
template<fl::u8 PIN> class FastPin {
	static RwReg sPinMask;            ///< Bitmask for the pin within its register
	static volatile RwReg *sPort;     ///< Output register for the pin
	static volatile RoReg *sInPort;   ///< Input register for the pin
	static void _init() {
#if !defined(FASTLED_NO_PINMAP)
		sPinMask = digitalPinToBitMask(PIN);
		sPort = portOutputRegister(digitalPinToPort(PIN));
		sInPort = portInputRegister(digitalPinToPort(PIN));
#endif
	}

public:
	typedef volatile RwReg * port_ptr_t;  ///< @copydoc Pin::port_ptr_t
	typedef RwReg port_t;  ///< @copydoc Pin::port_t

	/// @copydoc Pin::setOutput()
	inline static void setOutput() { _init(); pinMode(PIN, OUTPUT); }
	/// @copydoc Pin::setInput()
	inline static void setInput() { _init(); pinMode(PIN, INPUT); }

	/// @copydoc Pin::hi()
	inline static void hi() __attribute__ ((always_inline)) { *sPort |= sPinMask; }
	/// @copydoc Pin::lo()
	inline static void lo() __attribute__ ((always_inline)) { *sPort &= ~sPinMask; }

	/// @copydoc Pin::strobe()
	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	/// @copydoc Pin::toggle()
	inline static void toggle() __attribute__ ((always_inline)) { *sInPort = sPinMask; }

	/// @copydoc Pin::hi(FASTLED_REGISTER port_ptr_t)
	inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { *port |= sPinMask; }
	/// @copydoc Pin::lo(FASTLED_REGISTER port_ptr_t)
	inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { *port &= ~sPinMask; }
	/// @copydoc Pin::set(FASTLED_REGISTER port_t)
	inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *sPort = val; }

	/// @copydoc Pin::fastset()
	inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port  = val; }

	/// @copydoc Pin::hival()
	static port_t hival() __attribute__ ((always_inline)) { return *sPort | sPinMask;  }
	/// @copydoc Pin::loval()
	static port_t loval() __attribute__ ((always_inline)) { return *sPort & ~sPinMask; }
	/// @copydoc Pin::port()
	static port_ptr_t  port() __attribute__ ((always_inline)) { return sPort; }
	/// @copydoc Pin::mask()
	static port_t mask() __attribute__ ((always_inline)) { return sPinMask; }
};

template<fl::u8 PIN> RwReg FastPin<PIN>::sPinMask;
template<fl::u8 PIN> volatile RwReg *FastPin<PIN>::sPort;
template<fl::u8 PIN> volatile RoReg *FastPin<PIN>::sInPort;

#else

template<fl::u8 PIN> class FastPin {
	// This is a default implementation. If you are hitting this then FastPin<> is either:
	// 1) Not defined -or-
	// 2) Not part of the set of defined FastPin<> specializations for your platform
	// You need to define a FastPin<> specialization
	// or change what get's included for your particular build target.
	// Keep in mind that these messages are cryptic, so it's best to define an invalid in type.
	constexpr static bool validpin() { return false; }
	constexpr static bool LowSpeedOnlyRecommended() {  // Some implementations assume this exists.
        // Caller must always determine if high speed use if allowed on a given pin,
        // because it depends on more than just the chip packaging ... it depends on entire board (and even system) design.
        return false; // choosing default to be FALSE, to allow users to ATTEMPT to use high-speed on pins where support is not known
    }

	static_assert(validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");

	static void _init() { }

public:
	typedef volatile RwReg * port_ptr_t;  ///< @copydoc Pin::port_ptr_t
	typedef RwReg port_t;  ///< @copydoc Pin::port_t

	/// @copydoc Pin::setOutput()
	inline static void setOutput() { }
	/// @copydoc Pin::setInput()
	inline static void setInput() { }

	/// @copydoc Pin::hi()
	inline static void hi() __attribute__ ((always_inline)) { }
	/// @copydoc Pin::lo()
	inline static void lo() __attribute__ ((always_inline)) { }

	/// @copydoc Pin::strobe()
	inline static void strobe() __attribute__ ((always_inline)) { }

	/// @copydoc Pin::toggle()
	inline static void toggle() __attribute__ ((always_inline)) { }

	/// @copydoc Pin::hi(FASTLED_REGISTER port_ptr_t)
	inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) {
		FASTLED_UNUSED(port);
	}
	/// @copydoc Pin::lo(FASTLED_REGISTER port_ptr_t)
	inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) {
		FASTLED_UNUSED(port);
	}
	/// @copydoc Pin::set(FASTLED_REGISTER port_t)
	inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) {
		FASTLED_UNUSED(val);
	}

	/// @copydoc Pin::fastset()
	inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) {
		FASTLED_UNUSED(port);
		FASTLED_UNUSED(val);
	}

	/// @copydoc Pin::hival()
	static port_t hival() __attribute__ ((always_inline)) { return 0; }
	/// @copydoc Pin::loval()
	static port_t loval() __attribute__ ((always_inline)) { return 0;}
	/// @copydoc Pin::port()
	static port_ptr_t  port() __attribute__ ((always_inline)) { return NULL; }
	/// @copydoc Pin::mask()
	static port_t mask() __attribute__ ((always_inline)) { return 0; }
};

#endif

/// FastPin implementation for bit-banded access. 
/// Only for MCUs that support bitbanding.
/// @note This bitband class is optional!
template<fl::u8 PIN> class FastPinBB : public FastPin<PIN> {};

typedef volatile fl::u32 & reg32_t;      ///< Reference to a 32-bit register, volatile
typedef volatile fl::u32 * ptr_reg32_t;  ///< Pointer to a 32-bit register, volatile

/// Utility template for tracking down information about pins and ports
/// @tparam port the port to check information for
template<fl::u8 port> struct __FL_PORT_INFO {
	/// Checks whether a port exists
	static bool hasPort() { return 0; }
	/// Gets the name of the port, as a C-string
	static const char *portName() { return "--"; }
	/// Gets the raw address of the port
	static const void *portAddr() { return NULL; }
};


/// Macro to create the instantiations for defined ports. 
/// We're going to abuse this later for auto discovery of pin/port mappings
/// for new variants.  
/// Use this for ports that are numeric in nature, e.g. GPIO0, GPIO1, etc.
/// @param L the number of the port
/// @param BASE the data type for the register
#define _FL_DEFINE_PORT(L, BASE) template<> struct __FL_PORT_INFO<L> { \
	static bool hasPort() { return 1; } \
	static const char *portName() { return #L; } \
	typedef BASE __t_baseType;  \
	static const void *portAddr() { return (void*)&__t_baseType::r(); } };

/// Macro to create the instantiations for defined ports. 
/// We're going to abuse this later for auto discovery of pin/port mappings
/// for new variants.  
/// Use this for ports that are letters. The first parameter will be the
/// letter, the second parameter will be an integer/counter of some kind.
/// This is because attempts to turn macro parameters into character constants
/// break in some compilers.
/// @param L the letter of the port
/// @param LC an integer counter
/// @param BASE the data type for the register
#define _FL_DEFINE_PORT3(L, LC, BASE) template<> struct __FL_PORT_INFO<LC> { \
	static bool hasPort() { return 1; } \
	static const char *portName() { return #L; } \
	typedef BASE __t_baseType;  \
	static const void *portAddr() { return (void*)&__t_baseType::r(); } };

FASTLED_NAMESPACE_END

#pragma GCC diagnostic pop

#endif // __INC_FASTPIN_H

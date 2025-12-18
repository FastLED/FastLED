/// @file platforms/generic_pin.h
/// Generic runtime pin access using Arduino PINMAP functions
///
/// Provides the platform-independent Pin class implementation using Arduino's
/// digitalPinToBitMask(), portOutputRegister(), etc. This is the fallback
/// implementation for platforms without specialized pin access.

#pragma once

// Include base class definitions (includes Selectable and register.h)
// Note: This must be included before using Selectable as a base class
#include "fl/fastpin_base.h"
#include "fl/unused.h"

namespace fl {

/// Naive fallback solution for low level pin access using Arduino PINMAP functions
class Pin : public Selectable {
	volatile RwReg *mPort;    ///< Output register for the pin
	volatile RoReg *mInPort;  ///< Input register for the pin
	RwReg mPinMask;  ///< Bitmask for the pin within its register
	fl::u8 mPin;    ///< Arduino digital pin number

	/// Initialize the class by retrieving the register
	/// pointers and bitmask.
	void _init() {
		#if defined(digitalPinToBitMask) && defined(portOutputRegister) && defined(portInputRegister)
		// Use PINMAP functions if available
		mPinMask = digitalPinToBitMask(mPin);
		mPort = (volatile RwReg*)portOutputRegister(digitalPinToPort(mPin));
		mInPort = (volatile RoReg*)portInputRegister(digitalPinToPort(mPin));
		#else
		// Fallback for platforms without PINMAP functions (e.g., STM32 Mbed Arduino)
		mPinMask = 1;
		mPort = nullptr;
		mInPort = nullptr;
		#endif
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
	FL_DISABLE_WARNING_VOLATILE
	inline void hi() __attribute__ ((always_inline)) {
		if (mPort) { *mPort |= mPinMask; }
		else { digitalWrite(mPin, HIGH); }
	}
	/// Set the pin state to `LOW`
	inline void lo() __attribute__ ((always_inline)) {
		if (mPort) { *mPort &= ~mPinMask; }
		else { digitalWrite(mPin, LOW); }
	}
	FL_DISABLE_WARNING_POP

	/// Toggle the pin twice to create a short pulse
	inline void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }
	/// Toggle the pin.
	/// If the pin was high, set it low. If was low, set it high.
	inline void toggle() __attribute__ ((always_inline)) {
		if (mInPort) { *mInPort = mPinMask; }
		else { digitalWrite(mPin, !digitalRead(mPin)); }
	}

	/// Set the same pin on another port to `HIGH`
	/// @param port the port to modify
	FL_DISABLE_WARNING_PUSH
	FL_DISABLE_WARNING_VOLATILE
	inline void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { *port |= mPinMask; }
	/// Set the same pin on another port to `LOW`
	/// @param port the port to modify
	inline void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { *port &= ~mPinMask; }
	FL_DISABLE_WARNING_POP
	/// Set the state of the output register
	/// @param val the state to set the output register to
	/// @note This function is not limited to the current pin! It modifies the entire register.
	FL_DISABLE_WARNING_PUSH
	FL_DISABLE_WARNING_VOLATILE
	inline void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *mPort = val; }

	/// Set the state of a port
	/// @param port the port to modify
	/// @param val the state to set the port to
	inline void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port  = val; }

	/// Gets the state of the port with this pin `HIGH`
	FL_DISABLE_WARNING_PUSH
	FL_DISABLE_WARNING_VOLATILE
	volatile port_t hival() __attribute__ ((always_inline)) {
		if (mPort) { return *mPort | mPinMask; }
		else { return HIGH; }
	}
	FL_DISABLE_WARNING_POP
	/// Gets the state of the port with this pin `LOW`
	FL_DISABLE_WARNING_PUSH
	FL_DISABLE_WARNING_VOLATILE
	volatile port_t loval() __attribute__ ((always_inline)) {
		if (mPort) { return *mPort & ~mPinMask; }
		else { return LOW; }
	}
	FL_DISABLE_WARNING_POP
	/// Get the output state of the port
	port_ptr_t  port() __attribute__ ((always_inline)) { return mPort; }
	/// Get the pin mask
	FL_DISABLE_WARNING_PUSH
	FL_DISABLE_WARNING_VOLATILE
	volatile port_t mask() __attribute__ ((always_inline)) { return mPinMask; }
	FL_DISABLE_WARNING_POP
	FL_DISABLE_WARNING_POP

	/// @copydoc Pin::hi()
	virtual void select() override { hi(); }
	/// @copydoc Pin::lo()
	virtual void release() override { lo(); }
	/// Checks if the pin is currently `HIGH`
	virtual bool isSelected() override {
		if (mPort) { return (*mPort & mPinMask) == mPinMask; }
		else { return digitalRead(mPin) == HIGH; }
	}
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

} // namespace fl

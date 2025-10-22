/// @file fl/fastpin_base.h
/// Base class definitions for FastLED pin access
///
/// Contains abstract and default implementations of pin access classes.
/// Platform-specific implementations provide specializations of these classes.

#pragma once

#include "fl/compiler_control.h"
#include "fl/unused.h"
#include "fl/register.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#ifdef ESP32
// Get rid of the endless volatile warnings in ESP32
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wvolatile"
#endif

namespace fl {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Pin access class - needs to tune for various platforms (naive fallback solution?)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/// Mixin base class that provides the default validpin() implementation for explicitly declared pins.
/// All platform-specific pin classes (_ARMPIN, _ESPPIN, etc.) should inherit from this to indicate
/// that explicitly defined pins are valid by default. Undefined pins use the default FastPin<>
/// template which returns false.
struct ValidPinBase {
	/// All explicitly declared pins are valid by default
	/// Platforms can override this in specific cases to mark pins as invalid (e.g., ground pins, UART pins)
	static constexpr bool validpin() { return true; }
};

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
#ifdef FASTLED_ALL_PINS_VALID
	constexpr static bool validpin() { return true; }
#else
	constexpr static bool validpin() { return false; }
#endif
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
	static port_ptr_t  port() __attribute__ ((always_inline)) { return nullptr; }
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
	static const void *portAddr() { return nullptr; }
};


/// Macro to create the instantiations for defined ports.
/// We're going to abuse this later for auto discovery of pin/port mappings
/// for new variants.
/// Use this for ports that are numeric in nature, e.g. GPIO0, GPIO1, etc.
/// @param L the number of the port
/// @param BASE the data type for the register
#define _FL_DEFINE_PORT(L, BASE) template<> struct __FL_PORT_INFO<L> { \
	typedef BASE __t_baseType;  \
	static bool hasPort() { return 1; } \
	static const char *portName() { return #L; } \
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
	typedef BASE __t_baseType;  \
	static bool hasPort() { return 1; } \
	static const char *portName() { return #L; } \
	static const void *portAddr() { return (void*)&__t_baseType::r(); } };

} // namespace fl

#pragma GCC diagnostic pop

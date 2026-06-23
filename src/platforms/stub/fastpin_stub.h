/// @file platforms/stub/fastpin_stub.h
/// Stub pin implementation for testing and WebAssembly targets
///
/// Provides no-op implementations of Pin and FastPin for targets that don't
/// have hardware access (testing, WebAssembly/browser, simulation).

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "fl/stl/compiler_control.h"

// Include base class definitions (includes Selectable)
#include "fl/system/fastpin_base.h"
#include "fl/stl/noexcept.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
FL_DISABLE_WARNING_VOLATILE

namespace fl {

/// Stub Pin class for no-op pin access
class Pin : public Selectable {

	void _init() FL_NOEXCEPT {
	}

public:
	Pin(int pin) FL_NOEXCEPT { FL_UNUSED(pin); }

	void setPin(int pin) FL_NOEXCEPT { FL_UNUSED(pin); }

	typedef volatile RwReg * port_ptr_t;
	typedef RwReg port_t;

	inline void setOutput() FL_NOEXCEPT { /* NOOP */ }
	inline void setInput() FL_NOEXCEPT { /* NOOP */ }
	inline void setInputPullup() FL_NOEXCEPT { /* NOOP */ }

	inline void hi() FL_NOEXCEPT __attribute__ ((always_inline)) {}
	/// Set the pin state to `LOW`
	inline void lo() FL_NOEXCEPT __attribute__ ((always_inline)) {}

	inline void strobe() FL_NOEXCEPT __attribute__ ((always_inline)) {  }
	inline void toggle() FL_NOEXCEPT __attribute__ ((always_inline)) { }

	inline void hi(FASTLED_REGISTER port_ptr_t port) FL_NOEXCEPT __attribute__ ((always_inline)) { FL_UNUSED(port); }
	inline void lo(FASTLED_REGISTER port_ptr_t port) FL_NOEXCEPT __attribute__ ((always_inline)) { FL_UNUSED(port); }
	inline void set(FASTLED_REGISTER port_t val) FL_NOEXCEPT __attribute__ ((always_inline)) { FL_UNUSED(val); }

	inline void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) FL_NOEXCEPT __attribute__ ((always_inline)) { FL_UNUSED(port); FL_UNUSED(val); }

	port_t hival() FL_NOEXCEPT __attribute__ ((always_inline)) { return 0; }
	port_t loval() FL_NOEXCEPT __attribute__ ((always_inline)) { return 0; }
	port_ptr_t  port() FL_NOEXCEPT __attribute__ ((always_inline)) {
		static volatile RwReg port = 0;
		return &port;
	}
	port_t mask() FL_NOEXCEPT __attribute__ ((always_inline)) { return 0xff; }

	virtual void select() FL_NOEXCEPT override { hi(); }
	virtual void release() FL_NOEXCEPT override { lo(); }
	virtual bool isSelected() FL_NOEXCEPT override { return true; }
};

class OutputPin : public Pin {
public:
	OutputPin(int pin) FL_NOEXCEPT : Pin(pin) { setOutput(); }
};

class InputPin : public Pin {
public:
	InputPin(int pin) FL_NOEXCEPT : Pin(pin) { setInput(); }
};

} // namespace fl

FL_DISABLE_WARNING_POP

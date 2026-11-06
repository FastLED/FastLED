#pragma once

// Internal implementation shared by the real forced FastPin branch and its
// native tests. Mapping supplies register lookup only, never GPIO operations.
#include "platforms/is_platform.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/stdint.h"
#include "fl/system/pin.h"
#include "fl/stl/static_assert.h"
#include "platforms/output_pin_policy.h" // IWYU pragma: keep

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
FL_DISABLE_WARNING_VOLATILE
FL_DISABLE_WARNING(ignored-qualifiers)

namespace fl {
namespace detail {

template <u8 PIN> struct ForcedSoftwarePinMap {
// ESP32 deliberately keeps Arduino mapper macros out of library headers.
// Choose the same portable provider in every TU, including Arduino-first
// sketches, rather than letting include order change this template (ODR).
#if !defined(FASTLED_NO_PINMAP) && !defined(FL_IS_ESP32)
    static constexpr bool kHasPinMap = true;
    static RwReg mask() { return digitalPinToBitMask(PIN); }
    static volatile RwReg *output() { return portOutputRegister(digitalPinToPort(PIN)); }
    static volatile RoReg *input() { return portInputRegister(digitalPinToPort(PIN)); }
#else
    static constexpr bool kHasPinMap = false;
    static RwReg mask() { return 0; }
    static volatile RwReg *output() { return nullptr; }
    static volatile RoReg *input() { return nullptr; }
#endif
};

template<fl::u8 PIN, class Mapping = ForcedSoftwarePinMap<PIN>>
class ForcedSoftwarePin {
	static RwReg sPinMask;            ///< Bitmask for the pin within its register
	static volatile RwReg *sPort;     ///< Output register for the pin
	static volatile RoReg *sInPort;   ///< Input register for the pin
    static FASTLED_FORCE_INLINE void validateOutputPin() {
#if defined(FL_IS_ESP32) && !defined(FASTLED_STUB_IMPL) && !defined(FL_IS_WASM)
        FL_STATIC_ASSERT(Esp32OutputPinValidity<PIN>::value,
                         "Invalid ESP32 forced output pin");
#endif
    }

	static void _init() {
		if (Mapping::kHasPinMap) {
			sPinMask = Mapping::mask();
			sPort = Mapping::output();
			sInPort = Mapping::input();
		}
	}

public:
	typedef volatile RwReg * port_ptr_t;  ///< @copydoc Pin::port_ptr_t
	typedef RwReg port_t;  ///< @copydoc Pin::port_t

	/// @copydoc Pin::setOutput()
	inline static void setOutput() {
        validateOutputPin(); _init(); (fl::pinMode)(PIN, PinMode::Output); }
	/// @copydoc Pin::setInput()
	inline static void setInput() { _init(); (fl::pinMode)(PIN, PinMode::Input); }

	/// @copydoc Pin::hi()
	inline static void hi() __attribute__ ((always_inline)) {
        validateOutputPin();
		if (sPort) { *sPort |= sPinMask; }
		else { (fl::digitalWrite)(PIN, PinValue::High); }
	}
	/// @copydoc Pin::lo()
	inline static void lo() __attribute__ ((always_inline)) {
        validateOutputPin();
		if (sPort) { *sPort &= ~sPinMask; }
		else { (fl::digitalWrite)(PIN, PinValue::Low); }
	}

	/// @copydoc Pin::strobe()
	inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

	/// @copydoc Pin::toggle()
	inline static void toggle() __attribute__ ((always_inline)) {
        validateOutputPin();
		if (sInPort) { *sInPort = sPinMask; }
		else {
			const PinValue value = (fl::digitalRead)(PIN);
			(fl::digitalWrite)(PIN, value == PinValue::High ? PinValue::Low : PinValue::High);
		}
	}

	/// @copydoc Pin::hi(FASTLED_REGISTER port_ptr_t)
	inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) {
        validateOutputPin();
		if (port) { *port |= sPinMask; }
		else { hi(); }
	}
	/// @copydoc Pin::lo(FASTLED_REGISTER port_ptr_t)
	inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) {
        validateOutputPin();
		if (port) { *port &= ~sPinMask; }
		else { lo(); }
	}
	/// @copydoc Pin::set(FASTLED_REGISTER port_t)
	inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) {
        validateOutputPin();
		if (sPort) { *sPort = val; }
		else { (fl::digitalWrite)(PIN, val ? PinValue::High : PinValue::Low); }
	}

	/// @copydoc Pin::fastset()
	inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) {
        validateOutputPin();
		if (port) { *port = val; }
		else { set(val); }
	}

	/// @copydoc Pin::hival()
	// Without a register map these are logical single-pin tokens, not snapshots
	// of a fabricated port. set/fastset interpret nonzero as HIGH in that case.
	static port_t hival() __attribute__ ((always_inline)) { return sPort ? (*sPort | sPinMask) : 1; }
	/// @copydoc Pin::loval()
	static port_t loval() __attribute__ ((always_inline)) { return sPort ? (*sPort & ~sPinMask) : 0; }
	/// @copydoc Pin::port()
	static port_ptr_t  port() __attribute__ ((always_inline)) { return sPort; }
	/// @copydoc Pin::mask()
	static port_t mask() __attribute__ ((always_inline)) { return sPinMask; }
};

template<fl::u8 PIN, class Mapping> RwReg ForcedSoftwarePin<PIN, Mapping>::sPinMask;
template<fl::u8 PIN, class Mapping> volatile RwReg *ForcedSoftwarePin<PIN, Mapping>::sPort;
template<fl::u8 PIN, class Mapping> volatile RoReg *ForcedSoftwarePin<PIN, Mapping>::sInPort;


} // namespace detail
} // namespace fl

FL_DISABLE_WARNING_POP

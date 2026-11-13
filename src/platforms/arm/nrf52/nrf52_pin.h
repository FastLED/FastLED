// IWYU pragma: private

#pragma once

/// @file nrf52_pin.h
/// @brief nRF52 pin template and software SPI policy. The includer supplies the
/// Nordic declarations (NRF_GPIO_Type, nrf_gpio_cfg and friends) and
/// FASTLED_NRF52_INLINE_ATTRIBUTE, which keeps this file testable on a host.

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/type_traits.h"
#include "fl/system/fastpin_base.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

namespace fl {

// The actual class template can then use a typename, for what is essentially a constexpr NRF_GPIO_Type*
template <u32 _MASK, typename _PORT, u8 _PORT_NUMBER, u8 _PIN_NUMBER> class _ARMPIN : public ValidPinBase {
public:
    typedef volatile u32 * port_ptr_t;
    typedef u32 port_t;

    FASTLED_NRF52_INLINE_ATTRIBUTE static void       setOutput() FL_NO_EXCEPT {
        // OK for this to be more than one instruction, as unusual to quickly switch input/output modes
        nrf_gpio_cfg(
            nrf_pin(),
            NRF_GPIO_PIN_DIR_OUTPUT,        // set pin as output
            NRF_GPIO_PIN_INPUT_DISCONNECT,  // disconnect the input buffering
            NRF_GPIO_PIN_NOPULL,            // neither pull-up nor pull-down resistors enabled
            NRF_GPIO_PIN_H0H1,              // high drive mode required for faster speeds
            NRF_GPIO_PIN_NOSENSE            // pin sense level disabled
            );
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void       setInput() FL_NO_EXCEPT {
        // OK for this to be more than one instruction, as unusual to quickly switch input/output modes
        nrf_gpio_cfg(
            nrf_pin(),
            NRF_GPIO_PIN_DIR_INPUT,         // set pin as input
            NRF_GPIO_PIN_INPUT_DISCONNECT,  // disconnect the input buffering
            NRF_GPIO_PIN_NOPULL,            // neither pull-up nor pull-down resistors enabled
            NRF_GPIO_PIN_H0H1,              // high drive mode required for faster speeds
            NRF_GPIO_PIN_NOSENSE            // pin sense level disabled
            );
    }
    FASTLED_NRF52_INLINE_ATTRIBUTE static void       hi()        { (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUTSET = _MASK;            } // sets _MASK in the SET   OUTPUT register (output set high) // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static void       lo()        { (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUTCLR = _MASK;            } // sets _MASK in the CLEAR OUTPUT register (output set low) // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static void       toggle()    { (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUT ^= _MASK;              } // toggles _MASK bits in the OUTPUT GPIO port directly // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static void       strobe()    { toggle();     toggle();                } // BUGBUG -- Is this used by FastLED?  Without knowing (for example) SPI Speed?
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     hival()     { return (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUT | _MASK;        } // sets all _MASK bit(s) in the OUTPUT GPIO port to 1 // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     loval()     { return (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUT & ~_MASK;       } // sets all _MASK bit(s) in the OUTPUT GPIO port to 0 // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t port()      { return &((reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUT);             } // gets raw pointer to OUTPUT          GPIO port // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t cport()     { return &((reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUTCLR);          } // gets raw pointer to the output-clear register // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_ptr_t sport()     { return &((reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUTSET);          } // gets raw pointer to the output-set register // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static port_t     mask()      { return _MASK;                          } // gets the value of _MASK
    FASTLED_NRF52_INLINE_ATTRIBUTE static void hi (FASTLED_REGISTER port_ptr_t port) { hi();                      } // sets _MASK in the SET   OUTPUT register (output set high)
    FASTLED_NRF52_INLINE_ATTRIBUTE static void lo (FASTLED_REGISTER port_ptr_t port) { lo();                      } // sets _MASK in the CLEAR OUTPUT register (output set low)
    FASTLED_NRF52_INLINE_ATTRIBUTE static void set(FASTLED_REGISTER port_t     val ) { (reinterpret_cast<NRF_GPIO_Type*>(_PORT::r()))->OUT = val;     } // sets entire port's value (optimization used by FastLED) // ok reinterpret cast
    FASTLED_NRF52_INLINE_ATTRIBUTE static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) { *port = val; }
    constexpr                      static u32   nrf_pin2() { return NRF_GPIO_PIN_MAP(_PORT_NUMBER, _PIN_NUMBER); }
    constexpr                      static bool       LowSpeedOnlyRecommended() FL_NO_EXCEPT {
        // Caller must always determine if high speed use if allowed on a given pin,
        // because it depends on more than just the chip packaging ... it depends on entire board (and even system) design.
        return false; // choosing default to be FALSE, to allow users to ATTEMPT to use high-speed on pins where support is not known
    }
    // Expose the nrf pin (port/pin combined), port, and pin as properties (e.g., for setting up SPI)

    FASTLED_NRF52_INLINE_ATTRIBUTE static u32   nrf_pin()  { return NRF_GPIO_PIN_MAP(_PORT_NUMBER, _PIN_NUMBER); }
};

/// True only for FastPin specializations backed by a real nRF52 _ARMPIN.
template <u32 MASK, typename PORT, u8 PORT_NUMBER, u8 PIN_NUMBER>
constexpr bool isNrf52HardwarePin(const _ARMPIN<MASK, PORT, PORT_NUMBER, PIN_NUMBER>*) { return true; }
constexpr bool isNrf52HardwarePin(const void*) { return false; }

// FL_NRF52_ENABLE_FAST_SOFTWARE_SPI: optimized bit-bang software SPI pin policy (default 0).
// UNTESTED ON HARDWARE: no bench evidence exists for this platform yet.
// Default 0 keeps the legacy bit-bang writer. Define it to 1 to enable the
// optimized path at your own risk.
// Set it as a build flag (-D / build_flags), not a #define in the sketch: the
// value must match in every translation unit, including FastLED's own sources.
#ifndef FL_NRF52_ENABLE_FAST_SOFTWARE_SPI
#define FL_NRF52_ENABLE_FAST_SOFTWARE_SPI 0
#endif
#if FL_NRF52_ENABLE_FAST_SOFTWARE_SPI
/// Data updates use OUTSET/OUTCLR. Streaming stays off: BLE and RTOS tasks can
/// change other outputs on the port, so exclusive OUT ownership cannot be assumed.
/// kMinWriteCycles stays at its default: no GPIO write latency has been measured
/// here, so existing padding is kept.
template <u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
    static constexpr bool kAtomicWrite = isNrf52HardwarePin(static_cast<const FastPin<PIN>*>(nullptr));
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void write(u8 value) FL_NO_EXCEPT {
        writeImpl<BIT>(value, fl::integral_constant<bool, kAtomicWrite>());
    }

  private:
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void writeImpl(u8 value, fl::integral_constant<bool, true>) FL_NO_EXCEPT {
        writeBitSetClear<BIT>(value, FastPin<PIN>::mask(), FastPin<PIN>::sport(), FastPin<PIN>::cport());
    }
    // A user-defined FastPin need not have sport()/cport(), so it keeps hi()/lo().
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void writeImpl(u8 value, fl::integral_constant<bool, false>) FL_NO_EXCEPT {
        BitbangPinDefaults<FastPin<PIN>>::template write<BIT>(value);
    }
};
#endif  // FL_NRF52_ENABLE_FAST_SOFTWARE_SPI

} // namespace fl

#pragma once

// IWYU pragma: private

/// @file mgm240_pin.h
/// @brief MGM240 pin template and software SPI policy. The includer supplies the
/// EMLIB declarations (GPIO, GPIO_Port_TypeDef, GPIO_Pin*), which keeps this file
/// testable on a host.

#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "fl/stl/type_traits.h"
#include "fl/system/fastpin_base.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

namespace fl {
/// Forward declaration of base FastPin template
template<u8 PIN> class FastPin;

/// Initialize GPIO clock (required for Silicon Labs EFM32/EFR32 devices)
void _mgm240_gpio_init() FL_NO_EXCEPT;

/// @brief GPIO port accessor structures
/// These provide compile-time port resolution for template-based FastPin operations
struct __generated_struct_GPIO_PORT_A {
    static constexpr GPIO_Port_TypeDef port() { return gpioPortA; }
};

struct __generated_struct_GPIO_PORT_B {
    static constexpr GPIO_Port_TypeDef port() { return gpioPortB; }
};

struct __generated_struct_GPIO_PORT_C {
    static constexpr GPIO_Port_TypeDef port() { return gpioPortC; }
};

struct __generated_struct_GPIO_PORT_D {
    static constexpr GPIO_Port_TypeDef port() { return gpioPortD; }
};

/// @brief Hardware pin template for MGM240 GPIO operations
/// @tparam _MASK Bit mask for the pin within the port (1 << pin_number)
/// @tparam _PORT_STRUCT Port accessor structure (e.g., __generated_struct_GPIO_PORT_A)
/// @tparam _PORT_NUMBER Port number (0=A, 1=B, 2=C, 3=D)
/// @tparam _PIN_NUMBER Pin number within the port (0-7 typically)
template<u32 _MASK, typename _PORT_STRUCT, u8 _PORT_NUMBER, u8 _PIN_NUMBER> class _ARMPIN : public ValidPinBase {
public:
    typedef volatile u32 * port_ptr_t;
    typedef u32 port_t;

    /// Configure pin as push-pull output
    FASTLED_FORCE_INLINE static void setOutput() FL_NO_EXCEPT {
        _mgm240_gpio_init();
        GPIO_PinModeSet(_PORT_STRUCT::port(), _PIN_NUMBER, gpioModePushPull, 0);
    }

    /// Configure pin as input
    FASTLED_FORCE_INLINE static void setInput() FL_NO_EXCEPT {
        _mgm240_gpio_init();
        GPIO_PinModeSet(_PORT_STRUCT::port(), _PIN_NUMBER, gpioModeInput, 0);
    }

    /// Set pin output high
    FASTLED_FORCE_INLINE static void hi() FL_NO_EXCEPT {
        GPIO_PinOutSet(_PORT_STRUCT::port(), _PIN_NUMBER);
    }

    /// Set pin output low
    FASTLED_FORCE_INLINE static void lo() FL_NO_EXCEPT {
        GPIO_PinOutClear(_PORT_STRUCT::port(), _PIN_NUMBER);
    }

    /// Set pin output based on value (non-zero = high, zero = low)
    FASTLED_FORCE_INLINE static void set(port_t val) FL_NO_EXCEPT {
        if(val) hi(); else lo();
    }

    /// Generate a brief pulse by toggling twice
    FASTLED_FORCE_INLINE static void strobe() { toggle(); toggle(); }

    /// Toggle pin output state
    FASTLED_FORCE_INLINE static void toggle() FL_NO_EXCEPT {
        GPIO_PinOutToggle(_PORT_STRUCT::port(), _PIN_NUMBER);
    }

    /// Set pin high (port parameter ignored for compatibility)
    FASTLED_FORCE_INLINE static void hi(port_ptr_t port) { FL_UNUSED(port); hi(); }

    /// Set pin low (port parameter ignored for compatibility)
    FASTLED_FORCE_INLINE static void lo(port_ptr_t port) { FL_UNUSED(port); lo(); }

    /// Get port value with this pin set high
    FASTLED_FORCE_INLINE static port_t hival() FL_NO_EXCEPT {
        return GPIO_PortOutGet(_PORT_STRUCT::port()) | _MASK;
    }

    /// Get port value with this pin set low
    FASTLED_FORCE_INLINE static port_t loval() FL_NO_EXCEPT {
        return GPIO_PortOutGet(_PORT_STRUCT::port()) & ~_MASK;
    }

    /// Get pointer to GPIO port output register (for direct port manipulation)
    FASTLED_FORCE_INLINE static port_ptr_t port() FL_NO_EXCEPT {
        return &(GPIO->P[_PORT_STRUCT::port()].DOUT);
    }

    /// Get pointer to atomic SET register for race-free high operations
    FASTLED_FORCE_INLINE static port_ptr_t sport() FL_NO_EXCEPT {
        return &(GPIO->P_SET[_PORT_STRUCT::port()].DOUT);
    }

    /// Get pointer to atomic CLEAR register for race-free low operations
    FASTLED_FORCE_INLINE static port_ptr_t cport() FL_NO_EXCEPT {
        return &(GPIO->P_CLR[_PORT_STRUCT::port()].DOUT);
    }

    /// Fast port write operation (used by timing-critical code)
    FASTLED_FORCE_INLINE static void fastset(port_ptr_t port, port_t val) FL_NO_EXCEPT {
        *port = val;
    }

    /// Get the bit mask for this pin within its port
    FASTLED_FORCE_INLINE static port_t mask() { return _MASK; }

    /// Read pin input state
    FASTLED_FORCE_INLINE static bool isset() FL_NO_EXCEPT {
        return GPIO_PinInGet(_PORT_STRUCT::port(), _PIN_NUMBER) != 0;
    }
};

/// True only for FastPin specializations backed by a real MGM240 _ARMPIN.
template <u32 MASK, typename PORT_STRUCT, u8 PORT_NUMBER, u8 PIN_NUMBER>
constexpr bool isMgm240HardwarePin(const _ARMPIN<MASK, PORT_STRUCT, PORT_NUMBER, PIN_NUMBER>*) { return true; }
constexpr bool isMgm240HardwarePin(const void*) { return false; }

// FL_MGM240_ENABLE_FAST_SOFTWARE_SPI: optimized bit-bang software SPI pin policy (default 0).
// UNTESTED ON HARDWARE: no bench evidence exists for this platform yet.
// Default 0 keeps the legacy bit-bang writer. Define it to 1 to enable the
// optimized path at your own risk.
// Set it as a build flag (-D / build_flags), not a #define in the sketch: the
// value must match in every translation unit, including FastLED's own sources.
#ifndef FL_MGM240_ENABLE_FAST_SOFTWARE_SPI
#define FL_MGM240_ENABLE_FAST_SOFTWARE_SPI 0
#endif
#if FL_MGM240_ENABLE_FAST_SOFTWARE_SPI
/// Exclusive same-port bulk writes stream through DOUT; all other data updates use
/// the P_SET/P_CLR aliases. kMinWriteCycles stays at its default: no GPIO write
/// latency has been measured here, so existing padding is kept.
template <u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
    static constexpr bool kAtomicWrite = isMgm240HardwarePin(static_cast<const FastPin<PIN>*>(nullptr));
    static constexpr bool kTwoWriteStream = kAtomicWrite;
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
#endif  // FL_MGM240_ENABLE_FAST_SOFTWARE_SPI

}  // namespace fl

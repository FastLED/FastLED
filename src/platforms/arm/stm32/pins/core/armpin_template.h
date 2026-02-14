#pragma once

// IWYU pragma: private

// Unified STM32 ARM pin template for all families
// Single source of truth - eliminates 90% duplication between F1/F4/F2/F7/H7/L4
//
// KEY INNOVATION: HAS_BRR template parameter
// - STM32F1/F0/F3/L0/L4/G0/G4: Have separate BRR register at offset 0x28 (HAS_BRR=true)
// - STM32F2/F4/F7/H7: No BRR register, use BSRR upper 16 bits (HAS_BRR=false)
//
// USAGE:
//   Family-specific variant files set HAS_BRR appropriately:
//   - families/stm32f1.h: Uses _ARMPIN_STM32<..., true>  (has BRR)
//   - families/stm32f4.h: Uses _ARMPIN_STM32<..., false> (no BRR)
//
// ZERO RUNTIME OVERHEAD:
//   `if constexpr` is evaluated at compile time and eliminated from binary

#include "fl/stl/stdint.h"
#include "fl/fastpin_base.h"
#include "fl/pin.h"  // For PinMode, PinValue enums
#include "fl/compiler_control.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER

namespace fl {

/// Unified template for all STM32 families.
/// @tparam PIN Arduino digital pin number
/// @tparam _BIT GPIO bit position (0-15)
/// @tparam _MASK Bitmask (1 << _BIT)
/// @tparam _GPIO GPIO port struct (GPIOA, GPIOB, etc.)
/// @tparam HAS_BRR true if family has BRR register, false if must use BSRR upper bits
template<u8 PIN, u8 _BIT, u32 _MASK, typename _GPIO, bool HAS_BRR>
class _ARMPIN_STM32 {
public:
  typedef volatile u32 * port_ptr_t;
  typedef u32 port_t;

  inline static void setOutput() { pinMode(PIN, PinMode::Output); }
  inline static void setInput() { pinMode(PIN, PinMode::Input); }

  // Set pin HIGH - use BSRR lower 16 bits (same for ALL families)
  inline static void hi() __attribute__ ((always_inline)) {
    _GPIO::r()->BSRR = _MASK;
  }

  // Set pin LOW - family-dependent implementation
  // F1/L4/G0/G4: Use dedicated BRR register
  // F2/F4/F7/H7: Use BSRR upper 16 bits (BRR doesn't exist)
  inline static void lo() __attribute__ ((always_inline)) {
    if constexpr (HAS_BRR) {
      _GPIO::r()->BRR = _MASK;         // F1, L4, G0, G4 families
    } else {
      _GPIO::r()->BSRR = (_MASK << 16); // F2, F4, F7, H7 families
    }
  }

  inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) {
    _GPIO::r()->ODR = val;
  }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }
  inline static void toggle() __attribute__ ((always_inline)) {
    if(_GPIO::r()->ODR & _MASK) { lo(); } else { hi(); }
  }

  inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) {
    *port = val;
  }

  inline static port_t hival() __attribute__ ((always_inline)) { return _GPIO::r()->ODR | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return _GPIO::r()->ODR & ~_MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_GPIO::r()->ODR; }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRR; }

  // cport() returns clear/reset port pointer (family-dependent)
  // F1/L4/G0/G4: Returns BRR register
  // F2/F4/F7/H7: Returns BSRR (caller must shift mask by 16)
  inline static port_ptr_t cport() __attribute__ ((always_inline)) {
    if constexpr (HAS_BRR) {
      return &_GPIO::r()->BRR;          // F1, L4, G0, G4 families
    } else {
      return &_GPIO::r()->BSRR;         // F2, F4, F7, H7 families (caller shifts)
    }
  }

  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
  static constexpr bool validpin() { return true; }
};

}  // namespace fl

FL_DISABLE_WARNING_POP

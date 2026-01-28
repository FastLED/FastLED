#pragma once
// allow-include-after-namespace

#warning "fastpin_stm32f4.h is deprecated. Use fastpin_dispatcher.h instead. See src/platforms/arm/stm32/pins/README.md for migration guide."

// DEPRECATED: This file is deprecated in favor of the new unified architecture
// Please use fastpin_dispatcher.h which routes to the new families/ and boards/ structure
// This file will be removed in a future release

// STM32F4 pin definitions for Black Pill, Nucleo, Discovery, and similar STM32F4 based boards
//
// CRITICAL: STM32F4 does NOT have BRR register (removed in F2/F4/F7/H7 families)
// - STM32F1/F0/F3/L0/L4/G0/G4: Have separate BRR register at offset 0x28
// - STM32F2/F4/F7/H7: BRR register does NOT exist
// - Solution: Use BSRR upper 16 bits for reset instead of BRR
//
// BOARD-SPECIFIC PIN MAPPINGS:
// - FastPin templates are indexed by Arduino digital pin NUMBERS (0, 1, 2, ...), not pin NAMES (PA0, PB0)
// - STM32duino defines PA0, PB0 as macros that expand to Arduino pin numbers (board-specific)
// - Cannot use PA0, PB0 as template parameters because they expand to PIN_A0, PIN_A1, etc.
// - Solution: Code generation from variant_*.cpp digitalPin[] array
//
// PIN MAPPING STRATEGY:
// - Generated files in generated/ directory map Arduino pin numbers → (GPIO port, bit)
// - Example: FastPin<3> maps to PA3 on BlackPill F411CE (pin 3 = PA_3)
// - Board detection uses ARDUINO_* macros defined in variant.h
//
// Sources:
// - https://www.eevblog.com/forum/microcontrollers/bsrr-in-stm32f4xx-h/
// - https://community.st.com/t5/stm32-mcus-products/rm0385-has-references-to-nonexistent-gpiox-brr-register/td-p/138531

#include "fl/stl/stdint.h"
#include "fl/fastpin_base.h"

namespace fl {

/// Template definition for STM32F4 style ARM pins with BRR compatibility layer.
/// STM32F4 does NOT have BRR register - must use BSRR upper 16 bits for reset.
template<uint8_t PIN, uint8_t _BIT, uint32_t _MASK, typename _GPIO> class _ARMPIN_F4 {
public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  inline static void setOutput() { pinMode(PIN, OUTPUT); }
  inline static void setInput() { pinMode(PIN, INPUT); }

  // Set pin HIGH - use BSRR lower 16 bits (same as F1)
  inline static void hi() __attribute__ ((always_inline)) {
    _GPIO::r()->BSRR = _MASK;
  }

  // Set pin LOW - use BSRR upper 16 bits (NOT BRR - doesn't exist on F4!)
  // CRITICAL: This is the key difference from STM32F1
  inline static void lo() __attribute__ ((always_inline)) {
    _GPIO::r()->BSRR = (_MASK << 16);
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

  // cport() returns BSRR for reset (NOT BRR) - F4 compatibility
  // Caller must shift mask by 16 bits when using this
  inline static port_ptr_t cport() __attribute__ ((always_inline)) {
    return &_GPIO::r()->BSRR;
  }

  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
  static constexpr bool validpin() { return true; }
};

#define _R(T) struct __gen_struct_ ## T
#define _DEFPIN_ARM_F4(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN_F4<PIN, BIT, 1 << BIT, _R(GPIO ## L)> {};

// STM32F4 uses GPIO_TypeDef* for STM32duino (standard HAL structure)
#define _RD32_F4(T) \
  struct __gen_struct_ ## T { \
    static __attribute__((always_inline)) FASTLED_FORCE_INLINE volatile GPIO_TypeDef* r() { \
      return T; \
    } \
  };

#define _IO32(L) _RD32_F4(GPIO ## L)

// Initialize GPIO ports based on what's available in hardware
// All STM32F4 variants have at least ports A-E
_IO32(A); _IO32(B); _IO32(C); _IO32(D); _IO32(E);

// Optional ports (not all STM32F4 variants have these)
#if defined(GPIOF)
  _IO32(F);
#endif
#if defined(GPIOG)
  _IO32(G);
#endif
#if defined(GPIOH)
  _IO32(H);
#endif
#if defined(GPIOI)
  _IO32(I);
#endif
#if defined(GPIOJ)
  _IO32(J);
#endif
#if defined(GPIOK)
  _IO32(K);
#endif

// Include board-specific pin mappings
// Board detection based on ARDUINO_* macros defined by STM32duino
#if defined(ARDUINO_BLACKPILL_F411CE)
  #include "f411ce_blackpill.h"  // nolint
#elif defined(ARDUINO_NUCLEO_F411RE)
  #include "f411re_nucleo.h"  // nolint
#elif defined(ARDUINO_BLACKPILL_F401CC) || defined(ARDUINO_BLACKPILL_F401CE)
  #include "f401cx_blackpill.h"  // nolint
#elif defined(ARDUINO_NUCLEO_F401RE)
  #include "f401re_nucleo.h"  // nolint
#elif defined(ARDUINO_DISCO_F407VG)
  #include "f407vg_disco.h"  // nolint
#elif defined(ARDUINO_NUCLEO_F446RE)
  #include "f446re_nucleo.h"  // nolint
#else
  // Fallback for unsupported boards
  #warning "STM32F4: Board variant not recognized - FastPin support may be unavailable"
  #warning "Supported boards: BLACKPILL_F411CE, BLACKPILL_F401Cx, NUCLEO_F401RE, NUCLEO_F411RE, NUCLEO_F446RE, DISCO_F407VG"
  #warning "To add support: Add pin definitions to src/platforms/arm/stm32/pins/"
#endif

#define HAS_HARDWARE_PIN_SUPPORT

}  // namespace fl

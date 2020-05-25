#ifndef __FASTPIN_ARM_STM32_H
#define __FASTPIN_ARM_STM32_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be sloightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

/// Template definition for STM32 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data output, set output, clear output, toggle output, input, and direction

typedef volatile uint32_t * port_ptr_t;
typedef uint32_t port_t;
  
template<uint8_t PIN, uint8_t _BIT, uint32_t _MASK, typename _GPIO> class _ARMPIN {
public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  inline static void setOutput() { pinMode(PIN, OUTPUT); }
  inline static void setInput() { pinMode(PIN, INPUT); }

  inline static void hi() __attribute__ ((always_inline)) { _GPIO::x()->BSRR = _MASK; }
  inline static void lo() __attribute__ ((always_inline)) { _GPIO::x()->BRR = _MASK; }

  inline static void set(register port_t val) __attribute__ ((always_inline)) { _GPIO::x()->ODR = val; }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { if(_GPIO::x()->ODR & _MASK) { lo(); } else { hi(); } }

  inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { return _GPIO::x()->ODR | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return _GPIO::x()->ODR & ~_MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_GPIO::x()->ODR; }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO::x()->BSRR; }
  inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO::x()->BRR; }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};



#if defined(STM32F1xx)
  #define _R(T) struct __gen_struct_ ## T
  #define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile GPIO_TypeDef * x() { return T; } \
                                                static __attribute__((always_inline)) inline volatile port_ptr_t r() { return &(T->ODR); } };
  #define _FL_IO(L) _RD32(GPIO ## L); 
  #define _FL_DEFPIN(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, _R(GPIO ## L)> {};
#elif defined(STM32F10X_MD) 
  #define _R(T) struct __gen_struct_ ## T
  #define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile GPIO_TypeDef * x() { return T; } \
                                               static __attribute__((always_inline)) inline volatile port_ptr_t r() { return &(T->ODR); } };
  #define _FL_IO(L,C) _RD32(GPIO ## L); _FL_DEFINE_PORT3(L, C, _R(GPIO ## L));
  #define _FL_DEFPIN(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, _R(GPIO ## L)> {};
#elif defined(__STM32F1__)
  #define _R(T) struct __gen_struct_ ## T
  #define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline gpio_reg_map* x() { return T; } \
                                               static __attribute__((always_inline)) inline volatile port_ptr_t r() { return &(T->ODR); } };
  #define _FL_IO(L,C) _RD32(GPIO ## L ## _BASE); _FL_DEFINE_PORT3(L, C, _R(GPIO ## L));
  #define _FL_DEFPIN(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, _R(GPIO ## L ## _BASE)> {};
#else
 #error "Platform not supported"
#endif



// STM32duino Core support for STM32F103
#if defined (STM32F103x6) || defined (STM32F103xB) || defined (STM32F103xE) || defined (STM32F103xG)
#include "variants/pins/stm32f103_pins.h"

// Legacy support for other arduino cores
#elif defined(__STM32F1__) || defined(SPARK) 
#include "variants/pins/stm32f103_legacy_pins.h"

#else
 #error "Board not implemented"
#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END

#endif // __INC_FASTPIN_ARM_STM32

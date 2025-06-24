#pragma once

#include "fl/stdint.h"
#include "fl/namespace.h"

FASTLED_NAMESPACE_BEGIN

/// Template definition for STM32 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data output, set output, clear output, toggle output, input, and direction

template<uint8_t PIN, uint8_t _BIT, uint32_t _MASK, typename _GPIO> class _ARMPIN {
public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  #if 0
  inline static void setOutput() {
    if(_BIT<8) {
      _CRL::r() = (_CRL::r() & (0xF << (_BIT*4)) | (0x1 << (_BIT*4)));
    } else {
      _CRH::r() = (_CRH::r() & (0xF << ((_BIT-8)*4))) | (0x1 << ((_BIT-8)*4));
    }
  }
  inline static void setInput() { /* TODO */ } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }
  #endif

  inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
  inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

  inline static void hi() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = _MASK; }
  inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BRR = _MASK; }
  // inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = (_MASK<<16); }
  inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { _GPIO::r()->ODR = val; }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { if(_GPIO::r()->ODR & _MASK) { lo(); } else { hi(); } }

  inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { return _GPIO::r()->ODR | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return _GPIO::r()->ODR & ~_MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_GPIO::r()->ODR; }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRR; }
  inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO::r()->BRR; }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

#define _R(T) struct __gen_struct_ ## T
#define _DEFPIN_ARM(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, _R(GPIO ## L)> {};

// Actual pin definitions
#if defined(SPARK)
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile GPIO_TypeDef * r() { return T; } };
#define _IO32(L) _RD32(GPIO ## L)


_IO32(A); _IO32(B); _IO32(C); _IO32(D); _IO32(E); _IO32(F); _IO32(G);


#define MAX_PIN 19
_DEFPIN_ARM(0, 7, B);
_DEFPIN_ARM(1, 6, B);
_DEFPIN_ARM(2, 5, B);
_DEFPIN_ARM(3, 4, B);
_DEFPIN_ARM(4, 3, B);
_DEFPIN_ARM(5, 15, A);
_DEFPIN_ARM(6, 14, A);
_DEFPIN_ARM(7, 13, A);
_DEFPIN_ARM(8, 8, A);
_DEFPIN_ARM(9, 9, A);
_DEFPIN_ARM(10, 0, A);
_DEFPIN_ARM(11, 1, A);
_DEFPIN_ARM(12, 4, A);
_DEFPIN_ARM(13, 5, A);
_DEFPIN_ARM(14, 6, A);
_DEFPIN_ARM(15, 7, A);
_DEFPIN_ARM(16, 0, B);
_DEFPIN_ARM(17, 1, B);
_DEFPIN_ARM(18, 3, A);
_DEFPIN_ARM(19, 2, A);


#define SPI_DATA 15
#define SPI_CLOCK 13

#define HAS_HARDWARE_PIN_SUPPORT


#elif defined(__STM32F1__) || defined(STM32F1)

// One style of register mapping
#define _RD32_REG_MAP(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) FASTLED_FORCE_INLINE volatile gpio_reg_map* r() { return T->regs; } };
// Second style of register mapping
#define _RD32_GPIO_MAP(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) FASTLED_FORCE_INLINE volatile GPIO_TypeDef* r() { return T; } };

#if defined(ARDUINO_MAPLE_MINI)
#define _RD32 _RD32_REG_MAP
#else
#define _RD32 _RD32_GPIO_MAP
#endif

#define _IO32(L) _RD32(GPIO ## L)

_IO32(A); _IO32(B); _IO32(C); _IO32(D);

#define MAX_PIN PB1

_DEFPIN_ARM(PB11, 11, B);
_DEFPIN_ARM(PB10, 10, B);
_DEFPIN_ARM(PB2, 2, B);
_DEFPIN_ARM(PB0, 0, B);
_DEFPIN_ARM(PA7, 7, A);
_DEFPIN_ARM(PA6, 6, A);
_DEFPIN_ARM(PA5, 5, A);
_DEFPIN_ARM(PA4, 4, A);
_DEFPIN_ARM(PA3, 3, A);
_DEFPIN_ARM(PA2, 2, A);
_DEFPIN_ARM(PA1, 1, A);
_DEFPIN_ARM(PA0, 0, A);
_DEFPIN_ARM(PC15, 15, C);
_DEFPIN_ARM(PC14, 14, C);
_DEFPIN_ARM(PC13, 13, C);
_DEFPIN_ARM(PB7, 7, B);
_DEFPIN_ARM(PB6, 6, B);
_DEFPIN_ARM(PB5, 5, B);
_DEFPIN_ARM(PB4, 4, B);
_DEFPIN_ARM(PB3, 3, B);
_DEFPIN_ARM(PA15, 15, A);
_DEFPIN_ARM(PA14, 14, A);
_DEFPIN_ARM(PA13, 13, A);
_DEFPIN_ARM(PA12, 12, A);
_DEFPIN_ARM(PA11, 11, A);
_DEFPIN_ARM(PA10, 10, A);
_DEFPIN_ARM(PA9, 9, A);
_DEFPIN_ARM(PA8, 8, A);
_DEFPIN_ARM(PB15, 15, B);
_DEFPIN_ARM(PB14, 14, B);
_DEFPIN_ARM(PB13, 13, B);
_DEFPIN_ARM(PB12, 12, B);
_DEFPIN_ARM(PB8, 8, B);
_DEFPIN_ARM(PB1, 1, B);

// SPI2 MOSI
#define SPI_DATA PB15
// SPI2 SCK
#define SPI_CLOCK PB13

#define HAS_HARDWARE_PIN_SUPPORT

#else
#error "Please define pin map for this board"

#endif


FASTLED_NAMESPACE_END

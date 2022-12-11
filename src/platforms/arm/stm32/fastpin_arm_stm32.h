#ifndef __FASTPIN_ARM_STM32_H
#define __FASTPIN_ARM_STM32_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

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
            _CRL::r() = (_CRL::r() & (0xF << (_BIT*4)) | (0x1 << (_BIT*4));
        } else {
            _CRH::r() = (_CRH::r() & (0xF << ((_BIT-8)*4))) | (0x1 << ((_BIT-8)*4));
        }
    }
    inline static void setInput() { /* TODO */ } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }
    #endif

    inline static void setOutput() { pinMode(PIN, OUTPUT); } // TODO: perform MUX config { _PDDR::r() |= _MASK; }
    inline static void setInput() { pinMode(PIN, INPUT); } // TODO: preform MUX config { _PDDR::r() &= ~_MASK; }

#if defined(STM32F2XX)
    inline static void hi() __attribute__ ((always_inline)) { _GPIO::r()->BSRRL = _MASK; }
    inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BSRRH = _MASK; }
#else
    inline static void hi() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = _MASK; }
    inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BRR = _MASK; }
    // inline static void lo() __attribute__ ((always_inline)) { _GPIO::r()->BSRR = (_MASK<<16); }
#endif
    inline static void set(REGISTER port_t val) __attribute__ ((always_inline)) { _GPIO::r()->ODR = val; }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { if(_GPIO::r()->ODR & _MASK) { lo(); } else { hi(); } }

    inline static void hi(REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(REGISTER port_ptr_t port, REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { return _GPIO::r()->ODR | _MASK; }
    inline static port_t loval() __attribute__ ((always_inline)) { return _GPIO::r()->ODR & ~_MASK; }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return &_GPIO::r()->ODR; }

#if defined(STM32F2XX)
    inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRRL; }
    inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRRH; }
#else
    inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPIO::r()->BSRR; }
    inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPIO::r()->BRR; }
#endif

    inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};


#define _R(T) struct __gen_struct_ ## T
#define _FL_DEFPIN(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, _R(GPIO ## L)> {};

#if defined(STM32F10X_MD)
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile GPIO_TypeDef * r() { return T; } };
#define _FL_IO(L,C) _RD32(GPIO ## L);  _FL_DEFINE_PORT3(L, C, _R(GPIO ## L));

#elif defined(__STM32F1__)
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline gpio_reg_map* r() { return T->regs; } };
#define _FL_IO(L,C) _RD32(GPIO ## L);  _FL_DEFINE_PORT3(L, C, _R(GPIO ## L));

#elif defined(STM32F2XX)
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile GPIO_TypeDef * r() { return T; } };
#define _FL_IO(L,C) _RD32(GPIO ## L);

#elif defined (STM32F1)
// stm32duino
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile GPIO_TypeDef * r() { return T; } };
#define _FL_IO(L,C) _RD32(GPIO ## L);


#else
#error "Platform not supported"
#endif

#ifdef GPIOA
_FL_IO(A,0);
#endif
#ifdef GPIOB
_FL_IO(B,1);
#endif
#ifdef GPIOC
_FL_IO(C,2);
#endif
#ifdef GPIOD
_FL_IO(D,3);
#endif
#ifdef GPIOE
_FL_IO(E,4);
#endif
#ifdef GPIOF
_FL_IO(F,5);
#endif
#ifdef GPIOG
_FL_IO(G,6);
#endif

// Actual pin definitions
#if defined(STM32F2XX) // Photon Particle

// https://github.com/focalintent/FastLED-Sparkcore/blob/master/firmware/fastpin_arm_stm32.h
#define MAX_PIN 20
_FL_DEFPIN(0, 7, B);
_FL_DEFPIN(1, 6, B);
_FL_DEFPIN(2, 5, B);
_FL_DEFPIN(3, 4, B);
_FL_DEFPIN(4, 3, B);
_FL_DEFPIN(5, 15, A);
_FL_DEFPIN(6, 14, A);
_FL_DEFPIN(7, 13, A);
_FL_DEFPIN(10, 5, C);
_FL_DEFPIN(11, 3, C);
_FL_DEFPIN(12, 2, C);
_FL_DEFPIN(13, 5, A);
_FL_DEFPIN(14, 6, A);
_FL_DEFPIN(15, 7, A);
_FL_DEFPIN(16, 4, A);
_FL_DEFPIN(17, 0, A);
_FL_DEFPIN(18, 10, A);
_FL_DEFPIN(19, 9, A);
_FL_DEFPIN(20, 7, C);

#define SPI_DATA 15
#define SPI_CLOCK 13

#define HAS_HARDWARE_PIN_SUPPORT

#elif defined(SPARK) // Sparkfun STM32F103 based board

#define MAX_PIN 19
_FL_DEFPIN(0, 7, B);
_FL_DEFPIN(1, 6, B);
_FL_DEFPIN(2, 5, B);
_FL_DEFPIN(3, 4, B);
_FL_DEFPIN(4, 3, B);
_FL_DEFPIN(5, 15, A);
_FL_DEFPIN(6, 14, A);
_FL_DEFPIN(7, 13, A);
_FL_DEFPIN(8, 8, A);
_FL_DEFPIN(9, 9, A);
_FL_DEFPIN(10, 0, A);
_FL_DEFPIN(11, 1, A);
_FL_DEFPIN(12, 4, A);
_FL_DEFPIN(13, 5, A);
_FL_DEFPIN(14, 6, A);
_FL_DEFPIN(15, 7, A);
_FL_DEFPIN(16, 0, B);
_FL_DEFPIN(17, 1, B);
_FL_DEFPIN(18, 3, A);
_FL_DEFPIN(19, 2, A);

#define SPI_DATA 15
#define SPI_CLOCK 13

#define HAS_HARDWARE_PIN_SUPPORT

#elif defined(__STM32F1__) // Generic STM32F103 aka "Blue Pill"

#define MAX_PIN 46

_FL_DEFPIN(10, 0, A);	// PA0 - PA7
_FL_DEFPIN(11, 1, A);
_FL_DEFPIN(12, 2, A);
_FL_DEFPIN(13, 3, A);
_FL_DEFPIN(14, 4, A);
_FL_DEFPIN(15, 5, A);
_FL_DEFPIN(16, 6, A);
_FL_DEFPIN(17, 7, A);
_FL_DEFPIN(29, 8, A);	// PA8 - PA15
_FL_DEFPIN(30, 9, A);
_FL_DEFPIN(31, 10, A);
_FL_DEFPIN(32, 11, A);
_FL_DEFPIN(33, 12, A);
_FL_DEFPIN(34, 13, A);
_FL_DEFPIN(37, 14, A);
_FL_DEFPIN(38, 15, A);

_FL_DEFPIN(18, 0, B);	// PB0 - PB11
_FL_DEFPIN(19, 1, B);
_FL_DEFPIN(20, 2, B);
_FL_DEFPIN(39, 3, B);
_FL_DEFPIN(40, 4, B);
_FL_DEFPIN(41, 5, B);
_FL_DEFPIN(42, 6, B);
_FL_DEFPIN(43, 7, B);
_FL_DEFPIN(45, 8, B);
_FL_DEFPIN(46, 9, B);
_FL_DEFPIN(21, 10, B);
_FL_DEFPIN(22, 11, B);

_FL_DEFPIN(2, 13, C);	// PC13 - PC15
_FL_DEFPIN(3, 14, C);
_FL_DEFPIN(4, 15, C);

#define SPI_DATA BOARD_SPI1_MOSI_PIN
#define SPI_CLOCK BOARD_SPI1_SCK_PIN

#define HAS_HARDWARE_PIN_SUPPORT

#elif defined(ARDUINO_GENERIC_F103C8TX) // stm32duino generic STM32F103C8TX
#define MAX_PIN 36

// PA0-PA15
_FL_DEFPIN(0, 0, A);
_FL_DEFPIN(1, 1, A);
_FL_DEFPIN(2, 2, A);
_FL_DEFPIN(3, 3, A);
_FL_DEFPIN(4, 4, A);
_FL_DEFPIN(5, 5, A);
_FL_DEFPIN(6, 6, A);
_FL_DEFPIN(7, 7, A);
_FL_DEFPIN(8, 8, A);
_FL_DEFPIN(9, 9, A);
_FL_DEFPIN(10, 10, A);
_FL_DEFPIN(11, 11, A);
_FL_DEFPIN(12, 12, A);
_FL_DEFPIN(13, 13, A);
_FL_DEFPIN(14, 14, A);
_FL_DEFPIN(15, 15, A);

// PB0-PB15
_FL_DEFPIN(16, 0, B);
_FL_DEFPIN(17, 1, B);
_FL_DEFPIN(18, 2, B);
_FL_DEFPIN(19, 3, B);
_FL_DEFPIN(20, 4, B);
_FL_DEFPIN(21, 5, B);
_FL_DEFPIN(22, 6, B);
_FL_DEFPIN(23, 7, B);
_FL_DEFPIN(24, 8, B);
_FL_DEFPIN(25, 9, B);
_FL_DEFPIN(26, 10, B);
_FL_DEFPIN(27, 11, B);
_FL_DEFPIN(28, 12, B);
_FL_DEFPIN(29, 13, B);
_FL_DEFPIN(30, 14, B);
_FL_DEFPIN(31, 15, B);

// PC13-PC15
_FL_DEFPIN(32, 13, C);
_FL_DEFPIN(33, 14, C);
_FL_DEFPIN(34, 15, C);

// PD0-PD1
_FL_DEFPIN(35, 0, D);
_FL_DEFPIN(36, 1, D);

// SPI2 MOSI
#define SPI_DATA PB15
// SPI2 SCK
#define SPI_CLOCK PB13

#define HAS_HARDWARE_PIN_SUPPORT

#endif // STM32F1

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END

#endif // __INC_FASTPIN_ARM_STM32

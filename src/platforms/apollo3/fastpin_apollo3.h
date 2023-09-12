#ifndef __INC_FASTPIN_APOLLO3_H
#define __INC_FASTPIN_APOLLO3_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

template<uint8_t PIN, uint8_t PAD> class _APOLLO3PIN {
public:
    typedef volatile uint32_t * port_ptr_t;
    typedef uint32_t port_t;

    inline static void setOutput() { pinMode(PIN, OUTPUT); am_hal_gpio_fastgpio_enable(PAD); }
    inline static void setInput() { am_hal_gpio_fastgpio_disable(PAD); pinMode(PIN, INPUT); }

    inline static void hi() __attribute__ ((always_inline)) { am_hal_gpio_fastgpio_set(PAD); }
    inline static void lo() __attribute__ ((always_inline)) { am_hal_gpio_fastgpio_clr(PAD); }
    inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { if(val) { am_hal_gpio_fastgpio_set(PAD); } else { am_hal_gpio_fastgpio_clr(PAD); } }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { if( am_hal_gpio_fastgpio_read(PAD)) { lo(); } else { hi(); } }

    inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { set(val); }

    inline static port_t hival() __attribute__ ((always_inline)) { return 0; }
    inline static port_t loval() __attribute__ ((always_inline)) { return 0; }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return NULL; }
    inline static port_t mask() __attribute__ ((always_inline)) { return 0; }
};

// For the Apollo3 we need to define both the pin number and the associated pad
// to avoid having to use ap3_gpio_pin2pad for fastgpio (which would slow things down)
#define _FL_DEFPIN(PIN, PAD) template<> class FastPin<PIN> : public _APOLLO3PIN<PIN, PAD> {};

// Actual (pin, pad) definitions
#if defined(ARDUINO_SFE_EDGE)

#define MAX_PIN 49
_FL_DEFPIN(0, 0); _FL_DEFPIN(1, 1); _FL_DEFPIN(3, 3); _FL_DEFPIN(4, 4);
_FL_DEFPIN(5, 5); _FL_DEFPIN(6, 6); _FL_DEFPIN(7, 7); _FL_DEFPIN(8, 8); _FL_DEFPIN(9, 9);
_FL_DEFPIN(10, 10); _FL_DEFPIN(11, 11); _FL_DEFPIN(12, 12); _FL_DEFPIN(13, 13); _FL_DEFPIN(14, 14);
_FL_DEFPIN(15, 15); _FL_DEFPIN(17, 17);
_FL_DEFPIN(20, 20); _FL_DEFPIN(21, 21); _FL_DEFPIN(22, 22); _FL_DEFPIN(23, 23); _FL_DEFPIN(24, 24);
_FL_DEFPIN(25, 25); _FL_DEFPIN(26, 26); _FL_DEFPIN(27, 27); _FL_DEFPIN(28, 28); _FL_DEFPIN(29, 29);
_FL_DEFPIN(33, 33);
_FL_DEFPIN(36, 36); _FL_DEFPIN(37, 37); _FL_DEFPIN(38, 38); _FL_DEFPIN(39, 39);
_FL_DEFPIN(40, 40); _FL_DEFPIN(42, 42); _FL_DEFPIN(43, 43); _FL_DEFPIN(44, 44);
_FL_DEFPIN(46, 46); _FL_DEFPIN(47, 47); _FL_DEFPIN(48, 48); _FL_DEFPIN(49, 49);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_SFE_EDGE2)

#define MAX_PIN 49
_FL_DEFPIN(0, 0);
_FL_DEFPIN(5, 5); _FL_DEFPIN(6, 6); _FL_DEFPIN(7, 7); _FL_DEFPIN(8, 8); _FL_DEFPIN(9, 9);
_FL_DEFPIN(11, 11); _FL_DEFPIN(12, 12); _FL_DEFPIN(13, 13); _FL_DEFPIN(14, 14);
_FL_DEFPIN(15, 15); _FL_DEFPIN(16, 16); _FL_DEFPIN(17, 17); _FL_DEFPIN(18, 18); _FL_DEFPIN(19, 19);
_FL_DEFPIN(20, 20); _FL_DEFPIN(21, 21); _FL_DEFPIN(23, 23);
_FL_DEFPIN(25, 25); _FL_DEFPIN(26, 26); _FL_DEFPIN(27, 27); _FL_DEFPIN(28, 28); _FL_DEFPIN(29, 29);
_FL_DEFPIN(31, 31); _FL_DEFPIN(32, 32); _FL_DEFPIN(33, 33); _FL_DEFPIN(34, 34);
_FL_DEFPIN(35, 35); _FL_DEFPIN(37, 37); _FL_DEFPIN(39, 39);
_FL_DEFPIN(40, 40); _FL_DEFPIN(41, 41); _FL_DEFPIN(42, 42); _FL_DEFPIN(43, 43); _FL_DEFPIN(44, 44);
_FL_DEFPIN(45, 45); _FL_DEFPIN(48, 48); _FL_DEFPIN(49, 49);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AM_AP3_SFE_BB_ARTEMIS)

#define MAX_PIN 31
_FL_DEFPIN(0, 25); _FL_DEFPIN(1, 24); _FL_DEFPIN(2, 35); _FL_DEFPIN(3, 4); _FL_DEFPIN(4, 22);
_FL_DEFPIN(5, 23); _FL_DEFPIN(6, 27); _FL_DEFPIN(7, 28); _FL_DEFPIN(8, 32); _FL_DEFPIN(9, 12);
_FL_DEFPIN(10, 13); _FL_DEFPIN(11, 7); _FL_DEFPIN(12, 6); _FL_DEFPIN(13, 5); _FL_DEFPIN(14, 40);
_FL_DEFPIN(15, 39); _FL_DEFPIN(16, 29); _FL_DEFPIN(17, 11); _FL_DEFPIN(18, 34); _FL_DEFPIN(19, 33);
_FL_DEFPIN(20, 16); _FL_DEFPIN(21, 31); _FL_DEFPIN(22, 48); _FL_DEFPIN(23, 49); _FL_DEFPIN(24, 8);
_FL_DEFPIN(25, 9); _FL_DEFPIN(26, 10); _FL_DEFPIN(27, 38); _FL_DEFPIN(28, 42); _FL_DEFPIN(29, 43);
_FL_DEFPIN(30, 36); _FL_DEFPIN(31, 37);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AM_AP3_SFE_BB_ARTEMIS_NANO)

#define MAX_PIN 23
_FL_DEFPIN(0, 13); _FL_DEFPIN(1, 33); _FL_DEFPIN(2, 11); _FL_DEFPIN(3, 29); _FL_DEFPIN(4, 18);
_FL_DEFPIN(5, 31); _FL_DEFPIN(6, 43); _FL_DEFPIN(7, 42); _FL_DEFPIN(8, 38); _FL_DEFPIN(9, 39);
_FL_DEFPIN(10, 40); _FL_DEFPIN(11, 5); _FL_DEFPIN(12, 7); _FL_DEFPIN(13, 6); _FL_DEFPIN(14, 35);
_FL_DEFPIN(15, 32); _FL_DEFPIN(16, 12); _FL_DEFPIN(17, 32); _FL_DEFPIN(18, 12); _FL_DEFPIN(19, 19);
_FL_DEFPIN(20, 48); _FL_DEFPIN(21, 49); _FL_DEFPIN(22, 36); _FL_DEFPIN(23, 37);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AM_AP3_SFE_THING_PLUS)

#define MAX_PIN 28
_FL_DEFPIN(0, 25); _FL_DEFPIN(1, 24); _FL_DEFPIN(2, 44); _FL_DEFPIN(3, 35); _FL_DEFPIN(4, 4);
_FL_DEFPIN(5, 22); _FL_DEFPIN(6, 23); _FL_DEFPIN(7, 27); _FL_DEFPIN(8, 28); _FL_DEFPIN(9, 32);
_FL_DEFPIN(10, 14); _FL_DEFPIN(11, 7); _FL_DEFPIN(12, 6); _FL_DEFPIN(13, 5); _FL_DEFPIN(14, 40);
_FL_DEFPIN(15, 39); _FL_DEFPIN(16, 43); _FL_DEFPIN(17, 42); _FL_DEFPIN(18, 26); _FL_DEFPIN(19, 33);
_FL_DEFPIN(20, 13); _FL_DEFPIN(21, 11); _FL_DEFPIN(22, 29); _FL_DEFPIN(23, 12); _FL_DEFPIN(24, 31);
_FL_DEFPIN(25, 48); _FL_DEFPIN(26, 49); _FL_DEFPIN(27, 36); _FL_DEFPIN(28, 37);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AM_AP3_SFE_BB_ARTEMIS_ATP) || defined(ARDUINO_SFE_ARTEMIS)

#define MAX_PIN 49
_FL_DEFPIN(0, 0); _FL_DEFPIN(1, 1); _FL_DEFPIN(2, 2); _FL_DEFPIN(3, 3); _FL_DEFPIN(4, 4);
_FL_DEFPIN(5, 5); _FL_DEFPIN(6, 6); _FL_DEFPIN(7, 7); _FL_DEFPIN(8, 8); _FL_DEFPIN(9, 9);
_FL_DEFPIN(10, 10); _FL_DEFPIN(11, 11); _FL_DEFPIN(12, 12); _FL_DEFPIN(13, 13); _FL_DEFPIN(14, 14);
_FL_DEFPIN(15, 15); _FL_DEFPIN(16, 16); _FL_DEFPIN(17, 17); _FL_DEFPIN(18, 18); _FL_DEFPIN(19, 19);
_FL_DEFPIN(20, 20); _FL_DEFPIN(21, 21); _FL_DEFPIN(22, 22); _FL_DEFPIN(23, 23); _FL_DEFPIN(24, 24);
_FL_DEFPIN(25, 25); _FL_DEFPIN(26, 26); _FL_DEFPIN(27, 27); _FL_DEFPIN(28, 28); _FL_DEFPIN(29, 29);
_FL_DEFPIN(31, 31); _FL_DEFPIN(32, 32); _FL_DEFPIN(33, 33); _FL_DEFPIN(34, 34);
_FL_DEFPIN(35, 35); _FL_DEFPIN(36, 36); _FL_DEFPIN(37, 37); _FL_DEFPIN(38, 38); _FL_DEFPIN(39, 39);
_FL_DEFPIN(40, 40); _FL_DEFPIN(41, 41); _FL_DEFPIN(42, 42); _FL_DEFPIN(43, 43); _FL_DEFPIN(44, 44);
_FL_DEFPIN(45, 45); _FL_DEFPIN(47, 47); _FL_DEFPIN(48, 48); _FL_DEFPIN(49, 49);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_AM_AP3_SFE_ARTEMIS_DK)

#define MAX_PIN 49
_FL_DEFPIN(0, 0); _FL_DEFPIN(1, 1); _FL_DEFPIN(2, 2); _FL_DEFPIN(3, 3); _FL_DEFPIN(4, 4);
_FL_DEFPIN(5, 5); _FL_DEFPIN(6, 6); _FL_DEFPIN(7, 7); _FL_DEFPIN(8, 8); _FL_DEFPIN(9, 9);
_FL_DEFPIN(10, 10); _FL_DEFPIN(11, 11); _FL_DEFPIN(12, 12); _FL_DEFPIN(13, 13); _FL_DEFPIN(14, 14);
_FL_DEFPIN(15, 15); _FL_DEFPIN(16, 16); _FL_DEFPIN(17, 17); _FL_DEFPIN(18, 18); _FL_DEFPIN(19, 19);
_FL_DEFPIN(20, 20); _FL_DEFPIN(21, 21); _FL_DEFPIN(22, 22); _FL_DEFPIN(23, 23); _FL_DEFPIN(24, 24);
_FL_DEFPIN(25, 25); _FL_DEFPIN(26, 26); _FL_DEFPIN(27, 27); _FL_DEFPIN(28, 28); _FL_DEFPIN(29, 29);
_FL_DEFPIN(31, 31); _FL_DEFPIN(32, 32); _FL_DEFPIN(33, 33); _FL_DEFPIN(34, 34);
_FL_DEFPIN(35, 35); _FL_DEFPIN(36, 36); _FL_DEFPIN(37, 37); _FL_DEFPIN(38, 38); _FL_DEFPIN(39, 39);
_FL_DEFPIN(40, 40); _FL_DEFPIN(41, 41); _FL_DEFPIN(42, 42); _FL_DEFPIN(43, 43); _FL_DEFPIN(44, 44);
_FL_DEFPIN(45, 45); _FL_DEFPIN(47, 47); _FL_DEFPIN(48, 48); _FL_DEFPIN(49, 49);
#define HAS_HARDWARE_PIN_SUPPORT 1

#else

#error "Unrecognised APOLLO3 board!"

#endif

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END

#endif // __INC_FASTPIN_AVR_H

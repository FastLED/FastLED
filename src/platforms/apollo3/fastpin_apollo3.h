#ifndef __INC_FASTPIN_APOLLO3_H
#define __INC_FASTPIN_APOLLO3_H

#include "fl/stl/stdint.h"
#include "fl/fastpin_base.h"

// Include Arduino core to get Apollo3 HAL function declarations
#ifdef ARDUINO
#include "Arduino.h"
#endif

namespace fl {
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
    inline static port_ptr_t  port() __attribute__ ((always_inline)) { return NULL; }
    inline static port_t mask() __attribute__ ((always_inline)) { return 0; }
};

#define _FL_DEFPIN(PIN, PAD) template<> class FastPin<PIN> : public _APOLLO3PIN<PIN, PAD> {};

_FL_DEFPIN(0, 0);
_FL_DEFPIN(1, 1);
_FL_DEFPIN(2, 2);
_FL_DEFPIN(3, 3);
_FL_DEFPIN(4, 4);
_FL_DEFPIN(5, 5);
_FL_DEFPIN(6, 6);
_FL_DEFPIN(7, 7);
_FL_DEFPIN(8, 8);
_FL_DEFPIN(9, 9);
_FL_DEFPIN(10, 10);
_FL_DEFPIN(11, 11);
_FL_DEFPIN(12, 12);
_FL_DEFPIN(13, 13);
_FL_DEFPIN(14, 14);
_FL_DEFPIN(15, 15);
_FL_DEFPIN(16, 16);
_FL_DEFPIN(17, 17);
_FL_DEFPIN(18, 18);
_FL_DEFPIN(19, 19);
_FL_DEFPIN(20, 20);
_FL_DEFPIN(21, 21);
_FL_DEFPIN(22, 22);
_FL_DEFPIN(23, 23);
_FL_DEFPIN(24, 24);
_FL_DEFPIN(25, 25);
_FL_DEFPIN(26, 26);
_FL_DEFPIN(27, 27);
_FL_DEFPIN(28, 28);
_FL_DEFPIN(29, 29);
_FL_DEFPIN(30, 30);
_FL_DEFPIN(31, 31);
_FL_DEFPIN(32, 32);

#define HAS_HARDWARE_PIN_SUPPORT

#endif

}

#endif  // __INC_FASTPIN_APOLLO3_H

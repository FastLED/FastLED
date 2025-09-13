#pragma once

#include <stdint.h>

// Include Silicon Labs EMLIB GPIO for direct register access
#include "em_gpio.h"
#include "em_cmu.h"

#include "fl/force_inline.h"
#include "fl/namespace.h"
#include "fl/unused.h"

FASTLED_NAMESPACE_BEGIN

// Initialize GPIO clock (needed for Silicon Labs devices)
void _mgm240_gpio_init();

// Hardware pin template for MGM240 using direct register access
template<uint8_t PIN, uint8_t _BIT, uint32_t _MASK, GPIO_Port_TypeDef _PORT> class _ARMPIN {
public:
    typedef volatile uint32_t * port_ptr_t;
    typedef uint32_t port_t;

    FASTLED_FORCE_INLINE static void setOutput() {
        _mgm240_gpio_init();
        GPIO_PinModeSet(_PORT, _BIT, gpioModePushPull, 0);
    }

    FASTLED_FORCE_INLINE static void setInput() {
        _mgm240_gpio_init();
        GPIO_PinModeSet(_PORT, _BIT, gpioModeInput, 0);
    }

    FASTLED_FORCE_INLINE static void hi() {
        GPIO_PinOutSet(_PORT, _BIT);
    }

    FASTLED_FORCE_INLINE static void lo() {
        GPIO_PinOutClear(_PORT, _BIT);
    }

    FASTLED_FORCE_INLINE static void set(port_t val) {
        if(val) hi(); else lo();
    }

    FASTLED_FORCE_INLINE static void strobe() { toggle(); toggle(); }
    FASTLED_FORCE_INLINE static void toggle() {
        GPIO_PinOutToggle(_PORT, _BIT);
    }

    FASTLED_FORCE_INLINE static void hi(port_ptr_t port) { FL_UNUSED(port); hi(); }
    FASTLED_FORCE_INLINE static void lo(port_ptr_t port) { FL_UNUSED(port); lo(); }
    FASTLED_FORCE_INLINE static void fastset(port_ptr_t port, port_t val) { FL_UNUSED(port); set(val); }

    FASTLED_FORCE_INLINE static port_t hival() {
        return GPIO_PortOutGet(_PORT) | _MASK;
    }

    FASTLED_FORCE_INLINE static port_t loval() {
        return GPIO_PortOutGet(_PORT) & ~_MASK;
    }

    FASTLED_FORCE_INLINE static port_ptr_t port() {
        // Return pointer to the GPIO port's DOUT register
        return &(GPIO->P[_PORT].DOUT);
    }

    FASTLED_FORCE_INLINE static port_t mask() { return _MASK; }

    FASTLED_FORCE_INLINE static bool isset() {
        return GPIO_PinInGet(_PORT, _BIT) != 0;
    }
};

// Pin definition macro for MGM240 with direct register access
#define _FL_DEFPIN(PIN, BIT, PORT, MASK) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, MASK, PORT> {};

// Arduino Nano Matter (MGM240SD22VNA) Pin Mappings
// Based on Arduino Nano form factor and typical GPIO assignments
// Note: These mappings should be verified against actual Arduino Nano Matter pinout

// Digital pins 0-13 (Arduino Nano standard layout)
_FL_DEFPIN(0,  0, gpioPortA, (1 << 0));   // D0/RX  - PA00
_FL_DEFPIN(1,  1, gpioPortA, (1 << 1));   // D1/TX  - PA01
_FL_DEFPIN(2,  2, gpioPortA, (1 << 2));   // D2     - PA02
_FL_DEFPIN(3,  3, gpioPortA, (1 << 3));   // D3/PWM - PA03
_FL_DEFPIN(4,  4, gpioPortA, (1 << 4));   // D4     - PA04
_FL_DEFPIN(5,  5, gpioPortA, (1 << 5));   // D5/PWM - PA05
_FL_DEFPIN(6,  6, gpioPortA, (1 << 6));   // D6/PWM - PA06
_FL_DEFPIN(7,  7, gpioPortA, (1 << 7));   // D7     - PA07
_FL_DEFPIN(8,  0, gpioPortB, (1 << 0));   // D8     - PB00
_FL_DEFPIN(9,  1, gpioPortB, (1 << 1));   // D9/PWM - PB01
_FL_DEFPIN(10, 2, gpioPortB, (1 << 2));   // D10/SS - PB02
_FL_DEFPIN(11, 3, gpioPortB, (1 << 3));   // D11/MOSI - PB03
_FL_DEFPIN(12, 4, gpioPortB, (1 << 4));   // D12/MISO - PB04
_FL_DEFPIN(13, 5, gpioPortB, (1 << 5));   // D13/SCK/LED - PB05

// Analog pins A0-A7 (mapped to port C)
_FL_DEFPIN(14, 0, gpioPortC, (1 << 0));   // A0 - PC00
_FL_DEFPIN(15, 1, gpioPortC, (1 << 1));   // A1 - PC01
_FL_DEFPIN(16, 2, gpioPortC, (1 << 2));   // A2 - PC02
_FL_DEFPIN(17, 3, gpioPortC, (1 << 3));   // A3 - PC03
_FL_DEFPIN(18, 4, gpioPortC, (1 << 4));   // A4/SDA - PC04
_FL_DEFPIN(19, 5, gpioPortC, (1 << 5));   // A5/SCL - PC05
_FL_DEFPIN(20, 6, gpioPortC, (1 << 6));   // A6 - PC06
_FL_DEFPIN(21, 7, gpioPortC, (1 << 7));   // A7 - PC07

// Additional GPIO pins for extended functionality (port D)
_FL_DEFPIN(22, 0, gpioPortD, (1 << 0));   // D22 - PD00
_FL_DEFPIN(23, 1, gpioPortD, (1 << 1));   // D23 - PD01
_FL_DEFPIN(24, 2, gpioPortD, (1 << 2));   // D24 - PD02
_FL_DEFPIN(25, 3, gpioPortD, (1 << 3));   // D25 - PD03

FASTLED_NAMESPACE_END

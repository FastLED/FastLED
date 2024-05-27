#ifndef __INC_FASTPIN_ARM_RENESAS_H
#define __INC_FASTPIN_ARM_RENESAS_H

FASTLED_NAMESPACE_BEGIN

#if defined(FASTLED_FORCE_SOFTWARE_PINS)
#warning "Software pin support forced, pin access will be slightly slower."
#define NO_HARDWARE_PIN_SUPPORT
#undef HAS_HARDWARE_PIN_SUPPORT

#else

#include "bsp_api.h"

/// Template definition for STM32 style ARM pins, providing direct access to the various GPIO registers.  Note that this
/// uses the full port GPIO registers.  In theory, in some way, bit-band register access -should- be faster, however I have found
/// that something about the way gcc does register allocation results in the bit-band code being slower.  It will need more fine tuning.
/// The registers are data output, set output, clear output, toggle output, input, and direction

template<uint8_t PIN, bsp_io_port_pin_t bspPin, uint32_t _PORT> class _ARMPIN {
public:

    typedef volatile uint16_t * port_ptr_t;
    typedef uint16_t port_t;

    #define PORT ((R_PORT0_Type*)(_PORT))
    #define digitalBspPinToPort(P)		   (P >> 8)
    #define digitalBspPinToBitMask(P)      (1 << (P & 0xFF))

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

    inline static void hi() __attribute__ ((always_inline)) { PORT->POSR = digitalBspPinToBitMask(bspPin); }
    inline static void lo() __attribute__ ((always_inline)) { PORT->PORR = digitalBspPinToBitMask(bspPin); }
    inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { PORT->PODR = val; }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { PORT->PODR & digitalBspPinToBitMask(bspPin) ? lo() : hi(); }

    inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { return PORT->PODR | digitalBspPinToBitMask(bspPin); }
    inline static port_t loval() __attribute__ ((always_inline)) { return PORT->PODR & ~digitalBspPinToBitMask(bspPin); }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return &PORT->PODR; }
    inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &PORT->POSR; }
    inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &PORT->PORR; }
    inline static port_t mask() __attribute__ ((always_inline)) { return digitalBspPinToBitMask(bspPin); }

};

#define _FL_DEFPIN(PIN, bspPin, PORT) template<> class FastPin<PIN> : public _ARMPIN<PIN, bspPin, PORT> {};

// Actual pin definitions
#if defined(ARDUINO_UNOR4_WIFI)

#define MAX_PIN 21
// D0-D13
_FL_DEFPIN( 0, BSP_IO_PORT_03_PIN_01, R_PORT3_BASE ); _FL_DEFPIN( 1, BSP_IO_PORT_03_PIN_02, R_PORT3_BASE ); _FL_DEFPIN( 2, BSP_IO_PORT_01_PIN_04, R_PORT1_BASE );
_FL_DEFPIN( 3, BSP_IO_PORT_01_PIN_05, R_PORT1_BASE ); _FL_DEFPIN( 4, BSP_IO_PORT_01_PIN_06, R_PORT1_BASE ); _FL_DEFPIN( 5, BSP_IO_PORT_01_PIN_07, R_PORT1_BASE );
_FL_DEFPIN( 6, BSP_IO_PORT_01_PIN_11, R_PORT1_BASE ); _FL_DEFPIN( 7, BSP_IO_PORT_01_PIN_12, R_PORT1_BASE ); _FL_DEFPIN( 8, BSP_IO_PORT_03_PIN_04, R_PORT3_BASE );
_FL_DEFPIN( 9, BSP_IO_PORT_03_PIN_03, R_PORT3_BASE ); _FL_DEFPIN(10, BSP_IO_PORT_01_PIN_03, R_PORT1_BASE ); _FL_DEFPIN(11, BSP_IO_PORT_04_PIN_11, R_PORT4_BASE );
_FL_DEFPIN(12, BSP_IO_PORT_04_PIN_10, R_PORT4_BASE ); _FL_DEFPIN(13, BSP_IO_PORT_01_PIN_02, R_PORT1_BASE );
// A0-A5
_FL_DEFPIN(14, BSP_IO_PORT_00_PIN_14, R_PORT0_BASE ); _FL_DEFPIN(15, BSP_IO_PORT_00_PIN_00, R_PORT0_BASE ); _FL_DEFPIN(16, BSP_IO_PORT_00_PIN_01, R_PORT0_BASE );
_FL_DEFPIN(17, BSP_IO_PORT_00_PIN_02, R_PORT0_BASE ); _FL_DEFPIN(18, BSP_IO_PORT_01_PIN_01, R_PORT1_BASE ); _FL_DEFPIN(19, BSP_IO_PORT_01_PIN_00, R_PORT1_BASE );

#elif defined(ARDUINO_UNOR4_MINIMA)

#define MAX_PIN 21
// D0-D13
_FL_DEFPIN( 0, BSP_IO_PORT_03_PIN_01, R_PORT3_BASE ); _FL_DEFPIN( 1, BSP_IO_PORT_03_PIN_02, R_PORT3_BASE ); _FL_DEFPIN( 2, BSP_IO_PORT_01_PIN_05, R_PORT1_BASE );
_FL_DEFPIN( 3, BSP_IO_PORT_01_PIN_04, R_PORT1_BASE ); _FL_DEFPIN( 4, BSP_IO_PORT_01_PIN_03, R_PORT1_BASE ); _FL_DEFPIN( 5, BSP_IO_PORT_01_PIN_02, R_PORT1_BASE );
_FL_DEFPIN( 6, BSP_IO_PORT_01_PIN_06, R_PORT1_BASE ); _FL_DEFPIN( 7, BSP_IO_PORT_01_PIN_07, R_PORT1_BASE ); _FL_DEFPIN( 8, BSP_IO_PORT_03_PIN_04, R_PORT3_BASE );
_FL_DEFPIN( 9, BSP_IO_PORT_03_PIN_03, R_PORT3_BASE ); _FL_DEFPIN(10, BSP_IO_PORT_01_PIN_12, R_PORT1_BASE ); _FL_DEFPIN(11, BSP_IO_PORT_01_PIN_09, R_PORT1_BASE );
_FL_DEFPIN(12, BSP_IO_PORT_01_PIN_10, R_PORT1_BASE ); _FL_DEFPIN(13, BSP_IO_PORT_01_PIN_11, R_PORT1_BASE );
// A0-A5
_FL_DEFPIN(14, BSP_IO_PORT_00_PIN_14, R_PORT0_BASE ); _FL_DEFPIN(15, BSP_IO_PORT_00_PIN_00, R_PORT0_BASE ); _FL_DEFPIN(16, BSP_IO_PORT_00_PIN_01, R_PORT0_BASE );
_FL_DEFPIN(17, BSP_IO_PORT_00_PIN_02, R_PORT0_BASE ); _FL_DEFPIN(18, BSP_IO_PORT_01_PIN_01, R_PORT1_BASE ); _FL_DEFPIN(19, BSP_IO_PORT_01_PIN_00, R_PORT1_BASE );

#elif defined(ARDUINO_THINGPLUS_RA6M5)

#define MAX_PIN 24
// D0-D06
_FL_DEFPIN( 0, BSP_IO_PORT_01_PIN_12, R_PORT1_BASE ); _FL_DEFPIN( 1, BSP_IO_PORT_04_PIN_06, R_PORT4_BASE ); _FL_DEFPIN( 2, BSP_IO_PORT_04_PIN_05, R_PORT4_BASE );
_FL_DEFPIN( 3, BSP_IO_PORT_04_PIN_04, R_PORT4_BASE ); _FL_DEFPIN( 4, BSP_IO_PORT_04_PIN_03, R_PORT4_BASE ); _FL_DEFPIN( 5, BSP_IO_PORT_04_PIN_02, R_PORT4_BASE );
_FL_DEFPIN( 6, BSP_IO_PORT_02_PIN_07, R_PORT2_BASE );

// D07-D12 (A0-A5)
_FL_DEFPIN( 7, BSP_IO_PORT_00_PIN_14, R_PORT0_BASE ); _FL_DEFPIN( 8, BSP_IO_PORT_00_PIN_15, R_PORT0_BASE ); _FL_DEFPIN( 9, BSP_IO_PORT_05_PIN_05, R_PORT5_BASE );
_FL_DEFPIN(10, BSP_IO_PORT_05_PIN_04, R_PORT5_BASE ); _FL_DEFPIN(11, BSP_IO_PORT_05_PIN_03, R_PORT5_BASE ); _FL_DEFPIN(12, BSP_IO_PORT_05_PIN_02, R_PORT5_BASE );

// D13-D21
_FL_DEFPIN(13, BSP_IO_PORT_01_PIN_05, R_PORT1_BASE ); _FL_DEFPIN(14, BSP_IO_PORT_01_PIN_06, R_PORT1_BASE ); _FL_DEFPIN(15, BSP_IO_PORT_04_PIN_01, R_PORT4_BASE );
_FL_DEFPIN(16, BSP_IO_PORT_04_PIN_00, R_PORT4_BASE ); _FL_DEFPIN(17, BSP_IO_PORT_01_PIN_10, R_PORT1_BASE ); _FL_DEFPIN(18, BSP_IO_PORT_01_PIN_09, R_PORT1_BASE );
_FL_DEFPIN(19, BSP_IO_PORT_01_PIN_11, R_PORT1_BASE ); _FL_DEFPIN(20, BSP_IO_PORT_04_PIN_09, R_PORT4_BASE ); _FL_DEFPIN(21, BSP_IO_PORT_04_PIN_08, R_PORT4_BASE );

// D30-31
_FL_DEFPIN(30, BSP_IO_PORT_03_PIN_04, R_PORT3_BASE ); _FL_DEFPIN(31, BSP_IO_PORT_04_PIN_15, R_PORT4_BASE );

#elif defined(ARDUINO_ARCH_RENESAS_PORTENTA)

#define MAX_PIN 22
// D0-D14
_FL_DEFPIN( 0, BSP_IO_PORT_01_PIN_05, R_PORT1_BASE ); _FL_DEFPIN( 1, BSP_IO_PORT_01_PIN_06, R_PORT1_BASE ); _FL_DEFPIN( 2, BSP_IO_PORT_01_PIN_01, R_PORT1_BASE );
_FL_DEFPIN( 3, BSP_IO_PORT_03_PIN_03, R_PORT3_BASE ); _FL_DEFPIN( 4, BSP_IO_PORT_04_PIN_01, R_PORT4_BASE ); _FL_DEFPIN( 5, BSP_IO_PORT_02_PIN_10, R_PORT2_BASE );
_FL_DEFPIN( 6, BSP_IO_PORT_06_PIN_01, R_PORT6_BASE ); _FL_DEFPIN( 7, BSP_IO_PORT_04_PIN_02, R_PORT4_BASE ); _FL_DEFPIN( 8, BSP_IO_PORT_09_PIN_00, R_PORT9_BASE );
_FL_DEFPIN( 9, BSP_IO_PORT_02_PIN_04, R_PORT2_BASE ); _FL_DEFPIN(10, BSP_IO_PORT_03_PIN_15, R_PORT3_BASE ); _FL_DEFPIN(11, BSP_IO_PORT_04_PIN_07, R_PORT4_BASE );
_FL_DEFPIN(12, BSP_IO_PORT_04_PIN_08, R_PORT4_BASE ); _FL_DEFPIN(13, BSP_IO_PORT_01_PIN_10, R_PORT1_BASE ); _FL_DEFPIN(14, BSP_IO_PORT_06_PIN_02, R_PORT6_BASE )
// A0-A5
_FL_DEFPIN(15, BSP_IO_PORT_00_PIN_06, R_PORT0_BASE ); _FL_DEFPIN(16, BSP_IO_PORT_00_PIN_05, R_PORT0_BASE ); _FL_DEFPIN(17, BSP_IO_PORT_00_PIN_04, R_PORT0_BASE );
_FL_DEFPIN(18, BSP_IO_PORT_00_PIN_02, R_PORT0_BASE ); _FL_DEFPIN(19, BSP_IO_PORT_01_PIN_01, R_PORT1_BASE ); _FL_DEFPIN(20, BSP_IO_PORT_00_PIN_15, R_PORT0_BASE );
_FL_DEFPIN(21, BSP_IO_PORT_00_PIN_14, R_PORT0_BASE );_FL_DEFPIN(22, BSP_IO_PORT_00_PIN_00, R_PORT0_BASE );
#endif

#define SPI_DATA 12
#define SPI_CLOCK 13

#define HAS_HARDWARE_PIN_SUPPORT 1

#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END


#endif // __INC_FASTPIN_ARM_RENESAS_H

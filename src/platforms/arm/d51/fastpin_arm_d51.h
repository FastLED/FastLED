#ifndef __INC_FASTPIN_ARM_D51_H
#define __INC_FASTPIN_ARM_D51_H

#include "force_inline.h"

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

template<uint8_t PIN, uint8_t _BIT, uint32_t _MASK, int _GRP> class _ARMPIN {
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

    inline static void hi() __attribute__ ((always_inline)) { PORT->Group[_GRP].OUTSET.reg = _MASK; }
    inline static void lo() __attribute__ ((always_inline)) { PORT->Group[_GRP].OUTCLR.reg = _MASK; }
    inline static void set(FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { PORT->Group[_GRP].OUT.reg = val; }

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { PORT->Group[_GRP].OUTTGL.reg = _MASK; }

    inline static void hi(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { return PORT->Group[_GRP].OUT.reg | _MASK; }
    inline static port_t loval() __attribute__ ((always_inline)) { return PORT->Group[_GRP].OUT.reg & ~_MASK; }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { return &PORT->Group[_GRP].OUT.reg; }
    inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &PORT->Group[_GRP].OUTSET.reg; }
    inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &PORT->Group[_GRP].OUTCLR.reg; }
    inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

#define _R(T) struct __gen_struct_ ## T
#define _RD32(T) struct __gen_struct_ ## T { static FASTLED_FORCE_INLINE volatile PortGroup * r() { return T; } };

#define _FL_IO(L) _RD32(GPIO ## L)

#define _FL_DEFPIN(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1ul << BIT, L> {};

// Actual pin definitions
#if defined(ADAFRUIT_ITSYBITSY_M4_EXPRESS)

#define MAX_PIN 19
// D0-D13, including D6+D8 (DotStar CLK + DATA)
_FL_DEFPIN( 0, 16, 0); _FL_DEFPIN( 1, 17, 0); _FL_DEFPIN( 2,  7, 0); _FL_DEFPIN( 3, 22, 1);
_FL_DEFPIN( 4, 14, 0); _FL_DEFPIN( 5, 15, 0); _FL_DEFPIN( 6,  2, 1); _FL_DEFPIN( 7, 18, 0);
_FL_DEFPIN( 8,  3, 1); _FL_DEFPIN( 9, 19, 0); _FL_DEFPIN(10, 20, 0); _FL_DEFPIN(11, 21, 0);
_FL_DEFPIN(12, 23, 0); _FL_DEFPIN(13, 22, 0);
// A0-A5
_FL_DEFPIN(14,  2, 0); _FL_DEFPIN(15,  5, 0); _FL_DEFPIN(16,  8, 1); _FL_DEFPIN(17,  9, 1);
_FL_DEFPIN(18,  4, 0); _FL_DEFPIN(19,  6, 0); /* A6 is present in variant.h but couldn't find it on the schematic */
// SDA/SCL
_FL_DEFPIN(21, 12, 0); _FL_DEFPIN(22, 13, 0);

// 23..25  MISO/SCK/MOSI
_FL_DEFPIN(23, 23, 1); _FL_DEFPIN(24,  1, 0); _FL_DEFPIN(25,  0, 0);

#define SPI_DATA 25
#define SPI_CLOCK 24

#define HAS_HARDWARE_PIN_SUPPORT 1

// Actual pin definitions
#elif defined(ADAFRUIT_METRO_M4_AIRLIFT_LITE)

#define MAX_PIN 20
// D0-D13, including D6+D8 (DotStar CLK + DATA)
_FL_DEFPIN( 0, 23, 0); _FL_DEFPIN( 1, 22, 0); _FL_DEFPIN( 2,  17, 1); _FL_DEFPIN( 3, 16, 1);
_FL_DEFPIN( 4, 13, 1); _FL_DEFPIN( 5, 14, 1); _FL_DEFPIN( 6,  15, 1); _FL_DEFPIN( 7, 12, 1);
_FL_DEFPIN( 8,  21, 0); _FL_DEFPIN( 9, 20, 0); _FL_DEFPIN(10, 18, 0); _FL_DEFPIN(11, 19, 0);
_FL_DEFPIN(12, 17, 0); _FL_DEFPIN(13, 16, 0);
// A0-A5
_FL_DEFPIN(14,  2, 0); _FL_DEFPIN(15,  5, 0); _FL_DEFPIN(16,  6, 0); _FL_DEFPIN(17,  0, 1);
_FL_DEFPIN(18,  8, 1); _FL_DEFPIN(19,  9, 1);
// SDA/SCL
_FL_DEFPIN(22, 2, 1); _FL_DEFPIN(23, 3, 1);

// 23..25  MISO/SCK/MOSI
_FL_DEFPIN(24, 14, 0); _FL_DEFPIN(25,  13, 0); _FL_DEFPIN(26,  12, 0);

#define SPI_DATA 26
#define SPI_CLOCK 25

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ADAFRUIT_FEATHER_M4_CAN)

#define MAX_PIN 19
// D0-D13, including D8 (neopixel)  no pins 2 3
_FL_DEFPIN( 0, 17, 1); _FL_DEFPIN( 1, 16, 1);
_FL_DEFPIN( 4, 14, 0); _FL_DEFPIN( 5, 16, 0); _FL_DEFPIN( 6,  18, 0);
_FL_DEFPIN( 7,  3, 1); _FL_DEFPIN( 8,  2, 1); _FL_DEFPIN( 9, 19, 0); _FL_DEFPIN(10, 20, 0); _FL_DEFPIN(11, 21, 0);
_FL_DEFPIN(12, 22, 0); _FL_DEFPIN(13, 23, 0);
// A0-A5
_FL_DEFPIN(14,  2, 0); _FL_DEFPIN(15,  5, 0); _FL_DEFPIN(16,  8, 1); _FL_DEFPIN(17,  9, 1);
_FL_DEFPIN(18,  4, 0); _FL_DEFPIN(19,  6, 0); /* A6 is present in variant.h but couldn't find it on the schematic */
// SDA/SCL
_FL_DEFPIN(21, 12, 0); _FL_DEFPIN(22, 13, 0);
// 23..25  MISO/MOSI/SCK
_FL_DEFPIN(23, 22, 1); _FL_DEFPIN(24,  23, 1); _FL_DEFPIN(25,  17, 0);

#define SPI_DATA 24
#define SPI_CLOCK 25

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ADAFRUIT_FEATHER_M4_EXPRESS)

#define MAX_PIN 19
// D0-D13, including D8 (neopixel)  no pins 2 3
_FL_DEFPIN( 0, 17, 1); _FL_DEFPIN( 1, 16, 1);
_FL_DEFPIN( 4, 14, 0); _FL_DEFPIN( 5, 16, 0); _FL_DEFPIN( 6,  18, 0);
_FL_DEFPIN( 8,  3, 1); _FL_DEFPIN( 9, 19, 0); _FL_DEFPIN(10, 20, 0); _FL_DEFPIN(11, 21, 0);
_FL_DEFPIN(12, 22, 0); _FL_DEFPIN(13, 23, 0);
// A0-A5
_FL_DEFPIN(14,  2, 0); _FL_DEFPIN(15,  5, 0); _FL_DEFPIN(16,  8, 1); _FL_DEFPIN(17,  9, 1);
_FL_DEFPIN(18,  4, 0); _FL_DEFPIN(19,  6, 0); /* A6 is present in variant.h but couldn't find it on the schematic */
// SDA/SCL
_FL_DEFPIN(21, 12, 0); _FL_DEFPIN(22, 13, 0);
// 23..25  MISO/MOSI/SCK
_FL_DEFPIN(23, 22, 1); _FL_DEFPIN(24,  23, 1); _FL_DEFPIN(25,  17, 0);

#define SPI_DATA 24
#define SPI_CLOCK 25

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(SEEED_WIO_TERMINAL)

#define MAX_PIN 9
// D0/A0-D8/A8
_FL_DEFPIN( 0,  8, 1); _FL_DEFPIN( 1,  9, 1); _FL_DEFPIN( 2,  7, 0); _FL_DEFPIN( 3,  4, 1);
_FL_DEFPIN( 4,  5, 1); _FL_DEFPIN( 5,  6, 1); _FL_DEFPIN( 6,  4, 0); _FL_DEFPIN( 7,  7, 1);
_FL_DEFPIN( 8,  6, 0);
// SDA/SCL
_FL_DEFPIN(12, 17, 0); _FL_DEFPIN(13, 16, 0);
// match GPIO pin nubers 9..11  MISO/MOSI/SCK
_FL_DEFPIN(PIN_SPI_MISO, 0, 1); _FL_DEFPIN(PIN_SPI_MOSI, 2, 1); _FL_DEFPIN(PIN_SPI_SCK, 3, 1);

#define SPI_DATA PIN_SPI_MOSI
#define SPI_CLOCK PIN_SPI_SCK

#define ARDUNIO_CORE_SPI
#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ADAFRUIT_MATRIXPORTAL_M4_EXPRESS)

#define MAX_PIN 21
// 0/1 - SERCOM/UART (Serial1)
_FL_DEFPIN( 0,  1, 0); _FL_DEFPIN( 1,  0, 0);
// 2..3 buttons
_FL_DEFPIN( 2, 22, 1); _FL_DEFPIN( 3, 23, 1);
// 4 neopixel
_FL_DEFPIN( 4, 23, 0);
// SDA/SCL
_FL_DEFPIN( 5, 31, 1); _FL_DEFPIN( 6, 30, 1);
// 7..12 RGBRGB pins
_FL_DEFPIN( 7,  0, 1); _FL_DEFPIN( 8,  1, 1); _FL_DEFPIN( 9,  2, 1); _FL_DEFPIN(10,  3, 1);
_FL_DEFPIN(11,  4, 1); _FL_DEFPIN(12,  5, 1);
// 13 LED
_FL_DEFPIN(13, 14, 0);
// 14..21  Control pins
_FL_DEFPIN(14,  6, 1); _FL_DEFPIN(15, 14, 1); _FL_DEFPIN(16, 12, 1); _FL_DEFPIN(17,  7, 1);
_FL_DEFPIN(18,  8, 1); _FL_DEFPIN(19,  9, 1); _FL_DEFPIN(20, 15, 1); _FL_DEFPIN(21, 13, 1);
// 22..26 Analog pins
_FL_DEFPIN(22,  2, 1); _FL_DEFPIN(23,  5, 1); _FL_DEFPIN(24,  4, 1); _FL_DEFPIN(25,  6, 1);
_FL_DEFPIN(26,  7, 1);
// 34..36 ESP SPI
_FL_DEFPIN(34, 16, 0); _FL_DEFPIN(35, 17, 0); _FL_DEFPIN(36, 19, 0); 
// 48..50 external SPI #2 on sercom 0
_FL_DEFPIN(48,  5, 0); _FL_DEFPIN(49,  4, 0); _FL_DEFPIN(50,  7, 0); 

#define SPI_DATA 4
#define SPI_CLOCK 7

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ADAFRUIT_GRAND_CENTRAL_M4)

#define MAX_PIN 54
// D0..D7
_FL_DEFPIN( 0, 25, 1); _FL_DEFPIN( 1, 24, 1); _FL_DEFPIN( 2, 18, 2); _FL_DEFPIN( 3, 19, 2);
_FL_DEFPIN( 4, 20, 2); _FL_DEFPIN( 5, 21, 2); _FL_DEFPIN( 6, 20, 3); _FL_DEFPIN( 7, 21, 3);
// D8..D13
_FL_DEFPIN( 8, 18, 1); _FL_DEFPIN( 9,  2, 1); _FL_DEFPIN(10, 22, 1);
_FL_DEFPIN(11, 23, 1); _FL_DEFPIN(12,  0, 1); _FL_DEFPIN(13,  1, 0);
// D14..D21
_FL_DEFPIN(14, 16, 1); _FL_DEFPIN(15, 17, 1); _FL_DEFPIN(16, 22, 2); _FL_DEFPIN(17, 23, 2);
_FL_DEFPIN(18, 12, 1); _FL_DEFPIN(19, 13, 1); _FL_DEFPIN(20, 20, 1); _FL_DEFPIN(21, 21, 1);
// D22..D53
_FL_DEFPIN(22, 12, 3); _FL_DEFPIN(23, 15, 0); _FL_DEFPIN(24, 17, 2); _FL_DEFPIN(25, 16, 2);
_FL_DEFPIN(26, 12, 0); _FL_DEFPIN(27, 13, 0); _FL_DEFPIN(28, 14, 0); _FL_DEFPIN(29, 19, 1);
_FL_DEFPIN(30, 23, 0); _FL_DEFPIN(31, 22, 0); _FL_DEFPIN(32, 21, 0); _FL_DEFPIN(33, 20, 0);
_FL_DEFPIN(34, 19, 0); _FL_DEFPIN(35, 18, 0); _FL_DEFPIN(36, 17, 0); _FL_DEFPIN(37, 16, 0);
_FL_DEFPIN(38, 15, 1); _FL_DEFPIN(39, 14, 1); _FL_DEFPIN(40, 13, 2); _FL_DEFPIN(41, 12, 2);
_FL_DEFPIN(42, 15, 2); _FL_DEFPIN(43, 14, 2); _FL_DEFPIN(44, 11, 2); _FL_DEFPIN(45, 10, 2);
_FL_DEFPIN(46,  6, 2); _FL_DEFPIN(47,  7, 2); _FL_DEFPIN(48,  4, 2); _FL_DEFPIN(49,  5, 2);
_FL_DEFPIN(50, 11, 3); _FL_DEFPIN(51,  8, 3); _FL_DEFPIN(52,  9, 3); _FL_DEFPIN(53, 10, 3);

#define SPI_DATA 51
#define SPI_CLOCK 52

#define HAS_HARDWARE_PIN_SUPPORT 1

#endif


#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END


#endif // __INC_FASTPIN_ARM_D51_H

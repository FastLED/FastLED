#ifndef __INC_FASTPIN_ARM_SAM_H
#define __INC_FASTPIN_ARM_SAM_H

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

  inline static void hi() __attribute__ ((always_inline)) { PORT_IOBUS->Group[_GRP].OUTSET.reg = _MASK; }
  inline static void lo() __attribute__ ((always_inline)) { PORT_IOBUS->Group[_GRP].OUTCLR.reg = _MASK; }
  inline static void set(register port_t val) __attribute__ ((always_inline)) { PORT_IOBUS->Group[_GRP].OUT.reg = val; }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) { PORT_IOBUS->Group[_GRP].OUTTGL.reg = _MASK; }

  inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { return PORT_IOBUS->Group[_GRP].OUT.reg | _MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return PORT_IOBUS->Group[_GRP].OUT.reg & ~_MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return &PORT_IOBUS->Group[_GRP].OUT.reg; }
  inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &PORT_IOBUS->Group[_GRP].OUTSET.reg; }
  inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &PORT_IOBUS->Group[_GRP].OUTCLR.reg; }
  inline static port_t mask() __attribute__ ((always_inline)) { return _MASK; }
};

#define _R(T) struct __gen_struct_ ## T
#define _RD32(T) struct __gen_struct_ ## T { static __attribute__((always_inline)) inline volatile PortGroup * r() { return T; } };

#define _FL_IO(L) _RD32(GPIO ## L)

#define _FL_DEFPIN(PIN, BIT, L) template<> class FastPin<PIN> : public _ARMPIN<PIN, BIT, 1 << BIT, L> {};

// Actual pin definitions
#if defined(ARDUINO_SAMD_CIRCUITPLAYGROUND_EXPRESS)

#define MAX_PIN 17
_FL_DEFPIN( 8,23,1);
_FL_DEFPIN( 0, 9,1); _FL_DEFPIN( 1, 8,1); _FL_DEFPIN( 2, 2,1); _FL_DEFPIN( 3, 3,1);
_FL_DEFPIN( 6, 5,0); _FL_DEFPIN( 9, 6,0); _FL_DEFPIN(10, 7,0); _FL_DEFPIN(12, 2,0);
_FL_DEFPIN(A6, 9,1); _FL_DEFPIN(A7, 8,1); _FL_DEFPIN(A5, 2,1); _FL_DEFPIN(A4, 3,1);
_FL_DEFPIN(A1, 5,0); _FL_DEFPIN(A2, 6,0); _FL_DEFPIN(A3, 7,0); _FL_DEFPIN(A0, 2,0);

#define HAS_HARDWARE_PIN_SUPPORT 1


#elif defined(ADAFRUIT_HALLOWING)

#define MAX_PIN 20
// 0 & 1
_FL_DEFPIN( 0, 9, 0);    _FL_DEFPIN( 1, 10, 0);
// 2, 3, 4
_FL_DEFPIN( 2, 14, 0);   _FL_DEFPIN( 3, 11, 0);   _FL_DEFPIN( 4, 8, 0);
// 5, 6, 7
_FL_DEFPIN( 5, 15, 0);   _FL_DEFPIN( 6, 18, 0);   _FL_DEFPIN( 7, 0, 0);
// 8, 9, 10
_FL_DEFPIN( 8, 12, 0);   _FL_DEFPIN( 9, 19, 0);   _FL_DEFPIN(10, 20, 0);
// 11, 12, 13
_FL_DEFPIN(11, 21, 0);   _FL_DEFPIN(12, 22, 0);   _FL_DEFPIN(13, 23, 0);
// 14, 15, 16 (A0 - A2)
_FL_DEFPIN(14,  2, 0);   _FL_DEFPIN(15,  8, 1);   _FL_DEFPIN(16, 9, 1);
// 17, 18, 19 (A3 - A5)
_FL_DEFPIN(17,  4, 0);   _FL_DEFPIN(18,  5, 0);   _FL_DEFPIN(19, 6, 0);

#define SPI_DATA  PIN_SPI_MOSI
#define SPI_CLOCK PIN_SPI_SCK

#define HAS_HARDWARE_PIN_SUPPORT 1


#elif defined(ARDUINO_SAMD_ZERO)

#define MAX_PIN 42
_FL_DEFPIN( 0,10,0); _FL_DEFPIN( 1,11,0); _FL_DEFPIN( 2, 8,0); _FL_DEFPIN( 3, 9,0);
_FL_DEFPIN( 4,14,0); _FL_DEFPIN( 5,15,0); _FL_DEFPIN( 6,20,0); _FL_DEFPIN( 7,21,0);
_FL_DEFPIN( 8, 6,0); _FL_DEFPIN( 9, 7,0); _FL_DEFPIN(10,18,0); _FL_DEFPIN(11,16,0);
_FL_DEFPIN(12,19,0); _FL_DEFPIN(13,17,0); _FL_DEFPIN(14, 2,0); _FL_DEFPIN(15, 8,1);
_FL_DEFPIN(16, 9,1); _FL_DEFPIN(17, 4,0); _FL_DEFPIN(18, 5,0); _FL_DEFPIN(19, 2,1);
_FL_DEFPIN(20,22,0); _FL_DEFPIN(21,23,0); _FL_DEFPIN(22,12,0); _FL_DEFPIN(23,11,1);
_FL_DEFPIN(24,10,1); _FL_DEFPIN(25, 3,1); _FL_DEFPIN(26,27,0); _FL_DEFPIN(27,28,0);
_FL_DEFPIN(28,24,0); _FL_DEFPIN(29,25,0); _FL_DEFPIN(30,22,1); _FL_DEFPIN(31,23,1);
_FL_DEFPIN(32,22,0); _FL_DEFPIN(33,23,0); _FL_DEFPIN(34,19,0); _FL_DEFPIN(35,16,0);
_FL_DEFPIN(36,18,0); _FL_DEFPIN(37,17,0); _FL_DEFPIN(38,13,0); _FL_DEFPIN(39,21,0);
_FL_DEFPIN(40, 6,0); _FL_DEFPIN(41, 7,0); _FL_DEFPIN(42, 3,0);

#define SPI_DATA 24
#define SPI_CLOCK 23

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_SODAQ_AUTONOMO)

#define MAX_PIN 56
_FL_DEFPIN( 0, 9,0); _FL_DEFPIN( 1,10,0); _FL_DEFPIN( 2,11,0); _FL_DEFPIN( 3,10,1);
_FL_DEFPIN( 4,11,1); _FL_DEFPIN( 5,12,1); _FL_DEFPIN( 6,13,1); _FL_DEFPIN( 7,14,1);
_FL_DEFPIN( 8,15,1); _FL_DEFPIN( 9,14,0); _FL_DEFPIN(10,15,0); _FL_DEFPIN(11,16,0);
_FL_DEFPIN(12,17,0); _FL_DEFPIN(13,18,0); _FL_DEFPIN(14,19,0); _FL_DEFPIN(15,16,1);
_FL_DEFPIN(16, 8,0); _FL_DEFPIN(17,28,0); _FL_DEFPIN(18,17,1); _FL_DEFPIN(19, 2,0);
_FL_DEFPIN(20, 6,0); _FL_DEFPIN(21, 5,0); _FL_DEFPIN(22, 4,0); _FL_DEFPIN(23, 9,1);
_FL_DEFPIN(24, 8,1); _FL_DEFPIN(25, 7,1); _FL_DEFPIN(26, 6,1); _FL_DEFPIN(27, 5,1);
_FL_DEFPIN(28, 4,1); _FL_DEFPIN(29, 7,0); _FL_DEFPIN(30, 3,1); _FL_DEFPIN(31, 2,1);
_FL_DEFPIN(32, 1,1); _FL_DEFPIN(33, 0,1); _FL_DEFPIN(34, 3,0); _FL_DEFPIN(35, 3,0);
_FL_DEFPIN(36,30,1); _FL_DEFPIN(37,31,1); _FL_DEFPIN(38,22,1); _FL_DEFPIN(39,23,1);
_FL_DEFPIN(40,12,0); _FL_DEFPIN(41,13,0); _FL_DEFPIN(42,22,0); _FL_DEFPIN(43,23,0);
_FL_DEFPIN(44,20,0); _FL_DEFPIN(45,21,0); _FL_DEFPIN(46,27,0); _FL_DEFPIN(47,24,0);
_FL_DEFPIN(48,25,0); _FL_DEFPIN(49,13,1); _FL_DEFPIN(50,14,1); _FL_DEFPIN(51,17,0);
_FL_DEFPIN(52,18,0); _FL_DEFPIN(53,12,1); _FL_DEFPIN(54,13,1); _FL_DEFPIN(55,14,1);
_FL_DEFPIN(56,15,1);

#define SPI_DATA 44
#define SPI_CLOCK 45

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_SAMD_WINO)

#define MAX_PIN 22
_FL_DEFPIN(  0, 23, 0); _FL_DEFPIN(  1, 22, 0); _FL_DEFPIN(  2, 16, 0); _FL_DEFPIN(  3, 17, 0);
_FL_DEFPIN(  4, 18, 0); _FL_DEFPIN(  5, 19, 0); _FL_DEFPIN(  6, 24, 0); _FL_DEFPIN(  7, 25, 0);
_FL_DEFPIN(  8, 27, 0); _FL_DEFPIN(  9, 28, 0); _FL_DEFPIN( 10, 30, 0); _FL_DEFPIN( 11, 31, 0);
_FL_DEFPIN( 12, 15, 0); _FL_DEFPIN( 13, 14, 0); _FL_DEFPIN( 14,  2, 0); _FL_DEFPIN( 15,  3, 0);
_FL_DEFPIN( 16,  4, 0); _FL_DEFPIN( 17,  5, 0); _FL_DEFPIN( 18,  6, 0); _FL_DEFPIN( 19,  7, 0);
_FL_DEFPIN( 20,  8, 0); _FL_DEFPIN( 21,  9, 0); _FL_DEFPIN( 22, 10, 0); _FL_DEFPIN( 23, 11, 0);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_SAMD_MKR1000) || defined(ARDUINO_SAMD_MKRWIFI1010)

#define MAX_PIN 22
_FL_DEFPIN(  0, 22, 0); _FL_DEFPIN(  1, 23, 0); _FL_DEFPIN(  2, 10, 0); _FL_DEFPIN(  3, 11, 0);
_FL_DEFPIN(  4, 10, 1); _FL_DEFPIN(  5, 11, 1); _FL_DEFPIN(  6, 20, 0); _FL_DEFPIN(  7, 21, 0);
_FL_DEFPIN(  8, 16, 0); _FL_DEFPIN(  9, 17, 0); _FL_DEFPIN( 10, 19, 0); _FL_DEFPIN( 11,  8, 0);
_FL_DEFPIN( 12,  9, 0); _FL_DEFPIN( 13, 23, 1); _FL_DEFPIN( 14, 22, 1); _FL_DEFPIN( 15,  2, 0);
_FL_DEFPIN( 16,  2, 1); _FL_DEFPIN( 17,  3, 1); _FL_DEFPIN( 18,  4, 0); _FL_DEFPIN( 19,  5, 0);
_FL_DEFPIN( 20,  6, 0); _FL_DEFPIN( 21,  7, 0);

#define SPI_DATA 8
#define SPI_CLOCK 9

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_SAMD_NANO_33_IOT)

#define MAX_PIN 26
_FL_DEFPIN(  0, 23, 1); _FL_DEFPIN(  1, 22, 1); _FL_DEFPIN(  2, 10, 1); _FL_DEFPIN(  3, 11, 1);
_FL_DEFPIN(  4,  7, 0); _FL_DEFPIN(  5,  5, 0); _FL_DEFPIN(  6,  4, 0); _FL_DEFPIN(  7,  6, 0);
_FL_DEFPIN(  8, 18, 0); _FL_DEFPIN(  9, 20, 0); _FL_DEFPIN( 10, 21, 0); _FL_DEFPIN( 11, 16, 0);
_FL_DEFPIN( 12, 19, 0); _FL_DEFPIN( 13, 17, 0); _FL_DEFPIN( 14,  2, 0); _FL_DEFPIN( 15,  2, 1);
_FL_DEFPIN( 16, 11, 1); _FL_DEFPIN( 17, 10, 0); _FL_DEFPIN( 18,  8, 1); _FL_DEFPIN( 19,  9, 1);
_FL_DEFPIN( 20,  9, 0); _FL_DEFPIN( 21,  3, 1); _FL_DEFPIN( 22, 12, 0); _FL_DEFPIN( 23, 13, 0);
_FL_DEFPIN( 24, 14, 0); _FL_DEFPIN( 25, 15, 0);

#define SPI_DATA 22
#define SPI_CLOCK 25

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ARDUINO_GEMMA_M0)

#define MAX_PIN 4
_FL_DEFPIN( 0, 4, 0); _FL_DEFPIN( 1, 2, 0); _FL_DEFPIN( 2, 5, 0);
_FL_DEFPIN( 3, 0, 0); _FL_DEFPIN( 4, 1, 0);

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ADAFRUIT_TRINKET_M0)

#define MAX_PIN 7
_FL_DEFPIN( 0, 8, 0); _FL_DEFPIN( 1, 2, 0); _FL_DEFPIN( 2, 9, 0);
_FL_DEFPIN( 3, 7, 0); _FL_DEFPIN( 4, 6, 0); _FL_DEFPIN( 7, 0, 0); _FL_DEFPIN( 8, 1, 0);

#define SPI_DATA  4
#define SPI_CLOCK 3

#define HAS_HARDWARE_PIN_SUPPORT 1

#elif defined(ADAFRUIT_ITSYBITSY_M0)

#define MAX_PIN 16
_FL_DEFPIN( 2, 14, 0); _FL_DEFPIN( 3, 9, 0); _FL_DEFPIN( 4, 8, 0);
_FL_DEFPIN( 5, 15, 0); _FL_DEFPIN( 6, 20, 0); _FL_DEFPIN( 7, 21, 0);
_FL_DEFPIN( 8, 6, 0); _FL_DEFPIN( 9, 7, 0); _FL_DEFPIN( 10, 18, 0);
_FL_DEFPIN( 11, 16, 0); _FL_DEFPIN( 12, 19, 0); _FL_DEFPIN( 13, 17, 0);
_FL_DEFPIN( 29, 10, 0); // MOSI
_FL_DEFPIN( 30, 11, 0); // SCK
_FL_DEFPIN( 40, 0, 0); //APA102 Clock
_FL_DEFPIN( 41, 0, 1) //APA102 Data

#define SPI_DATA  29
#define SPI_CLOCK 30

#define HAS_HARDWARE_PIN_SUPPORT 1

#endif


#endif // FASTLED_FORCE_SOFTWARE_PINS

FASTLED_NAMESPACE_END


#endif // __INC_FASTPIN_ARM_SAM_H

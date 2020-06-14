#pragma once

FASTLED_NAMESPACE_BEGIN

template<uint8_t PIN, uint32_t MASK> class _ESPPIN {

public:
  typedef volatile uint32_t * port_ptr_t;
  typedef uint32_t port_t;

  inline static void setOutput() { pinMode(PIN, OUTPUT); }
  inline static void setInput() { pinMode(PIN, INPUT); }

  inline static void hi() __attribute__ ((always_inline)) {
      if (PIN < 32) GPIO.out_w1ts = MASK;
      else GPIO.out1_w1ts.val = MASK;
  }

  inline static void lo() __attribute__ ((always_inline)) {
      if (PIN < 32) GPIO.out_w1tc = MASK;
      else GPIO.out1_w1tc.val = MASK;
  }

  inline static void set(register port_t val) __attribute__ ((always_inline)) {
      if (PIN < 32) GPIO.out = val;
      else GPIO.out1.val = val;
  }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) {
      if(PIN < 32) { GPIO.out ^= MASK; }
      else { GPIO.out1.val ^=MASK; }
  }

  inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) {
      if (PIN < 32) return GPIO.out | MASK;
      else return GPIO.out1.val | MASK;
  }

  inline static port_t loval() __attribute__ ((always_inline)) {
      if (PIN < 32) return GPIO.out & ~MASK;
      else return GPIO.out1.val & ~MASK;
  }

  inline static port_ptr_t port() __attribute__ ((always_inline)) {
      if (PIN < 32) return &GPIO.out;
      else return &GPIO.out1.val;
  }

  inline static port_ptr_t sport() __attribute__ ((always_inline)) {
      if (PIN < 32) return &GPIO.out_w1ts;
      else return &GPIO.out1_w1ts.val;
  }

  inline static port_ptr_t cport() __attribute__ ((always_inline)) {
      if (PIN < 32) return &GPIO.out_w1tc;
      else return &GPIO.out1_w1tc.val;
  }

  inline static port_t mask() __attribute__ ((always_inline)) { return MASK; }

  inline static bool isset() __attribute__ ((always_inline)) {
      if (PIN < 32) return GPIO.out & MASK;
      else return GPIO.out1.val & MASK;
  }
};

#define _FL_DEFPIN(PIN)  template<> class FastPin<PIN> : public _ESPPIN<PIN, ((PIN<32)?((uint32_t)1 << PIN):((uint32_t)1 << (PIN-32)))> {};

_FL_DEFPIN(0);
_FL_DEFPIN(1); // WARNING: Using TX causes flashiness when uploading
_FL_DEFPIN(2);
_FL_DEFPIN(3); // WARNING: Using RX causes flashiness when uploading
_FL_DEFPIN(4);
_FL_DEFPIN(5);

// -- These pins are not safe to use:
// _FL_DEFPIN(6,6); _FL_DEFPIN(7,7); _FL_DEFPIN(8,8);
// _FL_DEFPIN(9,9); _FL_DEFPIN(10,10); _FL_DEFPIN(11,11);

_FL_DEFPIN(12);
_FL_DEFPIN(13);
_FL_DEFPIN(14);
_FL_DEFPIN(15);
_FL_DEFPIN(16);
_FL_DEFPIN(17);
_FL_DEFPIN(18);
_FL_DEFPIN(19);

// No pin 20 : _FL_DEFPIN(20,20);

_FL_DEFPIN(21); // Works, but note that GPIO21 is I2C SDA
_FL_DEFPIN(22); // Works, but note that GPIO22 is I2C SCL
_FL_DEFPIN(23);

// No pin 24 : _FL_DEFPIN(24,24);

_FL_DEFPIN(25);
_FL_DEFPIN(26);
_FL_DEFPIN(27);

// No pin 28-31: _FL_DEFPIN(28,28); _FL_DEFPIN(29,29); _FL_DEFPIN(30,30); _FL_DEFPIN(31,31);

// Need special handling for pins > 31
_FL_DEFPIN(32);
_FL_DEFPIN(33);

#define HAS_HARDWARE_PIN_SUPPORT

FASTLED_NAMESPACE_END

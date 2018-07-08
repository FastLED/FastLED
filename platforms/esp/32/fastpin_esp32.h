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
      else if(PIN < 34) GPIO.out1_w1ts.val = MASK;
  }

  inline static void lo() __attribute__ ((always_inline)) {
      if (PIN < 32) GPIO.out_w1tc = MASK;
      else if(PIN < 34) GPIO.out1_w1tc.val = MASK;
  }

  inline static void set(register port_t val) __attribute__ ((always_inline)) {
      if (val) hi();
      else lo();
  }

  inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() __attribute__ ((always_inline)) {  }

  inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
  inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() __attribute__ ((always_inline)) { return MASK; }
  inline static port_t loval() __attribute__ ((always_inline)) { return MASK; }
  inline static port_ptr_t port() __attribute__ ((always_inline)) { return 0; }

  inline static port_ptr_t sport() __attribute__ ((always_inline)) { 
      if (PIN < 32) return &GPIO.out_w1ts;
      else if(PIN < 34) return &GPIO.out1_w1ts.val;
      else return 0;
  }

  inline static port_ptr_t cport() __attribute__ ((always_inline)) {
      if (PIN < 32) return &GPIO.out_w1tc;
      else if(PIN < 34) return &GPIO.out1_w1tc.val;
      else return 0;
  }

  inline static port_t mask() __attribute__ ((always_inline)) { return MASK; }

  inline static bool isset() __attribute__ ((always_inline)) { return (0x004 & MASK); }
};

#define _DEFPIN_ESP32(PIN)  template<> class FastPin<PIN> : public _ESPPIN<PIN, ((uint32_t)1 << PIN)> {};
#define _DEFPIN_32_33_ESP32(PIN) template<> class FastPin<PIN> : public _ESPPIN<PIN, ((uint32_t)1 << (PIN-32))> {};

_DEFPIN_ESP32(0);
// _DEFPIN_ESP32(1); Using TX causes flashiness when uploading
_DEFPIN_ESP32(2); 
// _DEFPIN_ESP32(3); Using RX causes flashiness when uploading
_DEFPIN_ESP32(4);
_DEFPIN_ESP32(5);

// -- These pins are not safe to use:
// _DEFPIN_ESP32(6,6); _DEFPIN_ESP32(7,7); _DEFPIN_ESP32(8,8); 
// _DEFPIN_ESP32(9,9); _DEFPIN_ESP32(10,10); _DEFPIN_ESP32(11,11); 

_DEFPIN_ESP32(12);
_DEFPIN_ESP32(13);
_DEFPIN_ESP32(14);
_DEFPIN_ESP32(15);
_DEFPIN_ESP32(16);
_DEFPIN_ESP32(17);
_DEFPIN_ESP32(18);
_DEFPIN_ESP32(19);

// No pin 20 : _DEFPIN_ESP32(20,20); 

_DEFPIN_ESP32(21); // Works, but note that GPIO21 is I2C SDA
_DEFPIN_ESP32(22); // Works, but note that GPIO22 is I2C SCL
_DEFPIN_ESP32(23); 

// No pin 24 : _DEFPIN_ESP32(24,24); 

_DEFPIN_ESP32(25);
_DEFPIN_ESP32(26);
_DEFPIN_ESP32(27); 

// No pin 28-31: _DEFPIN_ESP32(28,28); _DEFPIN_ESP32(29,29); _DEFPIN_ESP32(30,30); _DEFPIN_ESP32(31,31);

// Need special handling for pins > 31
_DEFPIN_32_33_ESP32(32); 
_DEFPIN_32_33_ESP32(33);

#define HAS_HARDWARE_PIN_SUPPORT

FASTLED_NAMESPACE_END

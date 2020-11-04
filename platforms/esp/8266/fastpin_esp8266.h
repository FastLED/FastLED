#pragma once

FASTLED_NAMESPACE_BEGIN

struct FASTLED_ESP_IO {
    volatile uint32_t _GPO;
    volatile uint32_t _GPOS;
    volatile uint32_t _GPOC;
};

#define _GPB (*(FASTLED_ESP_IO*)(0x60000000+(0x300)))


template<uint8_t PIN, uint32_t MASK> class _ESPPIN {
public:
    typedef volatile uint32_t * port_ptr_t;
    typedef uint32_t port_t;

    inline static void setOutput() { pinMode(PIN, OUTPUT); }
    inline static void setInput() { pinMode(PIN, INPUT); }

    inline static void hi() __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPOS = MASK; } else { GP16O = 1; } }
    inline static void lo() __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPOC = MASK; } else { GP16O = 0; } }
    inline static void set(register port_t val) __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPO = val; } else { GP16O = val; }}

    inline static void strobe() __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPO ^= MASK; } else { GP16O ^= MASK; } }

    inline static void hi(register port_ptr_t port) __attribute__ ((always_inline)) { hi(); }
    inline static void lo(register port_ptr_t port) __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(register port_ptr_t port, register port_t val) __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() __attribute__ ((always_inline)) { if (PIN<16) { return GPO | MASK;  } else { return GP16O | MASK; } }
    inline static port_t loval() __attribute__ ((always_inline)) { if (PIN<16) { return GPO & ~MASK; } else { return GP16O & ~MASK; } }
    inline static port_ptr_t port() __attribute__ ((always_inline)) { if(PIN<16) { return &_GPB._GPO; } else { return &GP16O; } }
    inline static port_ptr_t sport() __attribute__ ((always_inline)) { return &_GPB._GPOS; } // there is no GP160 support for this
    inline static port_ptr_t cport() __attribute__ ((always_inline)) { return &_GPB._GPOC; }
    inline static port_t mask() __attribute__ ((always_inline)) { return MASK; }

    inline static bool isset() __attribute__ ((always_inline)) { return (PIN < 16) ? (GPO & MASK) : (GP16O & MASK); }
};

#define _FL_DEFPIN(PIN, REAL_PIN) template<> class FastPin<PIN> : public _ESPPIN<REAL_PIN, (1<<(REAL_PIN & 0x0F))> {};


#ifdef FASTLED_ESP8266_RAW_PIN_ORDER
#define MAX_PIN 16
_FL_DEFPIN(0,0); _FL_DEFPIN(1,1); _FL_DEFPIN(2,2); _FL_DEFPIN(3,3);
_FL_DEFPIN(4,4); _FL_DEFPIN(5,5);

// These pins should be disabled, as they always cause WDT resets
// _FL_DEFPIN(6,6); _FL_DEFPIN(7,7);
// _FL_DEFPIN(8,8); _FL_DEFPIN(9,9); _FL_DEFPIN(10,10); _FL_DEFPIN(11,11);

_FL_DEFPIN(12,12); _FL_DEFPIN(13,13); _FL_DEFPIN(14,14); _FL_DEFPIN(15,15);
_FL_DEFPIN(16,16);

#define PORTA_FIRST_PIN 12
#elif defined(FASTLED_ESP8266_D1_PIN_ORDER)
#define MAX_PIN 15
_FL_DEFPIN(0,3);
_FL_DEFPIN(1,1);
_FL_DEFPIN(2,16);
_FL_DEFPIN(3,5);
_FL_DEFPIN(4,4);
_FL_DEFPIN(5,14);
_FL_DEFPIN(6,12);
_FL_DEFPIN(7,13);
_FL_DEFPIN(8,0);
_FL_DEFPIN(9,2);
_FL_DEFPIN(10,15);
_FL_DEFPIN(11,13);
_FL_DEFPIN(12,12);
_FL_DEFPIN(13,14);
_FL_DEFPIN(14,4);
_FL_DEFPIN(15,5);

#define PORTA_FIRST_PIN 12

#else // if defined(FASTLED_ESP8266_NODEMCU_PIN_ORDER)
#define MAX_PIN 10

// This seems to be the standard Dxx pin mapping on most of the esp boards that i've found
_FL_DEFPIN(0,16); _FL_DEFPIN(1,5); _FL_DEFPIN(2,4); _FL_DEFPIN(3,0);
_FL_DEFPIN(4,2); _FL_DEFPIN(5,14); _FL_DEFPIN(6,12); _FL_DEFPIN(7,13);
_FL_DEFPIN(8,15); _FL_DEFPIN(9,3); _FL_DEFPIN(10,1);

#define PORTA_FIRST_PIN 6

// The rest of the pins - these are generally not available
// _FL_DEFPIN(11,6);
// _FL_DEFPIN(12,7); _FL_DEFPIN(13,8); _FL_DEFPIN(14,9); _FL_DEFPIN(15,10);
// _FL_DEFPIN(16,11);

#endif

#define HAS_HARDWARE_PIN_SUPPORT

FASTLED_NAMESPACE_END

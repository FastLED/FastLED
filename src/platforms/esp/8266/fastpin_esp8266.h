#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
#include "fl/system/fastpin_base.h"
#include "fl/system/pin.h"  // For PinMode, PinValue enums
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"
#include "platforms/delaycycles.h"
#include "platforms/cpu_frequency.h"
#include <esp8266_peri.h> // IWYU pragma: keep

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
namespace fl {
struct FASTLED_ESP_IO {
    volatile u32 _GPO;
    volatile u32 _GPOS;
    volatile u32 _GPOC;
};

#define _GPB (*(FASTLED_ESP_IO*)(0x60000000+(0x300)))


template<u8 PIN, u32 MASK> class _ESPPIN {
public:
    typedef volatile u32 * port_ptr_t;
    typedef u32 port_t;

    /// GPIO number this logical pin resolves to. Pin-order modes remap the
    /// template pin numbers, so drivers that need the real pad (for example
    /// the HSPI peripheral, which is hard-wired to GPIO13/14) must consult
    /// this rather than the template argument.
    static constexpr u8 kRealPin = PIN;

    inline static void setOutput() FL_NO_EXCEPT { pinMode(PIN, PinMode::Output); }
    inline static void setInput() FL_NO_EXCEPT { pinMode(PIN, PinMode::Input); }

    inline static void hi() FL_NO_EXCEPT __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPOS = MASK; } else { GP16O = 1; } }
    inline static void lo() FL_NO_EXCEPT __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPOC = MASK; } else { GP16O = 0; } }
    inline static void set(FASTLED_REGISTER port_t val) FL_NO_EXCEPT __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPO = val; } else { GP16O = val; }}

    inline static void strobe() FL_NO_EXCEPT __attribute__ ((always_inline)) { toggle(); toggle(); }

    inline static void toggle() FL_NO_EXCEPT __attribute__ ((always_inline)) { if(PIN < 16) { _GPB._GPO ^= MASK; } else { GP16O ^= MASK; } }

    inline static void hi(FASTLED_REGISTER port_ptr_t port) FL_NO_EXCEPT __attribute__ ((always_inline)) { hi(); }
    inline static void lo(FASTLED_REGISTER port_ptr_t port) FL_NO_EXCEPT __attribute__ ((always_inline)) { lo(); }
    inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) FL_NO_EXCEPT __attribute__ ((always_inline)) { *port = val; }

    inline static port_t hival() FL_NO_EXCEPT __attribute__ ((always_inline)) { if (PIN<16) { return GPO | MASK;  } else { return GP16O | MASK; } }
    inline static port_t loval() FL_NO_EXCEPT __attribute__ ((always_inline)) { if (PIN<16) { return GPO & ~MASK; } else { return GP16O & ~MASK; } }
    inline static port_ptr_t port() FL_NO_EXCEPT __attribute__ ((always_inline)) { if(PIN<16) { return &_GPB._GPO; } else { return &GP16O; } }
    inline static port_ptr_t sport() FL_NO_EXCEPT __attribute__ ((always_inline)) { return &_GPB._GPOS; } // there is no GP160 support for this
    inline static port_ptr_t cport() FL_NO_EXCEPT __attribute__ ((always_inline)) { return &_GPB._GPOC; }
    inline static port_t mask() FL_NO_EXCEPT __attribute__ ((always_inline)) { return MASK; }

    inline static bool isset() FL_NO_EXCEPT __attribute__ ((always_inline)) { return (PIN < 16) ? (GPO & MASK) : (GP16O & MASK); }

    inline static constexpr bool validpin() FL_NO_EXCEPT { return true; }
};

// Forward declaration of FastPin template
template<u8 PIN> class FastPin;

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

// Keep example defaults off UART0, boot-strapping pins, and flash GPIOs.
#ifndef FL_PIN_CLOCKLESS_1
#define FL_PIN_CLOCKLESS_1 4
#endif
#ifndef FL_PIN_SPI_DATA_1
#define FL_PIN_SPI_DATA_1 13
#endif
#ifndef FL_PIN_SPI_CLOCK_1
#define FL_PIN_SPI_CLOCK_1 14
#endif

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

// D2 is GPIO4; D7/D5 are the established hardware SPI MOSI/SCK pins.
#ifndef FL_PIN_CLOCKLESS_1
#define FL_PIN_CLOCKLESS_1 4
#endif
#ifndef FL_PIN_SPI_DATA_1
#define FL_PIN_SPI_DATA_1 7
#endif
#ifndef FL_PIN_SPI_CLOCK_1
#define FL_PIN_SPI_CLOCK_1 5
#endif

#define PORTA_FIRST_PIN 12

#else // if defined(FASTLED_ESP8266_NODEMCU_PIN_ORDER)
#define MAX_PIN 10

// This seems to be the standard Dxx pin mapping on most of the esp boards that i've found
_FL_DEFPIN(0,16); _FL_DEFPIN(1,5); _FL_DEFPIN(2,4); _FL_DEFPIN(3,0);
_FL_DEFPIN(4,2); _FL_DEFPIN(5,14); _FL_DEFPIN(6,12); _FL_DEFPIN(7,13);
_FL_DEFPIN(8,15); _FL_DEFPIN(9,3); _FL_DEFPIN(10,1);

// D2 is GPIO4; D7/D5 are the established hardware SPI MOSI/SCK pins.
#ifndef FL_PIN_CLOCKLESS_1
#define FL_PIN_CLOCKLESS_1 2
#endif
#ifndef FL_PIN_SPI_DATA_1
#define FL_PIN_SPI_DATA_1 7
#endif
#ifndef FL_PIN_SPI_CLOCK_1
#define FL_PIN_SPI_CLOCK_1 5
#endif

#define PORTA_FIRST_PIN 6

// The rest of the pins - these are generally not available
// _FL_DEFPIN(11,6);
// _FL_DEFPIN(12,7); _FL_DEFPIN(13,8); _FL_DEFPIN(14,9); _FL_DEFPIN(15,10);
// _FL_DEFPIN(16,11);

#endif

#define HAS_HARDWARE_PIN_SUPPORT

// FL_ESP8266_ENABLE_FAST_SOFTWARE_SPI: optimized bit-bang software SPI pin policy (default 1).
// Bench-measured on ESP8266EX hardware (docs/ESP8266_GENERIC_SPI_4966.md).
// Define it to 0 to fall back to the legacy per-pin bit-bang writer.
// Set it as a build flag (-D / build_flags), not a #define in the sketch: the
// value must match in every translation unit, including FastLED's own sources.
#ifndef FL_ESP8266_ENABLE_FAST_SOFTWARE_SPI
#define FL_ESP8266_ENABLE_FAST_SOFTWARE_SPI 1
#endif
#if FL_ESP8266_ENABLE_FAST_SOFTWARE_SPI
template <u8 PIN> struct BitbangPin<FastPin<PIN>> {
    static constexpr bool kAtomicWrite = true;
    static constexpr bool kEsp32LegacyWriteBit = false;  // generic writer trait
    static constexpr bool kTwoWriteStream = FastPin<PIN>::kRealPin < 16;
    // Normal GPIO writes serialize through the 80 MHz peripheral bus. The
    // measured floor is six bus cycles (12 CPU cycles at 160 MHz); at 80 MHz
    // the CPU can add another cycle. Keep the conservative six-cycle bound.
    // GPIO16 uses a different RTC path and is excluded from this optimization.
    static constexpr cycle_t kMinWriteCycles = FastPin<PIN>::kRealPin < 16
        ? static_cast<cycle_t>(FL_CPU_FREQUENCY()) * 6 / ESP8266_CLOCK : 0;
    FASTLED_FORCE_INLINE static u32 cycles() FL_NO_EXCEPT {
        u32 value;
        // The core's esp_get_cycle_count() uses this LX106 CCOUNT read.
        __asm__ __volatile__("rsr %0,ccount" : "=a"(value) : : "memory");
        return value;
    }
    template <fl::cycle_t CYCLES>
    FASTLED_FORCE_INLINE static void delay() FL_NO_EXCEPT {
        // The dispatch header exposes the inline template at global scope.
        // fl::delaycycles only has an out-of-line definition at this callsite;
        // calling it would pay CALL/RET three times for every transmitted bit.
        if (CYCLES > 32) {
            // Large inline NOP sequences overflow small flash profiles and
            // thrash the instruction cache. A bounded counter wait also
            // accounts for time spent servicing interrupts during the hold.
            const u32 start = cycles();
            while (static_cast<u32>(cycles() - start) < static_cast<u32>(CYCLES)) {}
        } else {
            ::delaycycles<CYCLES>();
        }
    }
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void write(u8 value) FL_NO_EXCEPT {
        if (FastPin<PIN>::kRealPin < 16) {
            writeBitSetClear<BIT>(value, FastPin<PIN>::mask(), &GPOS, &GPOC);
        } else {
            // GPIO16 belongs to the RTC block, with no W1TS/W1TC registers.
            GP16O = (GP16O & ~1UL) | ((value >> BIT) & 1U);
        }
    }
};
#endif  // FL_ESP8266_ENABLE_FAST_SOFTWARE_SPI
}  // namespace fl

FL_DISABLE_WARNING_POP

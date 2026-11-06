#pragma once

// IWYU pragma: private

#include "platforms/esp/is_esp.h"
#include "platforms/esp/32/core/output_pin_policy.h"

#include "fl/stl/stdint.h"
#include "fl/stl/static_assert.h"
// IWYU pragma: begin_keep
#include "soc/gpio_reg.h"
// IWYU pragma: end_keep

#include "fl/stl/compiler_control.h"
#include "platforms/esp/esp_version.h"
#include "fl/system/fastpin.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "platforms/delaycycles.h"
#include "platforms/esp/32/core/clock_cycles.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

FL_DISABLE_WARNING_PUSH
FL_DISABLE_WARNING_DEPRECATED_REGISTER
namespace fl {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wvolatile"

template<u8 PIN, u32 MASK, bool VALIDPIN> class _ESPPIN {
public:
  typedef volatile u32 * port_ptr_t;
  typedef u32 port_t;

  static constexpr bool validpin() FL_NO_EXCEPT { return VALIDPIN; }

#ifndef GPIO_OUT1_REG
  static constexpr u32 GPIO_REG = GPIO_OUT_REG;
  static constexpr u32 GPIO_BIT_SET_REG = GPIO_OUT_W1TS_REG;
  static constexpr u32 GPIO_BIT_CLEAR_REG = GPIO_OUT_W1TC_REG;
  #else
  static constexpr u32 GPIO_REG = PIN < 32 ? GPIO_OUT_REG : GPIO_OUT1_REG;
  static constexpr u32 GPIO_BIT_SET_REG = PIN < 32 ? GPIO_OUT_W1TS_REG : GPIO_OUT1_W1TS_REG;
  static constexpr u32 GPIO_BIT_CLEAR_REG = PIN < 32 ? GPIO_OUT_W1TC_REG : GPIO_OUT1_W1TC_REG;
  #endif

  inline static void setOutput() FL_NO_EXCEPT {
      FL_STATIC_ASSERT(validpin(), "This pin has been marked as an invalid pin, common reasons includes it being a ground pin, read only, or too noisy (e.g. hooked up to the uart).");
      (fl::pinMode)(PIN, PinMode::Output);
  }
  inline static void setInput() FL_NO_EXCEPT { (fl::pinMode)(PIN, PinMode::Input); }

  inline static void hi() FL_NO_EXCEPT __attribute__ ((always_inline)) {
      *sport() = MASK;
  }

  inline static void lo() FL_NO_EXCEPT __attribute__ ((always_inline)) {
      *cport() = MASK;
  }

  inline static void set(FASTLED_REGISTER port_t val) FL_NO_EXCEPT __attribute__ ((always_inline)) {
      *port() = val;
  }

  inline static void strobe() FL_NO_EXCEPT __attribute__ ((always_inline)) { toggle(); toggle(); }

  inline static void toggle() FL_NO_EXCEPT __attribute__ ((always_inline)) {
      // Never read-modify-write GPIO_OUT: a concurrent GPIO update could be
      // overwritten. Use the per-bit atomic set/clear registers instead.
      if (isset()) {
          lo();
      } else {
          hi();
      }
  }

  inline static void hi(FASTLED_REGISTER port_ptr_t port) FL_NO_EXCEPT __attribute__ ((always_inline)) { hi(); }
  inline static void lo(FASTLED_REGISTER port_ptr_t port) FL_NO_EXCEPT __attribute__ ((always_inline)) { lo(); }
  inline static void fastset(FASTLED_REGISTER port_ptr_t port, FASTLED_REGISTER port_t val) FL_NO_EXCEPT __attribute__ ((always_inline)) { *port = val; }

  inline static port_t hival() FL_NO_EXCEPT __attribute__ ((always_inline)) {
      return (*port()) | MASK;
  }

  inline static port_t loval() FL_NO_EXCEPT __attribute__ ((always_inline)) {
      return (*port()) & ~MASK;
  }

  inline static port_ptr_t port() FL_NO_EXCEPT __attribute__ ((always_inline)) {
      return (port_ptr_t)GPIO_REG;
  }

  inline static port_ptr_t sport() FL_NO_EXCEPT __attribute__ ((always_inline)) {
      return (port_ptr_t)GPIO_BIT_SET_REG;
  }

  inline static port_ptr_t cport() FL_NO_EXCEPT __attribute__ ((always_inline)) {
      return (port_ptr_t)GPIO_BIT_CLEAR_REG;
  }

  inline static port_t mask() FL_NO_EXCEPT __attribute__ ((always_inline)) { return MASK; }

  inline static bool isset() FL_NO_EXCEPT __attribute__ ((always_inline)) {
      return (*port()) & MASK;
  }
};

// NOTE: outside the FASTLED_UNUSABLE_PIN_MASK guard on purpose. A build that
// defines its own mask would otherwise skip this default and fall back to pin
// 3 -- reintroducing exactly the UART collision described below, but only for
// builds that customize the mask, which is the worst place for it to hide.
// Default data pin for examples -- see platforms/default_pins.h.
//
// Deliberately not 3. On ESP32 classic GPIO 3 is U0RXD, the pin Serial
// receives on; the mask comments above already flag "GPIO 1 & 3 commonly used
// for UART". Blink calls Serial.begin(), so the generic fallback of 3 would
// have an example drive an output onto its own console RX line. GPIO 3 is also
// a strapping pin on S3 (JTAG source select) and on H2.
//
// GPIO 4 is outside FASTLED_UNUSABLE_PIN_MASK for every variant above, is not
// a UART pin on any of them, and is not reserved for flash or USB-JTAG. It is
// a boot-strap pin on C6, which is harmless here: strapping pins are sampled
// at reset and are ordinary outputs afterwards.
//
// This is a compile-time default for examples, not a claim about any
// particular board's wiring. FastLED#4018 tracks the wider per-platform audit.
#ifndef FL_PIN_CLOCKLESS_1
#define FL_PIN_CLOCKLESS_1 4
#endif

// Default SPI pins for examples -- see platforms/default_pins.h.
//
// Deliberately not the generic 1/2 fallback: GPIO 1 is U0TXD on ESP32 classic,
// the pin Serial transmits on, and the mask comments above flag "GPIO 1 & 3
// commonly used for UART". A clocked chipset defaulting there drives its data
// line onto the console. GPIO 2 is a strapping pin on classic, C3 and H2.
//
// Per-variant, because no single pair is safe across the family: below GPIO 6
// everything is UART or strapping, 6-17 is flash on one variant or another,
// and above 17 exceeds the pin count on C2/C3. Each pair below is outside that
// variant's FASTLED_UNUSABLE_PIN_MASK, is not its UART, and is not a strapping
// pin for it.
//
// Not taken from the Arduino core's MOSI/SCK, though that was the first
// instinct: those are `static const uint8_t`, not macros, so `#if defined(MOSI)`
// is always false and the guard silently selects the fallback instead. Seven
// variants also set `MOSI = -1`, which would fail FastPin<>::validpin().
//
// Compile-time defaults for examples, not a claim about any board's wiring.
// FastLED#4022 tracks confirming these on hardware.
#if defined(FL_IS_ESP_32S2)
#define FL_PIN_SPI_DATA_1_DEFAULT 35
#define FL_PIN_SPI_CLOCK_1_DEFAULT 36
#elif defined(FL_IS_ESP_32S3)
#define FL_PIN_SPI_DATA_1_DEFAULT 11
#define FL_PIN_SPI_CLOCK_1_DEFAULT 12
#elif defined(FL_IS_ESP_32C3) || defined(FL_IS_ESP_32C2) || defined(FL_IS_ESP_32C5)
#define FL_PIN_SPI_DATA_1_DEFAULT 7
#define FL_PIN_SPI_CLOCK_1_DEFAULT 6
#elif defined(FL_IS_ESP_32C6)
#define FL_PIN_SPI_DATA_1_DEFAULT 19
#define FL_PIN_SPI_CLOCK_1_DEFAULT 18
#elif defined(FL_IS_ESP_32H2)
#define FL_PIN_SPI_DATA_1_DEFAULT 11
#define FL_PIN_SPI_CLOCK_1_DEFAULT 10
#elif defined(FL_IS_ESP_32P4)
#define FL_PIN_SPI_DATA_1_DEFAULT 23
#define FL_PIN_SPI_CLOCK_1_DEFAULT 22
#else
// ESP32 classic, and any variant this file does not recognise. 23/18 are the
// conventional VSPI MOSI/SCK and are free of flash, UART and strapping duty.
#define FL_PIN_SPI_DATA_1_DEFAULT 23
#define FL_PIN_SPI_CLOCK_1_DEFAULT 18
#endif

#ifndef FL_PIN_SPI_DATA_1
#define FL_PIN_SPI_DATA_1 FL_PIN_SPI_DATA_1_DEFAULT
#endif
#ifndef FL_PIN_SPI_CLOCK_1
#define FL_PIN_SPI_CLOCK_1 FL_PIN_SPI_CLOCK_1_DEFAULT
#endif



#if !defined(FASTLED_FORCE_SOFTWARE_PINS)
#define _FL_DEFPIN(PIN) template <> class FastPin<PIN> : public _ESPPIN<PIN, ((u32)1 << (PIN % 32)), Esp32OutputPinValidity<PIN>::value> {};

// Define all possible pins.  If the pin is not valid for a particular ESP32 variant, the pin number
// will be shifted into the 192-255 range, in effect rendering it unusable.
_FL_DEFPIN( 0); _FL_DEFPIN( 1); _FL_DEFPIN( 2); _FL_DEFPIN( 3);
_FL_DEFPIN( 4); _FL_DEFPIN( 5); _FL_DEFPIN( 6); _FL_DEFPIN( 7);
_FL_DEFPIN( 8); _FL_DEFPIN( 9); _FL_DEFPIN(10); _FL_DEFPIN(11);
_FL_DEFPIN(12); _FL_DEFPIN(13); _FL_DEFPIN(14); _FL_DEFPIN(15);
_FL_DEFPIN(16); _FL_DEFPIN(17); _FL_DEFPIN(18); _FL_DEFPIN(19);
_FL_DEFPIN(20); _FL_DEFPIN(21); _FL_DEFPIN(22); _FL_DEFPIN(23);
_FL_DEFPIN(24); _FL_DEFPIN(25); _FL_DEFPIN(26); _FL_DEFPIN(27);
_FL_DEFPIN(28); _FL_DEFPIN(29); _FL_DEFPIN(30); _FL_DEFPIN(31);
_FL_DEFPIN(32); _FL_DEFPIN(33); _FL_DEFPIN(34); _FL_DEFPIN(35);
_FL_DEFPIN(36); _FL_DEFPIN(37); _FL_DEFPIN(38); _FL_DEFPIN(39);
_FL_DEFPIN(40); _FL_DEFPIN(41); _FL_DEFPIN(42); _FL_DEFPIN(43);
_FL_DEFPIN(44); _FL_DEFPIN(45); _FL_DEFPIN(46); _FL_DEFPIN(47);
_FL_DEFPIN(48); _FL_DEFPIN(49); _FL_DEFPIN(50); _FL_DEFPIN(51);
_FL_DEFPIN(52); _FL_DEFPIN(53); _FL_DEFPIN(54); _FL_DEFPIN(55);
_FL_DEFPIN(56); _FL_DEFPIN(57); _FL_DEFPIN(58); _FL_DEFPIN(59);
_FL_DEFPIN(60); _FL_DEFPIN(61); _FL_DEFPIN(62); _FL_DEFPIN(63);

#define HAS_HARDWARE_PIN_SUPPORT
#endif // !defined(FASTLED_FORCE_SOFTWARE_PINS)

/// Separates the data store from the clock edge, which can target the same
/// set/clear register. Espressif erratum CPU-3.3 (ESP32 v0.0, fixed in v1.0) can
/// lose consecutive writes to one address. The RISC-V fence includes device I/O,
/// which `fence rw, rw` in FL_MEMORY_BARRIER does not.
FASTLED_FORCE_INLINE void gpioStoreBarrier() FL_NO_EXCEPT {
#if defined(__XTENSA__)
    __asm__ __volatile__("memw" : : : "memory");
#else
    __asm__ __volatile__("fence" : : : "memory");
#endif
}

/// Software SPI data updates are atomic per pin. Whole-port streaming stays off
/// because a port snapshot could overwrite concurrent GPIO updates.
template <u8 PIN> struct BitbangPin<FastPin<PIN>> : BitbangPinDefaults<FastPin<PIN>> {
    static constexpr bool kAtomicWrite = true;
    // The inline platform template avoids a call per clock phase; the visible
    // fl::delaycycles is only a declaration with an out-of-line definition.
    // Long waits use the cycle counter so they cannot expand to a NOP run.
    template <fl::cycle_t CYCLES>
    FASTLED_FORCE_INLINE static void delay() FL_NO_EXCEPT {
        if (CYCLES > 32) {
            const u32 start = __clock_cycles();
            while (static_cast<u32>(__clock_cycles() - start) < static_cast<u32>(CYCLES)) {}
        } else {
            ::delaycycles<(CYCLES > 32 ? 0 : CYCLES)>();
        }
    }
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void write(u8 value) FL_NO_EXCEPT {
        gpioStoreBarrier();
        writeBitSetClearNoBranch<BIT>(value, FastPin<PIN>::mask(), FastPin<PIN>::sport(), FastPin<PIN>::cport());
        gpioStoreBarrier();
    }
};

#pragma GCC diagnostic pop
}  // namespace fl

FL_DISABLE_WARNING_POP

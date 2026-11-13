#pragma once

// IWYU pragma: private

/// @file runtime_software_spi.h
/// @brief Software SPI for pins chosen at run time, using atomic set/clear registers
/// on targets where a pin's registers can be resolved once per transfer.

#include "fl/stl/compiler_control.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/system/delay.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

#if defined(ESP8266)
#include <esp8266_peri.h>  // IWYU pragma: keep
#include "fl/system/fastpin.h"  // IWYU pragma: keep
#elif defined(ESP32)
#include "fl/system/fastpin.h"  // IWYU pragma: keep
#endif

// Atomic runtime writes are part of each platform's optimized bit-bang path and
// follow the same FL_<PLATFORM>_ENABLE_FAST_SOFTWARE_SPI switch as the
// compile-time writer (fastpin_esp8266.h / fastpin_esp32.h). This header is
// compiled into FastLED's own sources, so the switch must be a build flag (-D)
// to agree with the sketch. When it is off, callers keep their Pin fallback.
#if defined(ESP8266) && defined(FL_ESP8266_ENABLE_FAST_SOFTWARE_SPI) && \
    FL_ESP8266_ENABLE_FAST_SOFTWARE_SPI
#define FL_RUNTIME_SOFTWARE_SPI_ESP8266_ATOMIC
#endif
#if defined(ESP32) && defined(_FL_VALID_PIN_MASK) && \
    defined(FL_ESP32_ENABLE_FAST_SOFTWARE_SPI) && FL_ESP32_ENABLE_FAST_SOFTWARE_SPI
#define FL_RUNTIME_SOFTWARE_SPI_ESP32_ATOMIC
#endif

namespace fl {

/// One pin's write-one-to-set and write-one-to-clear registers.
struct RuntimeGpio {
    volatile u32* set;
    volatile u32* clear;
    u32 mask;

    FASTLED_FORCE_INLINE void hi() const FL_NO_EXCEPT { *set = mask; }
    FASTLED_FORCE_INLINE void lo() const FL_NO_EXCEPT { *clear = mask; }
};

/// Finds a pin's atomic registers. Returns false when the target has none known
/// here, or the pin needs other handling; the caller then keeps its Pin fallback.
inline bool resolveRuntimeGpio(u8 pin, RuntimeGpio* out) FL_NO_EXCEPT {
#if defined(FL_RUNTIME_SOFTWARE_SPI_ESP8266_ATOMIC)
    if (pin >= 16) { // GPIO16 has its own register with no set/clear pair.
        return false;
    }
    *out = {&GPOS, &GPOC, 1UL << pin};
    return true;
#elif defined(FL_RUNTIME_SOFTWARE_SPI_ESP32_ATOMIC)
    // FastLED's mask also excludes the pins it marks unusable, such as flash or USB.
    if (pin >= 64 || ((_FL_VALID_PIN_MASK >> pin) & 1ULL) == 0) {
        return false;
    }
    if (pin >= 32) {
#if defined(GPIO_OUT1_W1TS_REG)
        *out = {reinterpret_cast<volatile u32*>(GPIO_OUT1_W1TS_REG), // ok reinterpret cast
                reinterpret_cast<volatile u32*>(GPIO_OUT1_W1TC_REG), // ok reinterpret cast
                1UL << (pin - 32)};
        return true;
#else
        return false;
#endif
    }
    *out = {reinterpret_cast<volatile u32*>(GPIO_OUT_W1TS_REG), // ok reinterpret cast
            reinterpret_cast<volatile u32*>(GPIO_OUT_W1TC_REG), // ok reinterpret cast
            1UL << pin};
    return true;
#else
    (void)pin;
    (void)out;
    return false;
#endif
}

/// Separates the data store from the neighbouring clock stores on ESP32, where
/// they can target one register; see gpioStoreBarrier().
FASTLED_FORCE_INLINE void runtimeGpioBarrier() FL_NO_EXCEPT {
#ifdef FL_RUNTIME_SOFTWARE_SPI_ESP32_ATOMIC
    gpioStoreBarrier();
#endif
}

/// Half of one clock period in ns for a requested rate; 0 (no pacing) when the
/// rate is unknown. The loop's own cost adds to it, so the clock never runs
/// faster than requested but can run slower.
inline u32 softwareSpiHalfPeriodNs(u32 speed_hz) FL_NO_EXCEPT {
    return speed_hz ? 500000000u / speed_hz : 0;
}

/// Waits for a software SPI half period; free when unpaced.
FASTLED_FORCE_INLINE void softwareSpiHalfDelay(u32 half_period_ns) FL_NO_EXCEPT {
    if (half_period_ns) {
        fl::delayNanoseconds(half_period_ns);
    }
}

/// Writes bytes MSB first, with one data store per bit. Data is set half a
/// period before the rising edge, and the clock stays high for half a period.
template <class Clock>
inline void writeRuntimeSoftwareSpi(const Clock& clock, const RuntimeGpio& data, const u8* bytes,
                                    size_t length, u32 half_period_ns = 0) FL_NO_EXCEPT {
    clock.lo();
    for (size_t i = 0; i < length; ++i) {
        u8 byte = bytes[i];
        for (u8 bit = 0; bit < 8; ++bit) {
            runtimeGpioBarrier();
            writeBitSetClearNoBranch<7>(byte, data.mask, data.set, data.clear);
            runtimeGpioBarrier();
            softwareSpiHalfDelay(half_period_ns);
            clock.hi();
            softwareSpiHalfDelay(half_period_ns);
            clock.lo();
            byte = static_cast<u8>(byte << 1);
        }
    }
}

} // namespace fl

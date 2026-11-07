#pragma once

// IWYU pragma: private

/// @file runtime_software_spi.h
/// @brief Software SPI for pins chosen at run time, using atomic set/clear registers
/// on targets where a pin's registers can be resolved once per transfer.

#include "fl/stl/compiler_control.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "platforms/shared/spi_bitbang/bitbang_pin.h"

#if defined(ESP8266)
#include <esp8266_peri.h>  // IWYU pragma: keep
#elif defined(ESP32)
#include "fl/system/fastpin.h"  // IWYU pragma: keep
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
#if defined(ESP8266)
    if (pin >= 16) { // GPIO16 has its own register with no set/clear pair.
        return false;
    }
    *out = {&GPOS, &GPOC, 1UL << pin};
    return true;
#elif defined(ESP32) && defined(_FL_VALID_PIN_MASK)
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
#if defined(ESP32) && defined(_FL_VALID_PIN_MASK)
    gpioStoreBarrier();
#endif
}

/// Writes bytes MSB first, with one data store per bit. No delay is added: this
/// path has never had a programmable clock rate, and requested device speed is
/// not applied.
template <class Clock>
inline void writeRuntimeSoftwareSpi(const Clock& clock, const RuntimeGpio& data, const u8* bytes,
                                    size_t length) FL_NO_EXCEPT {
    clock.lo();
    for (size_t i = 0; i < length; ++i) {
        u8 byte = bytes[i];
        for (u8 bit = 0; bit < 8; ++bit) {
            runtimeGpioBarrier();
            writeBitSetClearNoBranch<7>(byte, data.mask, data.set, data.clear);
            runtimeGpioBarrier();
            clock.hi();
            clock.lo();
            byte = static_cast<u8>(byte << 1);
        }
    }
}

} // namespace fl

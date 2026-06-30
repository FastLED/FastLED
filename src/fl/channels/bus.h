#pragma once

/// @file bus.h
/// @brief Portable compile-time identifiers for LED channel buses.

#include "fl/channels/bus_features.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/stdint.h"
#include "platforms/is_platform.h"

// Platform CMSIS/Arduino headers may define peripheral pointer macros that
// collide with Bus enumerators and qualified names such as Bus::SPI.
#pragma push_macro("UART")
#pragma push_macro("SPI")
#pragma push_macro("I2S")
#undef UART
#undef SPI
#undef I2S

namespace fl {

struct ClocklessChipset;
struct SpiChipsetConfig;

/// @brief Portable public bus selector.
enum class Bus : fl::u8 {
    AUTO = 0,
    BIT_BANG,
    UART,
    SPI,
    DUAL_SPI,
    QUAD_SPI,
    OCTAL_SPI,
    RMT,
    FLEX_IO,
};

template<typename Chipset> struct DefaultBus;

#if defined(FL_IS_STUB) || defined(FL_IS_WASM)

template<> struct DefaultBus<ClocklessChipset> {
    static constexpr Bus value = Bus::BIT_BANG;
};

template<> struct DefaultBus<SpiChipsetConfig> {
    static constexpr Bus value = Bus::BIT_BANG;
};

#elif defined(FL_IS_ESP32)

#if defined(FL_IS_ESP_32P4) || defined(FL_IS_ESP_32C6) || \
    defined(FL_IS_ESP_32H2) || defined(FL_IS_ESP_32C5)
template<> struct DefaultBus<ClocklessChipset> {
    static constexpr Bus value = Bus::FLEX_IO;
};
#else
template<> struct DefaultBus<ClocklessChipset> {
    static constexpr Bus value = Bus::RMT;
};
#endif

#if defined(FL_IS_ESP_32S3) || defined(FL_IS_ESP_32DEV) || \
    defined(FL_IS_ESP_32P4) || defined(FL_IS_ESP_32C6) || \
    defined(FL_IS_ESP_32H2) || defined(FL_IS_ESP_32C5)
template<> struct DefaultBus<SpiChipsetConfig> {
    static constexpr Bus value = Bus::FLEX_IO;
};
#endif

#elif defined(FL_IS_TEENSY_4X)

template<> struct DefaultBus<ClocklessChipset> {
    static constexpr Bus value = Bus::FLEX_IO;
};

template<> struct DefaultBus<SpiChipsetConfig> {
    static constexpr Bus value = Bus::FLEX_IO;
};

#elif defined(FL_IS_ARM_LPC)

// LPC8xx / LPC11Uxx / LPC15xx have no parallel-IO clockless peripheral.
// Clockless output goes through the shared M0 cycle-counted C++ driver
// (FASTLED_M0_USE_C_IMPLEMENTATION in led_sysdefs_arm_lpc.h), which is
// what Bus::BIT_BANG names — the same engine that LPC's
// ClocklessController template uses today.
template<> struct DefaultBus<ClocklessChipset> {
    static constexpr Bus value = Bus::BIT_BANG;
};

// LPC845 / LPC804 have a hardware SPI driver (spi_arm_lpc.h, UM11029).
// LPC11xx / LPC15xx do not yet ship one (#2845 Stage 4) — they fall
// through to the unspecialized DefaultBus, which is intentional: a
// link error there is the right signal that no SPI driver is wired.
#if defined(FL_IS_ARM_LPC_845) || defined(FL_IS_ARM_LPC_804)
template<> struct DefaultBus<SpiChipsetConfig> {
    static constexpr Bus value = Bus::SPI;
};
#endif

#endif

template<Bus B> struct BusInstanceCount {
    static constexpr fl::u8 value = 1;
};

#if defined(FL_IS_TEENSY_4X)
template<> struct BusInstanceCount<Bus::FLEX_IO> { static constexpr fl::u8 value = 2; };
template<> struct BusInstanceCount<Bus::SPI> { static constexpr fl::u8 value = 4; };
template<> struct BusInstanceCount<Bus::UART> { static constexpr fl::u8 value = 1; };
#elif defined(FL_IS_ESP_32S3)
template<> struct BusInstanceCount<Bus::FLEX_IO> { static constexpr fl::u8 value = 2; };
template<> struct BusInstanceCount<Bus::SPI> { static constexpr fl::u8 value = 2; };
template<> struct BusInstanceCount<Bus::UART> { static constexpr fl::u8 value = 3; };
#elif defined(FL_IS_ESP_32C6)
template<> struct BusInstanceCount<Bus::FLEX_IO> { static constexpr fl::u8 value = 2; };
template<> struct BusInstanceCount<Bus::SPI> { static constexpr fl::u8 value = 1; };
template<> struct BusInstanceCount<Bus::UART> { static constexpr fl::u8 value = 2; };
#endif

inline const char* busName(Bus b, fl::u8 which = 0) FL_NO_EXCEPT {
    (void)which;
    switch (b) {
        case Bus::AUTO:      return "AUTO";
        case Bus::BIT_BANG:  return "BIT_BANG";
        case Bus::UART:      return "UART";
        case Bus::SPI:       return "SPI";
        case Bus::DUAL_SPI:  return "DUAL_SPI";
        case Bus::QUAD_SPI:  return "QUAD_SPI";
        case Bus::OCTAL_SPI: return "OCTAL_SPI";
        case Bus::RMT:       return "RMT";
        case Bus::FLEX_IO:   return "FLEX_IO";
    }
    return "";
}

/// @brief Concrete ChannelManager driver name for a portable bus.
inline const char* busDriverName(Bus b, fl::u8 which = 0, bool spi = false) FL_NO_EXCEPT {
    switch (b) {
        case Bus::FLEX_IO:
#if defined(FL_IS_TEENSY_4X)
            (void)spi;
            return which == 0 ? "OBJECT_FLED" : "FLEX_IO";
#elif defined(FL_IS_ESP32)
#if FASTLED_HAS_PARLIO
            (void)which;
            (void)spi;
            return "PARLIO";
#elif FASTLED_HAS_LCD_SPI || FASTLED_HAS_LCD_CAM_CLOCKLESS
            (void)which;
            return spi ? "LCD_SPI" : "LCD_CLOCKLESS";
#elif FASTLED_HAS_I2S_SPI || FASTLED_HAS_I2S_LED
            (void)which;
            return spi ? "I2S_SPI" : "I2S";
#else
            (void)which;
            (void)spi;
            return "FLEX_IO";
#endif
#else
            (void)which;
            (void)spi;
            return "FLEX_IO";
#endif
        case Bus::AUTO:
        case Bus::BIT_BANG:
        case Bus::SPI:
        case Bus::DUAL_SPI:
        case Bus::QUAD_SPI:
        case Bus::OCTAL_SPI:
        case Bus::RMT:
            (void)spi;
            return busName(b, which);
        case Bus::UART:
            (void)which;
            (void)spi;
#if defined(FL_IS_TEENSY_4X)
            return "LPUART";
#else
            return "UART";
#endif
    }
    return "";
}

FL_STATIC_ASSERT(static_cast<fl::u8>(Bus::FLEX_IO) == 8,
                 "Bus changed: update busName() and busDriverName()");

}  // namespace fl

#pragma pop_macro("I2S")
#pragma pop_macro("SPI")
#pragma pop_macro("UART")

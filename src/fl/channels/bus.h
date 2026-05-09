#pragma once

/// @file bus.h
/// @brief Compile-time identifier for an LED channel transmission bus.
///
/// `fl::Bus` is the single source of truth for which low-level driver
/// implementation handles a channel. The same value drives:
///   - Compile-time template binding (`Channel<Bus::X, Chipset>`).
///   - Per-platform default selection (`DefaultBus<Chipset>::value`).
///   - Opt-in runtime registry enrollment (`enableDrivers<Bus::X...>()`).
///
/// Drivers link only when their `Bus` value is named somewhere in the
/// translation unit graph. See issue #2428.

#include "fl/stl/stdint.h"
#include "platforms/is_platform.h"

namespace fl {

// Forward declarations. The chipset family types live in fl/channels/config.h,
// but DefaultBus<Chipset> only needs the type identity, not the layout.
struct ClocklessChipset;
struct SpiChipsetConfig;

/// @brief Driver identifier for compile-time bus selection.
///
/// The values are deliberately named after the underlying peripheral so a single
/// identifier (`Bus::RMT`, `Bus::PARLIO`, etc.) flows through both the
/// template-binding API surface and the runtime registry overrides.
enum class Bus : fl::u8 {
    AUTO = 0,        ///< Sentinel: defer to `DefaultBus<Chipset>::value`.
    RMT,             ///< ESP32 RMT peripheral (all ESP32 variants).
    PARLIO,          ///< ESP32-P4/C6/H2/C5 parallel I/O peripheral.
    SPI,             ///< Generic SPI clockless driver.
    I2S,             ///< ESP32-S3 LCD_CAM via legacy I80 bus (clockless).
    I2S_SPI,         ///< Original ESP32 native I2S parallel SPI (true SPI chipsets).
    LCD_RGB,         ///< ESP32-P4 LCD RGB peripheral (parallel clockless).
    LCD_SPI,         ///< ESP32-S3 LCD_CAM SPI driver (true SPI chipsets).
    LCD_CLOCKLESS,   ///< ESP32-S3 LCD_CAM clockless driver (replaces misnamed I2S).
    UART,            ///< ESP32 UART driver via wave8 framing.
    FLEX_IO,         ///< Teensy 4.x FlexIO2 driver.
    OBJECT_FLED,     ///< Teensy 4.x ObjectFLED driver.
    BIT_BANG,        ///< Portable bit-bang fallback driver.
    STUB,            ///< Native/host/test stub driver.
};

/// @brief Per-platform default bus for a given chipset family.
///
/// Specialize this for each `(platform, Chipset)` pair so that a template
/// instantiation like `Channel<DefaultBus<ClocklessChipset>::value, ClocklessChipset>`
/// resolves to the platform's preferred driver without an explicit `Bus` parameter.
///
/// Specializations are scoped by platform `FL_IS_*` macros (see
/// `platforms/is_platform.h`). Platforms without a channel-driver implementation
/// for a given chipset family intentionally have no specialization, which causes
/// a clear "incomplete type" compile error if the templated API is called there.
template<typename Chipset> struct DefaultBus;

// ---------------------------------------------------------------------------
// Per-platform DefaultBus<Chipset> specializations
// ---------------------------------------------------------------------------
//
// Each specialization sets `value` to the Bus identifier whose driver TU should
// be linked when the user does not name a bus explicitly.

#if defined(FL_IS_STUB) || defined(FL_IS_WASM)

template<> struct DefaultBus<ClocklessChipset> {
    static constexpr Bus value = Bus::STUB;
};

template<> struct DefaultBus<SpiChipsetConfig> {
    static constexpr Bus value = Bus::STUB;
};

#elif defined(FL_IS_ESP32)

// ESP32-P4 / C6 / H2 / C5 -- PARLIO is the highest-priority clockless driver.
// ESP32-dev / S2 / S3 -- RMT is the recommended clockless default.
// Mirrors the priority order used by channel_manager_esp32.cpp.hpp.
#if defined(FL_IS_ESP_32P4) || defined(FL_IS_ESP_32C6) || \
    defined(FL_IS_ESP_32H2) || defined(FL_IS_ESP_32C5)
template<> struct DefaultBus<ClocklessChipset> {
    static constexpr Bus value = Bus::PARLIO;
};
#else
template<> struct DefaultBus<ClocklessChipset> {
    static constexpr Bus value = Bus::RMT;
};
#endif

// True SPI chipsets (APA102, SK9822, HD108): pick the platform-native parallel
// SPI driver. ESP32-S3 uses LCD_SPI; original ESP32 uses I2S_SPI; other variants
// are intentionally undefined so users get a clear "specify Bus::X explicitly"
// error rather than a silent wrong default.
#if defined(FL_IS_ESP_32S3)
template<> struct DefaultBus<SpiChipsetConfig> {
    static constexpr Bus value = Bus::LCD_SPI;
};
#elif defined(FL_IS_ESP_32DEV)
template<> struct DefaultBus<SpiChipsetConfig> {
    static constexpr Bus value = Bus::I2S_SPI;
};
#endif

#elif defined(FL_IS_TEENSY_4X)

template<> struct DefaultBus<ClocklessChipset> {
    static constexpr Bus value = Bus::OBJECT_FLED;
};
// SpiChipsetConfig default for Teensy 4.x is intentionally unspecified -- users
// pick explicitly between Bus::BIT_BANG and any future hardware-SPI bus.

#endif

}  // namespace fl

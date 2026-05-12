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

#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/stdint.h"
#include "platforms/is_platform.h"

// Post-#2428 architecture: drivers do NOT auto-register with `ChannelManager`.
// Only the platform-default driver TU (named by the legacy clockless controller's
// Phase 5b pre-bind via `BusTraits<DefaultBus<Chipset>>::instancePtr()`) links
// into the binary. Users who need additional drivers at runtime call:
//   - `fl::enableDrivers<fl::Bus::X, ...>()` for selective opt-in,
//   - `FastLED.enableAllDrivers()` for 3.10.3-style universal enrollment,
//   - `FastLED.setExclusiveDriver<fl::Bus::X>()` to register a non-default driver
//     at high priority (must be called before `addLeds<>` to override the pre-bind).
//
// The historical `FASTLED_DISABLE_LEGACY_DRIVER_REGISTRY` opt-in macro has been
// removed; the previously opt-in behaviour is now the only behaviour.

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

/// @brief Canonical driver-name string for a `Bus` value.
///
/// Each `Bus::X` corresponds to exactly one concrete `IChannelDriver`. That
/// driver's `getName()` returns a stable string that `ChannelManager` uses as
/// the lookup key (`findDriverByName` / `getDriverByName`). Channels read
/// `ChannelOptions::mBus` (#2459) and the dispatch layer derives the lookup
/// string via `busName(mBus)` — typo-prone string literals never appear at the
/// call site.
///
/// Returns `"AUTO"` for `Bus::AUTO` — diagnostic/log paths get a stable
/// human-readable label. `mBus == Bus::AUTO` means "no Bus pin; let the
/// manager pick by priority", so this name never reaches the manager's lookup.
///
/// Spelling is normalized to match the C++ enumerator name exactly (including
/// underscores in `FLEX_IO`, `OBJECT_FLED`, `BIT_BANG`). Driver `getName()`
/// implementations return the same string, so callers never need a reverse
/// mapping or special-case handling.
///
/// **Lifetime.** Every returned pointer is to a string literal, so it has
/// static storage duration — safe to feed to `fl::string::from_literal()`.
inline const char* busName(Bus b) FL_NOEXCEPT {
    switch (b) {
        case Bus::AUTO:          return "AUTO";
        case Bus::RMT:           return "RMT";
        case Bus::PARLIO:        return "PARLIO";
        case Bus::SPI:           return "SPI";
        case Bus::I2S:           return "I2S";
        case Bus::I2S_SPI:       return "I2S_SPI";
        case Bus::LCD_RGB:       return "LCD_RGB";
        case Bus::LCD_SPI:       return "LCD_SPI";
        case Bus::LCD_CLOCKLESS: return "LCD_CLOCKLESS";
        case Bus::UART:          return "UART";
        case Bus::FLEX_IO:       return "FLEX_IO";
        case Bus::OBJECT_FLED:   return "OBJECT_FLED";
        case Bus::BIT_BANG:      return "BIT_BANG";
        case Bus::STUB:          return "STUB";
    }
    return "";  // unreachable — silences -Wreturn-type on toolchains that miss the exhaustive switch
}

// Drift-prevention: if a new `Bus` enumerator is added, the switch above must
// gain a case. Update this assertion to the new last enumerator after you
// wire the case in — its failure is the prompt to revisit `busName()`.
FL_STATIC_ASSERT(static_cast<fl::u8>(Bus::STUB) == 13,
                 "Bus changed: add the new value to busName() in this file");  // ok plain enum

}  // namespace fl

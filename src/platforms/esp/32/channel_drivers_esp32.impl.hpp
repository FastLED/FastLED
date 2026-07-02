#pragma once

// IWYU pragma: private

/// @file channel_drivers_esp32.impl.hpp
/// @brief ESP32 channel-driver fragment for `fl::enableAllDrivers()`.

#include "fl/channels/bus.h"
#include "fl/channels/bus_traits.h"
#include "fl/stl/compiler_control.h"
#include "platforms/esp/32/feature_flags/enabled.h"
#include "platforms/shared/bitbang/bus_traits.h"  // ok platform headers // IWYU pragma: keep

// Each per-driver bus_traits.h applies its own `FASTLED_ESP32_HAS_*` /
// `FASTLED_RMT5` guard internally, so the include is unconditional here.
#include "platforms/esp/32/drivers/i2s/bus_traits.h"        // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/i2s_spi/bus_traits.h"    // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/lcd_cam/bus_traits.h"    // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/lcd_spi/bus_traits.h"    // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/parlio/bus_traits.h"     // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/rmt/rmt_4/bus_traits.h"  // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/rmt/rmt_5/bus_traits.h"  // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/spi/bus_traits.h"        // ok platform headers // IWYU pragma: keep
#include "platforms/esp/32/drivers/uart/bus_traits.h"       // ok platform headers // IWYU pragma: keep

namespace fl {
namespace platforms {

inline void enableAllChannelDrivers() FL_NO_EXCEPT {
    fl::enableDrivers<
        fl::Bus::BIT_BANG
#if FASTLED_ESP32_HAS_RMT
        , fl::Bus::RMT
#endif
#if FASTLED_ESP32_HAS_CLOCKLESS_SPI
        , fl::Bus::SPI
#endif
#if FASTLED_ESP32_HAS_PARLIO || FASTLED_ESP32_HAS_I2S || FASTLED_ESP32_HAS_LCD_SPI
        , fl::Bus::FLEX_IO
#endif
#if FASTLED_ESP32_HAS_UART
        , fl::Bus::UART
#endif
    >();
#if FASTLED_ESP32_HAS_LCD_RGB
    fl::enableDriver<fl::Bus::FLEX_IO, 1>();
#endif
#if FASTLED_ESP32_HAS_I2S
    // FastLED#3576 Phase 1 — second clockless bank on I2S0 ("I2S0",
    // FLEX_IO instance 1). Priority one below the primary I2S1 bank so
    // clockless channels fill 16 lanes there first, then overflow here
    // (32 lanes total), then RMT. I2S0 hardware is contended with the
    // clocked-SPI driver — the port-claim registry arbitrates at
    // initialize() time.
    fl::enableDriver<fl::Bus::FLEX_IO, 1>();
#endif
    // FastLED#3526 — the modern classic-ESP32 I2S parallel-out engine
    // (`ChannelEngineI2sEsp32Dev`) wraps the SAME I2S1 peripheral as
    // the current `I2S_SPI` driver at `Bus::FLEX_IO, 0`. Per the
    // parallel-IO unified-engine rule (agents/docs/cpp-standards.md
    // -> "the peripheral is the dispatch boundary, not the mode"),
    // both cannot bind at the same slot. Phase 2d proper replaces the
    // I2S_SPI binding at slot 0 with the unified clockless+SPI modern
    // engine — but that requires Phase 2c-SPI's real transmit path to
    // land first so SPI users aren't broken. Until then, slot 0 stays
    // with I2S_SPI and the modern engine is a library-linked
    // implementation ready for the swap.
}

}  // namespace platforms
}  // namespace fl

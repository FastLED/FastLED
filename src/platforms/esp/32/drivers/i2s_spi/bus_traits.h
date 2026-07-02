#pragma once

// IWYU pragma: private

/// @file bus_traits.h
/// @brief BusTraits<Bus::FLEX_IO, 0> for classic-ESP32 I2S1 unified engine.
///
/// **FastLED#3526 Phase 2d — slot swap.** Previously this specialization
/// returned `createI2sSpiEngine()` (the standalone I2S-SPI driver).
/// Now returns `createI2sEsp32DevEngine()` — the modern unified engine
/// that handles BOTH clockless and SPI batches on the same I2S1
/// peripheral, per the parallel-IO unified-engine rule in
/// `agents/docs/cpp-standards.md`.
///
/// SPI users are unaffected in behavior — the modern engine delegates
/// SPI batches to the tested `ChannelDriverI2sSpi` internally
/// (`channel_engine_i2s_esp32dev.cpp.hpp`, `has_spi` branch). Only the
/// dispatch entry point changes.
///
/// This file is scheduled for deletion at Phase 2e along with the rest
/// of `drivers/i2s_spi/` — the modern-engine specialization moves to
/// `drivers/i2s/bus_traits.h` at that point. Keeping it here in the
/// meantime avoids churning the `_build.cpp.hpp` and channel-manager
/// wiring on the same PR that flips the binding.

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#endif

#if defined(FL_IS_ESP32) && FASTLED_ESP32_HAS_I2S

#include "fl/channels/bus.h"
#include "fl/channels/bus_priorities.h"
#include "fl/channels/bus_traits.h"
#include "fl/channels/config.h"
#include "fl/channels/driver.h"
#include "fl/channels/manager.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/type_traits.h"
#include "platforms/esp/32/drivers/i2s/channel_engine_i2s_esp32dev.h"

namespace fl {

// createI2sEsp32DevEngine() is declared in channel_engine_i2s_esp32dev.h.

template<> struct BusTraits<Bus::FLEX_IO, 0> {
    using Driver = IChannelDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static fl::shared_ptr<Driver> gHolder = createI2sEsp32DevEngine();
        return gHolder;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::FLEX_IO, 0), instancePtr());
    }
};

// The unified engine handles both clockless AND SPI on the same slot —
// per the parallel-IO code-review rule (agents/docs/cpp-standards.md
// -> "Parallel-IO Driver: Unified Clockless + SPI Engine").
template<> struct BusSupports<Bus::FLEX_IO, SpiChipsetConfig, 0> : fl::true_type {};

// FastLED#3576 Phase 1 — second clockless bank on I2S0 ("I2S0",
// `Bus::FLEX_IO` instance 1). Classic ESP32 has two identical I2S
// blocks; instance 0 wraps I2S1 (primary), instance 1 wraps I2S0.
// I2S0 is contended with the clocked-SPI driver — the port-claim
// registry (`i2s_port_claim.h`) arbitrates at initialize() time, so
// whichever mode touches I2S0 first per session wins and the other
// fails cleanly.
template<> struct BusTraits<Bus::FLEX_IO, 1> {
    using Driver = IChannelDriver;

    static fl::shared_ptr<Driver> instancePtr() FL_NO_EXCEPT {
        static fl::shared_ptr<Driver> gHolder = createI2sEsp32DevEngine(/*port=*/0);
        return gHolder;
    }

    static Driver& instance() FL_NO_EXCEPT { return *instancePtr(); }

    static void registerWithManager() FL_NO_EXCEPT {
        ChannelManager::instance().addDriver(default_bus_priority(Bus::FLEX_IO, 1), instancePtr());
    }
};

template<> struct BusSupports<Bus::FLEX_IO, SpiChipsetConfig, 1> : fl::true_type {};

}  // namespace fl

#endif  // FL_IS_ESP32 && FASTLED_ESP32_HAS_I2S

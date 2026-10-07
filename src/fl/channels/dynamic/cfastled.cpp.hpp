// ok no header - CFastLED is declared in FastLED.h
#define FASTLED_INTERNAL
// IWYU pragma: private

/// @brief CFastLED runtime TX channel entry points, linked independently.

#include "FastLED.h" // ok include: implements public CFastLED methods
#include "fl/channels/all_drivers.h"
#include "fl/channels/channel.h"
#include "fl/channels/config.h"
#include "fl/channels/manager.h"
#include "fl/log/log.h"
void CFastLED::enableAllDrivers() {
	fl::enableAllDrivers();
}

fl::ChannelPtr CFastLED::add(const fl::ChannelConfig& config) {
    // Issue #2459: the non-template `FastLED.add(cfg)` path is the runtime-
    // selection mode. To make sure `cfg.options.mBus` (or priority dispatch
    // when `mBus == Bus::AUTO`) can actually find the requested driver at
    // runtime, we eagerly enroll every driver available on this platform.
    // That trades binary size for ergonomics â€” the user wanted runtime
    // selection, so they get runtime selection.
    //
    // For minimum binary size, callers should use the compile-time path:
    //   FastLED.addLeds<CHIPSET, PIN, ORDER, fl::Bus::X>(leds, n);
    // which ODR-uses only `BusTraits<X>::instancePtr()` and lets
    // `--gc-sections` drop every other driver TU.
    //
    // The warning fires at most once per process (FL_WARN_ONCE) and can be
    // disabled with `-DFASTLED_SUPPRESS_RUNTIME_DRIVER_WARNING`.
    #ifndef FASTLED_SUPPRESS_RUNTIME_DRIVER_WARNING
    FL_WARN_ONCE("FastLED.add(cfg): runtime-selection mode â€” enrolling every "
                 "available driver via fl::enableAllDrivers(). For minimum "
                 "binary size, prefer FastLED.addLeds<CHIPSET, PIN, ORDER, "
                 "fl::Bus::X>(leds, n) which links only the named driver. "
                 "Suppress this warning with -DFASTLED_SUPPRESS_RUNTIME_DRIVER_WARNING.");
    #endif
    fl::enableAllDrivers();

    fl::ChannelManager& manager = fl::channelManager();
    FL_ASSERT(manager.getDriverCount() > 0,
              "No channel drivers available - channel API requires at least one registered driver");
    auto channel = fl::Channel::create(config);
    add(channel);
    return channel;
}

fl::vector<fl::ChannelPtr> CFastLED::add(fl::span<const fl::ChannelConfig> configs) {
    fl::vector<fl::ChannelPtr> channels;
    channels.reserve(configs.size());

    for (const auto& config : configs) {
        channels.push_back(add(config));
    }

    return channels;
}

fl::vector<fl::ChannelPtr> CFastLED::add(fl::initializer_list<fl::ChannelConfig> configs) {
    fl::vector<fl::ChannelPtr> channels;
    channels.reserve(configs.size());

    for (const auto& config : configs) {
        channels.push_back(add(config));
    }

    return channels;
}

fl::vector<fl::ChannelPtr> CFastLED::add(const fl::MultiChannelConfig& multiConfig) {
    fl::vector<fl::ChannelPtr> channels;
    channels.reserve(multiConfig.mChannels.size());

    for (const auto& configPtr : multiConfig.mChannels) {
        if (configPtr) {
            channels.push_back(add(*configPtr));
        }
    }

    return channels;
}

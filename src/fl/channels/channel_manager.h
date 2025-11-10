/// @file channel_manager.h
/// @brief Central manager for coordinating multiple PARLIO groups
///
/// The ChannelManager acts as the central nervous system for the parallel I/O architecture,
/// coordinating all groups to prevent conflicts and ensure proper sequencing.
/// It also serves as the SOLE gatekeeper for engine access.

#pragma once

#include "channel.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"
#include "fl/ptr.h"
#include "fl/span.h"
#include "fl/stdint.h"
#include "pixeltypes.h"
#include "rgbw.h"
#include "channel_config.h"
#include "channel_engine.h"
#include "fl/singleton.h"

namespace fl {

// Forward declaration
class IChannelGroup;

/// @brief Abstract interface for PARLIO manager coordinating all active groups
///
/// The ChannelManager acts as the central nervous system for the parallel I/O architecture,
/// coordinating all groups to prevent conflicts and ensure proper sequencing.
/// It also serves as the SOLE gatekeeper for engine access.
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class IChannelManager {
public:

    /// @brief Create a channel with compile-time chipset timing
    /// @tparam TIMING Chipset timing type (e.g., TIMING_WS2812_800KHZ)
    /// @param leds LED data buffer
    /// @param rgbw RGBW configuration (default: disabled)
    /// @return Shared pointer to channel (auto-cleanup when out of scope)
    template<typename CHANNEL_ENGINE>
    static IChannelPtr create(const ChannelConfig& config) {
        IChannelEngine* engine = fl::Singleton<CHANNEL_ENGINE>::get();
        return createInternal(config, engine);
    }

private:
    /// @brief Virtual destructor
    virtual ~IChannelManager() = default;

    /// @brief Create a channel with runtime chipset timing
    /// @param timing Chipset timing configuration
    /// @param leds LED data buffer
    /// @param rgbw RGBW configuration (default: disabled)
    /// @return Shared pointer to channel (auto-cleanup when out of scope)
    static IChannelPtr createInternal(const ChannelConfig& config, IChannelEngine* engine);

    /// @brief Protected constructor (interface pattern)
    IChannelManager() = default;

    // Non-copyable, non-movable
    IChannelManager(const IChannelManager&) = delete;
    IChannelManager& operator=(const IChannelManager&) = delete;
    IChannelManager(IChannelManager&&) = delete;
    IChannelManager& operator=(IChannelManager&&) = delete;
};

} // namespace fl

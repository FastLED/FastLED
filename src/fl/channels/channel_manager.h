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
#include "fl/map.h"
#include "fl/vector.h"

namespace fl {

/// @brief Central manager for coordinating multiple PARLIO groups
///
/// The ChannelManager acts as the central nervous system for the parallel I/O architecture,
/// coordinating all groups to prevent conflicts and ensure proper sequencing.
/// It also serves as the SOLE gatekeeper for engine access.
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class ChannelManager {
public:
    /// @brief Get the singleton instance
    /// @return Pointer to the ChannelManager instance
    static ChannelManager* getInstance();

    /// @brief Destructor
    ~ChannelManager();

    /// @brief Private constructor (singleton pattern)
    ChannelManager();

    /// @brief Create a channel with compile-time chipset timing
    /// @tparam CHANNEL_ENGINE Engine type (e.g., ParlioEngine)
    /// @param config Channel configuration
    /// @return Shared pointer to channel (auto-cleanup when out of scope)
    template<typename CHANNEL_ENGINE>
    ChannelPtr create(const ChannelConfig& config) {
        IChannelEngine* engine = fl::Singleton<CHANNEL_ENGINE>::instance();
        return createInternal(config, engine);
    }

    /// @brief Enqueues a channel for transmission
    void onBeginShow(ChannelPtr channel);

    /// @brief Organizes all the channels by engine and calls
    /// beginTransmission on each engine
    void show();

    /// @brief Create a channel with runtime chipset timing
    /// @param config Channel configuration
    /// @param engine Channel engine to use
    /// @return Shared pointer to channel (auto-cleanup when out of scope)
    ChannelPtr createInternal(const ChannelConfig& config, IChannelEngine* engine);

private:
    // Non-copyable, non-movable
    ChannelManager(const ChannelManager&) = delete;
    ChannelManager& operator=(const ChannelManager&) = delete;
    ChannelManager(ChannelManager&&) = delete;
    ChannelManager& operator=(ChannelManager&&) = delete;

    /// @brief Mapping from channel engines to their associated channels
    /// @note Uses inlined vector with capacity 16 to avoid heap allocation for typical use cases
    fl::fl_map<IChannelEngine*, fl::vector_inlined<ChannelPtr, 16>> mPendingChannels;
};

} // namespace fl

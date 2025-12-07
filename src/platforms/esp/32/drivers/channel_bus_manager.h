/// @file channel_bus_manager.h
/// @brief ESP32-specific ChannelBusManager singleton accessor
///
/// This header provides the ESP32 platform's singleton accessor for the
/// ChannelBusManager using fl::Singleton<T>. The singleton pattern is an
/// implementation detail of the ESP32 platform, not part of the generic
/// fl/channels API.

#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "fl/channels/channel_bus_manager.h"
#include "fl/singleton.h"

namespace fl {

/// @brief ESP32-specific singleton type for ChannelBusManager
/// @note The singleton is automatically initialized before main() via FL_INIT
/// @note Access via ChannelBusManagerSingleton::instance() or channelBusManager()
using ChannelBusManagerSingleton = Singleton<ChannelBusManager>;

/// @brief Convenience function to get the ChannelBusManager singleton
/// @return Reference to the global ChannelBusManager instance
/// @note Shorthand for ChannelBusManagerSingleton::instance()
inline ChannelBusManager& channelBusManager() {
    return ChannelBusManagerSingleton::instance();
}

} // namespace fl

#endif // ESP32

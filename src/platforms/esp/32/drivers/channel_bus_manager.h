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
using ChannelBusManagerSingleton = Singleton<ChannelBusManager>;

/// @brief Get the initialized ChannelBusManager singleton for ESP32
/// @return Reference to the ChannelBusManager singleton
/// @note First call initializes the manager with ESP32 engines
ChannelBusManager& getChannelBusManager();

} // namespace fl

#endif // ESP32

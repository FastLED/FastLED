#pragma once

#include "fl/stl/function.h"
#include "fl/string.h"
#include "fl/singleton.h"

namespace fl {

class Channel;
class IChannelEngine;
struct ChannelConfig;

/// @brief Singleton event router for Channel lifecycle events
///
/// Centralized event system - supports multiple listeners per event type.
/// All listeners are shared across all channels. Zero per-channel overhead.
///
/// Usage:
/// @code
/// // Add a listener
/// int id = FastLED.channelEvents().onChannelCreated.add([](const Channel& ch) {
///     FL_DBG("Channel created: " << ch.name());
/// });
///
/// // Add with priority (higher priority executes first)
/// int id2 = FastLED.channelEvents().onChannelCreated.add([](const Channel& ch) {
///     FL_DBG("High priority listener");
/// }, 100);
///
/// // Remove a listener
/// FastLED.channelEvents().onChannelCreated.remove(id);
/// @endcode
///
/// Event naming convention:
/// - Past tense (Created, Added, Configured, Removed, Enqueued): fired after the action
/// - "Begin" prefix (BeginDestroy, BeginShow): fired before the action
/// - "End" prefix (EndShow): fired after a bracketed action
struct ChannelEvents {
    static ChannelEvents& instance();

    // -- Lifecycle events --

    /// Fired after a Channel is constructed via Channel::create()
    fl::function_list<void(const Channel&)> onChannelCreated;

    /// Fired at the start of ~Channel(), before members are torn down
    fl::function_list<void(const Channel&)> onChannelBeginDestroy;

    // -- FastLED list events --

    /// Fired after a Channel is added to FastLED's controller list
    fl::function_list<void(const Channel&)> onChannelAdded;

    /// Fired after a Channel is removed from FastLED's controller list
    fl::function_list<void(const Channel&)> onChannelRemoved;

    // -- Configuration events --

    /// Fired after applyConfig() reconfigures a Channel
    fl::function_list<void(const Channel&, const ChannelConfig&)> onChannelConfigured;

    // -- Rendering events --

    /// Fired after channel data is enqueued to an engine
    /// Second parameter is the engine name (empty string for unnamed engines)
    fl::function_list<void(const Channel&, const fl::string& engineName)> onChannelEnqueued;
};

}  // namespace fl

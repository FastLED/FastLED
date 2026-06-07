#pragma once

#include "fl/stl/function.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"  // IWYU pragma: keep

namespace fl {

class IChannel;
class IChannelDriver;  // IWYU pragma: keep
class ChannelData;
struct ChannelConfig;

/// @brief Singleton event router for Channel lifecycle events
///
/// Centralized event system - supports multiple listeners per event type.
/// All listeners are shared across all channels. Zero per-channel overhead.
///
/// Usage:
/// @code
/// // Add a listener
/// int id = FastLED.channelEvents().onChannelCreated.add([](const IChannel& ch) {
///     FL_DBG("Channel created: " << ch.name());
/// });
///
/// // Add with priority (higher priority executes first)
/// int id2 = FastLED.channelEvents().onChannelCreated.add([](const IChannel& ch) {
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
///
/// Callbacks receive `const IChannel&` so the same listener works regardless
/// of which `Channel<Bus, Chipset>` instantiation produced the event. See
/// `fl/channels/ichannel.h` and issue #2428.
#if defined(FASTLED_DISABLE_CHANNEL_EVENTS) && FASTLED_DISABLE_CHANNEL_EVENTS

namespace detail {
/// No-op event slot used when FASTLED_DISABLE_CHANNEL_EVENTS=1. All
/// `operator()(...)` / `add(...)` / `remove(...)` calls compile and
/// optimize to nothing. The class has no `fl::function_list` template
/// instantiation, so `--gc-sections` can drop the supporting machinery
/// it would have pulled in. See #2931 / #2886.
struct NoOpChannelEvent {
    template <typename... Args>
    void operator()(Args&&...) const FL_NOEXCEPT {}

    template <typename F>
    int add(F&& /*f*/, int /*priority*/ = 0) const FL_NOEXCEPT { return -1; }

    void remove(int /*id*/) const FL_NOEXCEPT {}
};
}  // namespace detail

struct ChannelEvents {
    static ChannelEvents& instance() FL_NOEXCEPT {
        static ChannelEvents s;
        return s;
    }

    detail::NoOpChannelEvent onChannelCreated;
    detail::NoOpChannelEvent onChannelBeginDestroy;
    detail::NoOpChannelEvent onChannelAdded;
    detail::NoOpChannelEvent onChannelRemoved;
    detail::NoOpChannelEvent onChannelConfigured;
    detail::NoOpChannelEvent onChannelDataEncoded;
    detail::NoOpChannelEvent onChannelEnqueued;
};

#else

struct ChannelEvents {
    static ChannelEvents& instance();

    // -- Lifecycle events --

    /// Fired after a Channel is constructed via Channel::create()
    fl::function_list<void(const IChannel&)> onChannelCreated;

    /// Fired at the start of ~Channel(), before members are torn down
    fl::function_list<void(const IChannel&)> onChannelBeginDestroy;

    // -- FastLED list events --

    /// Fired after a Channel is added to FastLED's controller list
    fl::function_list<void(const IChannel&)> onChannelAdded;

    /// Fired after a Channel is removed from FastLED's controller list
    fl::function_list<void(const IChannel&)> onChannelRemoved;

    // -- Configuration events --

    /// Fired after applyConfig() reconfigures a Channel
    fl::function_list<void(const IChannel&, const ChannelConfig&)> onChannelConfigured;

    // -- Rendering events --

    /// Fired after pixel data is encoded into byte stream (before enqueuing)
    /// Second parameter is the encoded channel data
    /// @note This event fires after writeWS2812/writeAPA102/etc. encoding completes
    fl::function_list<void(const IChannel&, const ChannelData&)> onChannelDataEncoded;

    /// Fired after channel data is enqueued to a driver
    /// Second parameter is the driver name (empty string for unnamed drivers)
    fl::function_list<void(const IChannel&, const fl::string& driverName)> onChannelEnqueued;
};

#endif  // FASTLED_DISABLE_CHANNEL_EVENTS

}  // namespace fl

#pragma once

/// @file ichannel.h
/// @brief Type-erased base for the templated `Channel<Bus, Chipset>` family.
///
/// `IChannel` is the non-template polymorphic anchor that `ChannelEvents`
/// callbacks and other consumers receive. Concrete channels (today: `Channel`;
/// after Phase 3b: `Channel<Bus, Chipset>`) inherit from this base so a single
/// callback signature works regardless of which template instantiation produced
/// the channel.
///
/// The surface is intentionally minimal — only the identifiers callbacks need
/// to distinguish one channel from another. Anything richer should be on the
/// concrete `Channel` class. See issue #2428.

#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"
#include "fl/stl/string.h"  // IWYU pragma: keep

namespace fl {

/// @brief Polymorphic identification base for any channel in the system.
class IChannel {
public:
    virtual ~IChannel() FL_NOEXCEPT = default;

    /// @brief Stable monotonically-assigned identifier (starts at 0).
    virtual i32 id() const = 0;

    /// @brief User-specified or auto-generated name (e.g. "Channel_3").
    virtual const fl::string& name() const = 0;

protected:
    IChannel() FL_NOEXCEPT = default;

    // Non-copyable, non-movable -- concrete channels are owned by shared_ptr.
    IChannel(const IChannel&) FL_NOEXCEPT = delete;
    IChannel& operator=(const IChannel&) FL_NOEXCEPT = delete;
    IChannel(IChannel&&) FL_NOEXCEPT = delete;
    IChannel& operator=(IChannel&&) FL_NOEXCEPT = delete;
};

}  // namespace fl

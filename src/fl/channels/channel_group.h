/// @file channel_group.h
/// @brief Interface for PARLIO write coordinator service
///
/// This is a pure write service that handles:
/// - DMA buffer packing and write operations
/// - Frame lifecycle management (IDLE → QUEUING → FLUSHED)
/// - Coordination with IChannelManager for batched write operations
///
/// Strip management (add/remove/get) happens in BulkClockless<PARLIO> specialization.
/// This service receives strip registrations and pixel data, then packs and writes.
///
/// @note This is an internal ESP32 platform header. Do not include directly.

#pragma once

#include "channel.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/chipsets/led_timing.h"  // makeTimingConfig
#include "fl/ptr.h"
#include "fl/span.h"
#include "fl/stdint.h"
#include "pixeltypes.h"

namespace fl {

// Forward declarations
class PixelIterator;

/// @brief Abstract interface for PARLIO write coordinator
///
/// This is a pure service (DESIGN.md "Group" terminology) that handles DMA write operations.
/// Multiple LED strips with identical chipset timing share one instance (singleton per timing).
///
/// Responsibilities:
/// - Frame lifecycle management (IDLE → QUEUING → FLUSHED)
/// - Strip registration (registerStrip/unregisterStrip)
/// - Pixel data buffering (writePixels)
/// - DMA packing and write operations (via IChannelManager)
/// - Singleton factory pattern for managing instances by chipset timing
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class IChannelGroup {
public:
    /// @brief Virtual destructor
    virtual ~IChannelGroup() = default;

    /// @brief Get all channels managed by this group
    /// @return Span of shared pointers to channels
    virtual fl::span<IChannelPtr> getChannels() = 0;

protected:
    /// @brief Protected constructor (interface pattern)
    IChannelGroup() = default;

    // Non-copyable, non-movable
    IChannelGroup(const IChannelGroup&) = delete;
    IChannelGroup& operator=(const IChannelGroup&) = delete;
    IChannelGroup(IChannelGroup&&) = delete;
    IChannelGroup& operator=(IChannelGroup&&) = delete;
};

} // namespace fl

/// @file can_match.h
/// @brief Free function that decides whether a ChannelRequest can be
///        served by a driver's published capability data (#3186).
///
/// The function operates purely on data: a DriverCapabilities snapshot
/// + a span of PinGroup descriptors + a ChannelRequest. No IChannelDriver
/// instance is required, which makes the compatibility matrix testable
/// in isolation against synthetic fixtures.
///
/// IChannelDriver::canMatch() default-implements to this function;
/// the Channel Manager's resolver calls it directly against cached
/// driver snapshots to avoid per-channel virtual dispatch.

#pragma once

#include "fl/channels/capabilities.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"

namespace fl {

/// @brief Test a request (single-pin OR multi-pin ganged) against a
///        driver's published capability data.
///
/// @param caps     Driver-wide descriptor (protocol gates, freq ranges,
///                 clockless timing model, flags).
/// @param groups   The driver's per-resource group list.
/// @param request  Channel request — data_pins bitset + clock_pin +
///                 chipset timing windows + optional frequency override.
///                 A single-pin request has exactly one bit set in
///                 data_pins.
///
/// @return HandleResult::Yes if any group accepts the request; otherwise
///         the strongest rejection reason observed across all groups.
HandleResult canMatch(
    const DriverCapabilities& caps,
    fl::span<const PinGroup>  groups,
    const ChannelRequest&     request) FL_NO_EXCEPT;

}  // namespace fl

/// @file channel.h
/// @brief ESP32-P4 Parallel IO (PARLIO) LED channel interface
///
/// This driver uses the ESP32-P4 PARLIO TX peripheral to drive up to 16
/// identical WS28xx-style LED strips in parallel with DMA-based hardware timing.
///
/// Key features:
/// - Simultaneous output to multiple LED strips
/// - DMA-based write operation (minimal CPU overhead)
/// - Hardware timing control (no CPU bit-banging)
/// - Runtime-configured for different channel counts and chipset timings

#pragma once

#include "fl/cled_controller.h"
#include "fl/ptr.h"
#include "fl/stdint.h"
#include "pixeltypes.h"

namespace fl {

// Forward declarations
struct ChipsetTimingConfig;
struct ChannelConfig;

FASTLED_SHARED_PTR(IChannel);

/// @brief Abstract interface for PARLIO LED channel
///
/// This interface provides platform-independent access to the ESP32-P4's
/// Parallel IO TX peripheral for driving multiple LED strips in parallel.
///
/// Implementation is hidden in .cpp file for complete platform isolation.
class IChannel: public CLEDController {
public:

    /// @brief Virtual destructor
    virtual ~IChannel() = default;

    /// @brief Get the channel ID
    /// @return Channel ID (always increments, starts at 0)
    virtual int id() const = 0;

    /// @brief Get the channel configuration
    /// @return ChannelConfig object containing all channel settings
    virtual const ChannelConfig& getConfig() const = 0;


    virtual IChannel& setCorrection(CRGB correction) = 0;
    virtual IChannel& setTemperature(CRGB temperature) = 0;
    virtual IChannel& setDither(fl::u8 ditherMode = BINARY_DITHER) = 0;
    virtual IChannel& setRgbw(const Rgbw& rgbw = RgbwDefault::value()) = 0;

protected:
    /// @brief Protected constructor (interface pattern)
    IChannel() = default;

    // Non-copyable, non-movable
    IChannel(const IChannel&) = delete;
    IChannel& operator=(const IChannel&) = delete;
    IChannel(IChannel&&) = delete;
    IChannel& operator=(IChannel&&) = delete;
};

}  // namespace fl

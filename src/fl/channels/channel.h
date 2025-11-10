/// @file channel.h
/// @brief ESP32-P4 Parallel IO (PARLIO) LED channel
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

#include "cpixel_ledcontroller.h"
#include "pixel_controller.h"
#include "fl/ptr.h"
#include "fl/stdint.h"
#include "pixeltypes.h"
#include "channel_config.h"

namespace fl {

// Forward declarations
class ChannelEngine;
class ChannelData;

class Channel;
FASTLED_SHARED_PTR(Channel);
FASTLED_SHARED_PTR(ChannelData);

/// @brief LED channel for parallel output
///
/// Provides access to LED channel functionality for driving LED strips.
/// RGB_ORDER is set to RGB - reordering is handled internally by the Channel
class Channel: protected CPixelLEDController<RGB> {
public:
    /// @brief Create a new channel
    /// @param config Channel configuration
    /// @param engine Channel engine this channel belongs to
    /// @return Shared pointer to the channel
    static ChannelPtr create(const ChannelConfig& config, ChannelEngine* engine);

    /// @brief Destructor
    ~Channel();

    /// @brief Get the channel ID
    /// @return Channel ID (always increments, starts at 0)
    int32_t id() const { return mId; }

    /// @brief Get the channel configuration
    /// @return ChannelConfig object containing all channel settings
    const ChannelConfig& getConfig() const { return mConfig; }

    Channel& setCorrection(CRGB correction);
    Channel& setTemperature(CRGB temperature);
    Channel& setDither(fl::u8 ditherMode = BINARY_DITHER);
    Channel& setRgbw(const Rgbw& rgbw = RgbwDefault::value());

    /// @brief Dispose of this channel and remove it from CLEDController linked list
    /// @note Safe to call at any time - channels currently drawing are protected by shared_ptr<>
    void dispose();

    /// @brief Get the channel engine this channel belongs to
    /// @return Pointer to the ChannelEngine, or nullptr if not set
    ChannelEngine* getChannelEngine() const { return mEngine; }

    /// @brief Set the channel engine this channel belongs to
    /// @param engine Pointer to the ChannelEngine
    void setChannelEngine(ChannelEngine* engine) { mEngine = engine; }

private:
    // CPixelLEDController interface implementation
    virtual void showPixels(PixelController<RGB, 1, 0xFFFFFFFF>& pixels) override;
    virtual void init() override;

    /// @brief Friend declaration for make_shared to access private constructor
    template<typename T, typename... Args>
    friend fl::shared_ptr<T> fl::make_shared(Args&&... args);

    /// @brief Private constructor (use create() factory method)
    Channel(const ChannelConfig& config, ChannelEngine* engine);

    // Non-copyable, non-movable
    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;
    Channel(Channel&&) = delete;
    Channel& operator=(Channel&&) = delete;

    static int32_t nextId();

    const ChannelConfig mConfig;
    ChannelEngine* mEngine;
    const int32_t mId;
    ChannelDataPtr mChannelData;
};

}  // namespace fl

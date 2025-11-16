/// @file channel_engine_parlio.h
/// @brief Parallel IO implementation of ChannelEngine for ESP32

#pragma once

#include "fl/compiler_control.h"
#ifdef ESP32

#include "fl/channels/channel_engine.h"
#include "fl/channels/channel_data.h"
#include "fl/engine_events.h"
#include "ftl/span.h"
#include "ftl/vector.h"

namespace fl {

/// @brief Parallel IO-based ChannelEngine implementation
///
/// Manages parallel LED output using ESP32's Parallel IO peripheral.
/// Allows simultaneous transmission to multiple LED strips on parallel pins.
///
/// Listens to onEndFrame events and manages transmission state.
class ChannelEnginePARLIO : public ChannelEngine, public EngineEvents::Listener {
public:
    ChannelEnginePARLIO();
    ~ChannelEnginePARLIO() override;

    // EngineEvents::Listener interface
    void onEndFrame() override;

protected:
    /// @brief Query engine state (hardware polling implementation)
    /// @return Current engine state (READY, BUSY, or ERROR)
    EngineState pollDerived() override;

    /// @brief Begin LED data transmission for all channels
    /// @param channelData Span of channel data to transmit
    void beginTransmission(fl::span<const ChannelDataPtr> channelData) override;

private:
    /// @brief Initialize PARLIO peripheral on first use
    void initializeIfNeeded();

    /// @brief Initialization flag
    bool mInitialized;
};

} // namespace fl

#endif // ESP32

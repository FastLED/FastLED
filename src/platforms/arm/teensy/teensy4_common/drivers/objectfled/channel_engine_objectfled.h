/// @file channel_engine_objectfled.h
/// @brief ObjectFLED DMA-based channel engine for Teensy 4.x
///
/// Implements IChannelDriver wrapping ObjectFLED's 3:1 DMA bit transposition.
/// ObjectFLED's show() is synchronous (blocks until DMA completes), so the
/// state machine is simply: READY -> BUSY -> READY (no DRAINING).
///
/// Data format: ChannelData contains raw RGB/RGBW bytes (from ws2812 encoder),
/// which is exactly what ObjectFLED expects in its frameBufferLocal.

#pragma once

// IWYU pragma: private

#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/vector.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declarations
struct TimingGroup;
class IObjectFLEDPeripheral;

/// @brief ObjectFLED DMA-based channel engine for Teensy 4.x
///
/// Uses ObjectFLED's 3:1 DMA bit transposition for parallel LED output.
/// Groups channels by ChipsetTimingConfig, creates one ObjectFLED instance
/// per timing group. ObjectFLED instances are cached across frames to avoid
/// re-initializing DMA hardware every frame.
///
/// Strict chipset filtering: Only handles clockless chipsets with total bit
/// period 1000-2500ns (standard WS2812/SK6812 range). Rejects WS2816 and
/// other HD chipsets that use different encoding.
///
/// State machine: READY -> BUSY -> READY (synchronous DMA)
class ChannelEngineObjectFLED : public IChannelDriver {
public:
    /// @brief Construct with platform-default peripheral
    ChannelEngineObjectFLED() FL_NOEXCEPT;

    /// @brief Construct with injected peripheral (for testing)
    explicit ChannelEngineObjectFLED(fl::shared_ptr<IObjectFLEDPeripheral> peripheral) FL_NOEXCEPT;

    ~ChannelEngineObjectFLED() override;

    /// @brief Check if this engine can handle the given channel data
    /// @return true for clockless chipsets with total period 1000-2500ns
    bool canHandle(const ChannelDataPtr& data) const FL_NOEXCEPT override;

    /// @brief Enqueue channel data for transmission
    void enqueue(ChannelDataPtr channelData) FL_NOEXCEPT override;

    /// @brief Trigger transmission of enqueued data (synchronous DMA)
    void show() FL_NOEXCEPT override;

    /// @brief Query engine state
    /// @return READY (ObjectFLED show() is synchronous, always completes before returning)
    DriverState poll() FL_NOEXCEPT override;

    /// @brief Get engine name
    /// @return "OBJECT_FLED" — matches the `fl::Bus::OBJECT_FLED` enumerator spelling.
    fl::string getName() const FL_NOEXCEPT override {
        return fl::string::from_literal("OBJECT_FLED");
    }

    /// @brief Get capabilities (clockless only)
    Capabilities getCapabilities() const FL_NOEXCEPT override {
        return Capabilities(true, false);
    }

private:
    /// @brief Peripheral abstraction (real hardware or mock)
    fl::shared_ptr<IObjectFLEDPeripheral> mPeripheral;

    /// @brief Channels waiting for show()
    fl::vector<ChannelDataPtr> mEnqueuedChannels;

    /// @brief Channels currently transmitting (for isInUse tracking)
    fl::vector<ChannelDataPtr> mTransmittingChannels;

    /// @brief Persistent timing groups (cached across frames)
    fl::vector<fl::unique_ptr<TimingGroup>> mTimingGroups;
};

} // namespace fl

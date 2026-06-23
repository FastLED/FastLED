/// @file channel_engine_flexio.h
/// @brief FlexIO2-based channel engine for Teensy 4.x
///
/// Implements IChannelDriver using FlexIO2 hardware for WS2812 waveform generation.
/// Uses 4-timer + 1-shifter architecture with DMA for asynchronous transmission.
///
/// Unlike ObjectFLED (which uses QTimer + XBAR + eDMA + GPIO), this driver uses
/// FlexIO2's built-in waveform generation with timer OR'ing on the output pin.
///
/// State machine: READY → BUSY → READY (asynchronous DMA)
/// - show() starts DMA and returns immediately
/// - poll() checks DMA completion

#pragma once

// IWYU pragma: private

#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/chipsets/chipset_timing_config.h"
#include "fl/stl/vector.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {

// Forward declaration
class IFlexIOPeripheral;

/// @brief FlexIO2-based channel engine for Teensy 4.x
///
/// Uses FlexIO2's 4-timer + 1-shifter architecture to generate WS2812/SK6812
/// waveforms directly. Each strip requires one FlexIO2 module instance (4 timers
/// + 1 shifter), so this engine supports a single strip at a time per FlexIO module.
///
/// Pin filtering: Only handles pins that map to FlexIO2 (pins 6-13, 32 on
/// Teensy 4.0/4.1). Non-FlexIO pins fall through to ObjectFLED.
///
/// Strict chipset filtering: Only handles clockless chipsets with total bit
/// period 1000-2500ns (standard WS2812/SK6812 range).
class ChannelEngineFlexIO : public IChannelDriver {
public:
    /// @brief Construct with platform-default peripheral
    ChannelEngineFlexIO() FL_NOEXCEPT;

    /// @brief Construct with injected peripheral (for testing)
    explicit ChannelEngineFlexIO(fl::shared_ptr<IFlexIOPeripheral> peripheral) FL_NOEXCEPT;

    ~ChannelEngineFlexIO() override;

    /// @brief Check if this engine can handle the given channel data
    /// @return true for clockless chipsets on FlexIO2-capable pins with period 1000-2500ns
    bool canHandle(const ChannelDataPtr& data) const FL_NOEXCEPT override;

    /// @brief Enqueue channel data for transmission
    void enqueue(ChannelDataPtr channelData) FL_NOEXCEPT override;

    /// @brief Trigger transmission of enqueued data (asynchronous DMA)
    void show() FL_NOEXCEPT override;

    /// @brief Query engine state
    /// @return READY when idle, BUSY during DMA transfer
    DriverState poll() FL_NOEXCEPT override;

    /// @brief Get engine name
    /// @return "FLEX_IO" — matches the `fl::Bus::FLEX_IO` enumerator spelling.
    fl::string getName() const FL_NOEXCEPT override {
        return fl::string::from_literal("FLEX_IO");
    }

    /// @brief Get capabilities (clockless only)
    Capabilities getCapabilities() const FL_NOEXCEPT override {
        return Capabilities(true, false);
    }

private:
    /// @brief Peripheral abstraction (real hardware or mock)
    fl::shared_ptr<IFlexIOPeripheral> mPeripheral;

    /// @brief Channels waiting for show()
    fl::vector<ChannelDataPtr> mEnqueuedChannels;

    /// @brief Channels currently transmitting
    fl::vector<ChannelDataPtr> mTransmittingChannels;

    /// @brief Whether FlexIO hardware has been initialized for current pin/timing
    bool mHwInitialized;

    /// @brief Currently configured pin (0xFF = none)
    u8 mCurrentPin;

    /// @brief Currently configured timing
    ChipsetTimingConfig mCurrentTiming;
};

} // namespace fl

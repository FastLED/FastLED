#pragma once

// IWYU pragma: private

/// @file bitbang_channel_driver.h
/// @brief Bit-bang IChannelDriver that drives up to 8 GPIO pins in parallel
///
/// Supports both clockless (WS2812-style) and SPI (APA102-style) protocols.
/// Fully blocking — show() does all transmission synchronously, poll() always
/// returns READY.
///
/// Clockless: Uses three-phase delay pattern (T1/T2/T3) matching
/// ClocklessBlockingGeneric, driving all 8 data pins simultaneously via
/// DigitalMultiWrite8.
///
/// SPI: Bit-bangs a shared clock pin via fl::digitalWrite() while driving up
/// to 8 data pins in parallel via DigitalMultiWrite8.

#include "fl/channels/driver.h"
#include "fl/channels/data.h"
#include "fl/pins.h"
#include "fl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"

namespace fl {

class BitBangChannelDriver : public IChannelDriver {
public:
    BitBangChannelDriver();
    ~BitBangChannelDriver() override;

    bool canHandle(const ChannelDataPtr& data) const override;
    void enqueue(ChannelDataPtr channelData) override;
    void show() override;
    DriverState poll() override;

    fl::string getName() const override;
    Capabilities getCapabilities() const override;

    /// @brief Access the pre-initialized DigitalMultiWrite8 for external use
    /// @return Reference to the multi-writer (valid after show() calls rebuildPinConfig)
    const DigitalMultiWrite8& getMultiWriter() const { return mMultiWriter; }

    /// @brief Access the pre-initialized DigitalMultiWrite8 (mutable)
    DigitalMultiWrite8& getMultiWriter() { return mMultiWriter; }

private:
    void rebuildPinConfig(fl::span<const ChannelDataPtr> channels);

    // Pin management: maps pin numbers to slot indices and back.
    // mSlotForPin: indexed by pin number (0-255), value is slot index (0-7) or -1.
    // mPinForSlot: indexed by slot (0-7), value is pin number or -1.
    int mSlotForPin[256];
    int mPinForSlot[8];
    int mNumActiveSlots;
    u8 mActiveSlotMask;  // bitmask of active slots for fast all-HIGH writes
    DigitalMultiWrite8 mMultiWriter;

    // Clockless transmission
    void transmitClockless(fl::span<const ChannelDataPtr> channels);
    void transmitClocklessBit(u8 onesMask, u32 t1_ns, u32 t2_ns, u32 t3_ns);

    // SPI transmission
    void transmitSpi(fl::span<const ChannelDataPtr> channels);

    // Two-queue pattern
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
};

}  // namespace fl

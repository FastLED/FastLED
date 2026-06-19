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
#include "fl/system/pins.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

namespace fl {

class BitBangChannelDriver : public IChannelDriver {
public:
    BitBangChannelDriver() FL_NO_EXCEPT;
    ~BitBangChannelDriver() override;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;

    fl::string getName() const FL_NO_EXCEPT override;
    Capabilities getCapabilities() const FL_NO_EXCEPT override;

    /// @brief Access the pre-initialized DigitalMultiWrite8 for external use
    /// @return Reference to the multi-writer (valid after show() calls rebuildPinConfig)
    const DigitalMultiWrite8& getMultiWriter() const FL_NO_EXCEPT { return mMultiWriter; }

    /// @brief Access the pre-initialized DigitalMultiWrite8 (mutable)
    DigitalMultiWrite8& getMultiWriter() FL_NO_EXCEPT { return mMultiWriter; }

private:
    void rebuildPinConfig(fl::span<const ChannelDataPtr> channels) FL_NO_EXCEPT;

    // Pin management: maps pin numbers to slot indices and back.
    // mSlotForPin: indexed by pin number (0-255), value is slot index (0-7) or -1.
    // mPinForSlot: indexed by slot (0-7), value is pin number or -1.
    int mSlotForPin[256];
    int mPinForSlot[8];
    int mNumActiveSlots;
    u8 mActiveSlotMask;  // bitmask of active slots for fast all-HIGH writes
    DigitalMultiWrite8 mMultiWriter;

    // Clockless transmission
    void transmitClockless(fl::span<const ChannelDataPtr> channels) FL_NO_EXCEPT;
    void transmitClocklessBit(u8 onesMask, u32 t1_ns, u32 t2_ns, u32 t3_ns) FL_NO_EXCEPT;

    // SPI transmission
    void transmitSpi(fl::span<const ChannelDataPtr> channels) FL_NO_EXCEPT;

    // Two-queue pattern
    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
};

}  // namespace fl

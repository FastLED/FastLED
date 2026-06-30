/// @file channel_engine_objectfled.h
/// @brief ObjectFLED DMA-based channel engine for Teensy 4.x
///        (unified clockless + SPI per #3428)
///
/// Implements IChannelDriver wrapping ObjectFLED's 3:1 DMA bit transposition
/// for clockless WS2812 output AND a DMA-bit-banged SPI mode for APA102 /
/// SK9822-class chipsets. One engine, one portable Bus slot (`Bus::FLEX_IO`, which 0),
/// one peripheral (FlexPWM + eDMA + GPIO bank) -- the mode switch happens
/// inside `show()` based on `ChannelData::isSpi()` vs `isClockless()`. See
/// `src/fl/channels/README.md` -> "Rule: Parallel-IO peripherals -- one
/// engine for both clockless and SPI modes".
///
/// ObjectFLED's show() starts DMA and returns while output/latch timing may
/// still be active, so poll() owns the DRAINING -> READY transition.
///
/// Data format: ChannelData contains raw RGB/RGBW bytes (from the WS2812
/// encoder) plus explicit ChannelPixelFormat metadata. The engine uses that
/// metadata for ObjectFLED's raw frameBufferLocal layout; it does not infer
/// RGBW from byte counts.

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
/// State machine: READY -> BUSY -> DRAINING -> READY
class ChannelEngineObjectFLED : public IChannelDriver {
public:
    /// @brief Construct with platform-default peripheral
    ChannelEngineObjectFLED() FL_NO_EXCEPT;

    /// @brief Construct with injected peripheral (for testing)
    explicit ChannelEngineObjectFLED(fl::shared_ptr<IObjectFLEDPeripheral> peripheral) FL_NO_EXCEPT;

    ~ChannelEngineObjectFLED() override;

    /// @brief Check if this engine can handle the given channel data
    /// @return true for:
    ///   - Clockless chipsets with total period 1000-2500ns (RGB/RGBW)
    ///   - SPI chipsets whose (MOSI, SCLK) pair lives on the same DMA-able
    ///     GPIO bank (#3428; hardware impl pending so canHandle currently
    ///     returns false for SPI)
    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;

    /// @brief Enqueue channel data for transmission
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;

    /// @brief Trigger transmission of enqueued data
    void show() FL_NO_EXCEPT override;

    /// @brief Query engine state
    /// @return DRAINING while ObjectFLED DMA/latch timing is active
    DriverState poll() FL_NO_EXCEPT override;

    /// @brief Get engine name
    /// @return "OBJECT_FLED" as the concrete Teensy vendor driver name.
    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("OBJECT_FLED");
    }

    /// @brief Get capabilities (unified clockless + SPI per #3428)
    ///
    /// Returns `(clockless=true, spi=true)` because the FlexPWM + eDMA +
    /// GPIO bank peripheral can serve both modes. SPI runtime serving is
    /// gated by `FL_OBJECTFLED_SPI_HARDWARE_ENABLE` and the runtime
    /// `objectfled_spi_lookup_pins()` -- the architectural slot is
    /// permanent. See `src/fl/channels/README.md` -> "Rule: Parallel-IO
    /// peripherals -- one engine for both clockless and SPI modes". Body
    /// lives in `channel_engine_objectfled.cpp.hpp` per the `src/**/*.h`
    /// header-discipline rule.
    Capabilities getCapabilities() const FL_NO_EXCEPT override;

private:
    /// @brief Peripheral abstraction (real hardware or mock)
    fl::shared_ptr<IObjectFLEDPeripheral> mPeripheral;

    /// @brief Channels waiting for show()
    fl::vector<ChannelDataPtr> mEnqueuedChannels;

    /// @brief Channels currently transmitting (for isInUse tracking)
    fl::vector<ChannelDataPtr> mTransmittingChannels;

    /// @brief Persistent timing groups (cached across frames)
    fl::vector<fl::unique_ptr<TimingGroup>> mTimingGroups;

    size_t mCurrentGroupIndex = 0;

    bool startNextTimingGroup() FL_NO_EXCEPT;
    bool startTimingGroup(TimingGroup& group) FL_NO_EXCEPT;
    void finishTransmission() FL_NO_EXCEPT;
};

} // namespace fl

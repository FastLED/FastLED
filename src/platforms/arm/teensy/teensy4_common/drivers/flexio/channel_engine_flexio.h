/// @file channel_engine_flexio.h
/// @brief FlexIO2-based channel engine for Teensy 4.x (unified clockless + SPI)
///
/// Implements IChannelDriver using FlexIO2 hardware for BOTH WS2812 clockless
/// waveform generation AND APA102/SK9822-class SPI transmission. One engine,
/// one Bus enum slot (`Bus::FLEX_IO`), one peripheral -- the mode switch
/// happens inside `show()` based on `ChannelData::isSpi()` vs `isClockless()`.
/// See `src/fl/channels/README.md` -> "Rule: Parallel-IO peripherals -- one
/// engine for both clockless and SPI modes" and #3428.
///
/// Unlike ObjectFLED (which uses QTimer + XBAR + eDMA + GPIO), this driver uses
/// FlexIO2's built-in shifter/timer hardware: 1-shifter + 1-timer for clockless
/// WS2812, and 1-shifter + 1-timer in SPI master mode for APA102/SK9822.
///
/// State machine: READY → BUSY → READY (asynchronous DMA)
/// - show() starts DMA and returns immediately
/// - poll() checks DMA completion

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

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
    ChannelEngineFlexIO() FL_NO_EXCEPT;

    /// @brief Construct with injected peripheral (for testing)
    explicit ChannelEngineFlexIO(fl::shared_ptr<IFlexIOPeripheral> peripheral) FL_NO_EXCEPT;

    ~ChannelEngineFlexIO() override;

    /// @brief Check if this engine can handle the given channel data
    /// @return true for:
    ///   - Clockless chipsets on FlexIO2-capable pins with period 1000-2500ns
    ///   - SPI chipsets whose (MOSI, SCLK) pin pair is FlexIO2-routable (#3428;
    ///     hardware impl pending so canHandle currently returns false for SPI)
    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;

    /// @brief Enqueue channel data for transmission
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;

    /// @brief Trigger transmission of enqueued data (asynchronous DMA)
    void show() FL_NO_EXCEPT override;

    /// @brief Query engine state
    /// @return READY when idle, BUSY during DMA transfer
    DriverState poll() FL_NO_EXCEPT override;

    /// @brief Get engine name
    /// @return "FLEX_IO" — matches the `fl::Bus::FLEX_IO` enumerator spelling.
    fl::string getName() const FL_NO_EXCEPT override {
        return fl::string::from_literal("FLEX_IO");
    }

    /// @brief Get capabilities (unified clockless + SPI per #3428)
    ///
    /// Returns `(clockless=true, spi=true)` because FlexIO2 can serve both
    /// modes from the same peripheral. SPI runtime support is currently
    /// gated by `flexio_spi_lookup_pins()` which returns `false` until the
    /// hardware impl lands -- so SPI channels are rejected by `canHandle()`
    /// today but the architectural slot is in place. See `src/fl/channels/
    /// README.md` -> "Rule: Parallel-IO peripherals -- one engine for both
    /// clockless and SPI modes".
    Capabilities getCapabilities() const FL_NO_EXCEPT override {
        return Capabilities(true, true);
    }

private:
    /// @brief Peripheral abstraction (real hardware or mock)
    fl::shared_ptr<IFlexIOPeripheral> mPeripheral;

    /// @brief Channels waiting for show()
    fl::vector<ChannelDataPtr> mEnqueuedChannels;

    /// @brief Channels currently transmitting
    fl::vector<ChannelDataPtr> mTransmittingChannels;

    /// @brief Which FlexIO2 mode the peripheral is currently programmed for.
    /// Used by `show()` to decide whether the peripheral needs to be torn
    /// down and reinitialized when the next channel uses a different mode.
    enum class Mode : u8 {
        Uninitialized = 0,
        Clockless,
#if defined(FL_IS_TEENSY_4X)
        Spi,
#endif
    };

    /// @brief Current peripheral mode (Uninitialized at boot)
    Mode mCurrentMode = Mode::Uninitialized;

    /// @brief Whether FlexIO hardware has been initialized for current pin/timing
    bool mHwInitialized;

    /// @brief Currently configured clockless pin (0xFF = none)
    u8 mCurrentPin;

    /// @brief Currently configured clockless timing
    ChipsetTimingConfig mCurrentTiming;

#if defined(FL_IS_TEENSY_4X)
    /// @brief Currently configured SPI MOSI pin (0xFF = none).
    /// Only present on Teensy 4.x (FlexIO2 SPI mode is platform-gated; the
    /// SPI hardware helpers in `flexio_spi_mode.h` are compiled out
    /// elsewhere, and `show()` never references these fields off-Teensy).
    u8 mCurrentSpiMosi = 0xFF;

    /// @brief Currently configured SPI SCLK pin (0xFF = none)
    u8 mCurrentSpiSclk = 0xFF;

    /// @brief Currently configured SPI clock rate (Hz; 0 = none)
    u32 mCurrentSpiClockHz = 0;
#endif  // FL_IS_TEENSY_4X
};

} // namespace fl

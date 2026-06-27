// IWYU pragma: private

#ifndef CHANNEL_ENGINE_FLEXIO_CPP_HPP_
#define CHANNEL_ENGINE_FLEXIO_CPP_HPP_

#include "platforms/arm/teensy/is_teensy.h"

// Channel engine compiles on all platforms (uses IFlexIOPeripheral abstraction)
// Only the real peripheral implementation is Teensy-specific

#include "platforms/arm/teensy/teensy4_common/drivers/flexio/channel_engine_flexio.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/iflexio_peripheral.h"
#include "platforms/arm/teensy/teensy4_common/drivers/flexio/flexio_spi_mode.h"

#include "fl/log/log.h"
#include "fl/stl/noexcept.h"

#if defined(FL_IS_TEENSY_4X)
// IWYU pragma: begin_keep
#include <Arduino.h>
// IWYU pragma: end_keep
#endif

namespace fl {

// Timing period bounds for canHandle() filtering.
// Same range as ObjectFLED - standard clockless protocols only.
static constexpr u32 kFlexIOMinPeriodNs = 1000;
static constexpr u32 kFlexIOMaxPeriodNs = 2500;

// ============================================================================
// ChannelEngineFlexIO Implementation
// ============================================================================

ChannelEngineFlexIO::ChannelEngineFlexIO()
 FL_NO_EXCEPT : mPeripheral(IFlexIOPeripheral::create()),
      mHwInitialized(false), mCurrentPin(0xFF) {
    FL_LOG_FLEXIO_F("ChannelEngineFlexIO: created");
}

ChannelEngineFlexIO::ChannelEngineFlexIO(fl::shared_ptr<IFlexIOPeripheral> peripheral)
 FL_NO_EXCEPT : mPeripheral(fl::move(peripheral)),
      mHwInitialized(false), mCurrentPin(0xFF) {
    FL_LOG_FLEXIO_F("ChannelEngineFlexIO: created (injected peripheral)");
}

ChannelEngineFlexIO::~ChannelEngineFlexIO() {
    if (mPeripheral) {
        mPeripheral->deinit();
    }
    FL_LOG_FLEXIO_F("ChannelEngineFlexIO: destroyed");
}

// Moved out of the header per `src/**/*.h` header-discipline (CodeRabbit #3431).
IChannelDriver::Capabilities ChannelEngineFlexIO::getCapabilities() const FL_NO_EXCEPT {
    return Capabilities(true, true);
}

bool ChannelEngineFlexIO::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data) {
        return false;
    }

    if (data->isClockless()) {
        // Check timing range
        const u32 period = data->getTiming().total_period_ns();
        if (period < kFlexIOMinPeriodNs || period > kFlexIOMaxPeriodNs) {
            return false;
        }

        // Check if pin has FlexIO2 mapping
        if (!mPeripheral) {
            return false;
        }
        const u8 pin = static_cast<u8>(data->getPin());
        return mPeripheral->canHandlePin(pin);
    }

#if defined(FL_IS_TEENSY_4X)
    if (data->isSpi()) {
        // #3428: SPI-mode dispatch on the SAME peripheral (unified engine).
        // The (MOSI, SCLK) pin pair must both be FlexIO2-routable. Today the
        // pin-pair table is stubbed (returns false); the real lookup lands
        // with the SPI hardware impl in a follow-up PR, at which point this
        // branch starts claiming SPI channels.
        const auto* spi = data->getChipset().ptr<SpiChipsetConfig>();
        if (!spi || spi->dataPin < 0 || spi->clockPin < 0) {
            return false;
        }
        FlexIOSPIPinInfo info;
        return flexio_spi_lookup_pins(static_cast<u8>(spi->dataPin),
                                      static_cast<u8>(spi->clockPin), &info);
    }
#endif

    return false;
}

void ChannelEngineFlexIO::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData) {
        channelData->setInUse(true);
        mEnqueuedChannels.push_back(fl::move(channelData));
    }
}

void ChannelEngineFlexIO::show() FL_NO_EXCEPT {
    if (!mPeripheral) {
        return;
    }

    // Wait for any previous transmission (per DMA Wait Pattern). Bounded
    // wait so a stuck FlexIO from a previous show cannot hang subsequent
    // shows -- if poll never becomes READY, force-drop the previous channel
    // set and proceed. 100 ms is plenty for any reasonable WS2812 frame.
#if defined(FL_IS_TEENSY_4X)
    const u32 spin_start = millis();
    while (poll() != DriverState::READY) {
        if ((u32)(millis() - spin_start) >= 100) {
            // Previous transmission stuck -- force-clear so we can proceed.
            for (auto& ch : mTransmittingChannels) {
                ch->setInUse(false);
            }
            mTransmittingChannels.clear();
            break;
        }
    }
#else
    while (poll() != DriverState::READY) {
        // Spin until FlexIO DMA completes
    }
#endif

    // Move enqueued → transmitting
    mTransmittingChannels.swap(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    if (mTransmittingChannels.empty()) {
        return;
    }

    // #3416 FX-LOW-6: current driver serialises strips through one
    // shifter + one timer. FlexIO2 hardware supports up to 8 parallel
    // shifters (RM 47.3 Features); a future PR could parallelise
    // multi-strip frames to bring perf from N*t_strip to t_strip.
    // For now strips are transmitted sequentially.
    //
    // #3428: per-channel mode routing. FlexIO2 is one peripheral that can
    // run in either WS2812 clockless mode or APA102/SK9822 SPI master mode.
    // We route each channel to the matching path and reconfigure the
    // peripheral on mode change.
    for (auto& ch : mTransmittingChannels) {
        if (ch->isClockless()) {
            // ----- Clockless WS2812 / SK6812 path -----
            const u8 pin = static_cast<u8>(ch->getPin());
            const ChipsetTimingConfig& timing = ch->getTiming();

            // Check if we need to reinitialize hardware (mode, pin, or timing changed)
            const bool need_reinit = !mHwInitialized
                                  || mCurrentMode != Mode::Clockless
                                  || pin != mCurrentPin
                                  || timing != mCurrentTiming;
            if (need_reinit) {
#if defined(FL_IS_TEENSY_4X)
                if (mCurrentMode == Mode::Spi) {
                    flexio_spi_deinit();
                }
#endif
                mPeripheral->deinit();
                // Reset cached state immediately after deinit so any later
                // `continue` (e.g. pin invalid, init failure) doesn't leave
                // mHwInitialized/mCurrentMode advertising a torn-down config.
                // Per coderabbitai review on PR #3431.
                mHwInitialized = false;
                mCurrentMode = Mode::Uninitialized;
                mCurrentPin = 0xFF;

                if (!mPeripheral->canHandlePin(pin)) {
                    FL_LOG_FLEXIO_F("ChannelEngineFlexIO: Pin %s not FlexIO2-capable", (int)pin);
                    continue;
                }

                // T0H = t1_ns, T1H = t1_ns + t2_ns, period = t1+t2+t3
                u32 t0h = timing.t1_ns;
                u32 t1h = timing.t1_ns + timing.t2_ns;
                u32 period = timing.total_period_ns();

                if (!mPeripheral->init(pin, t0h, t1h, period, timing.reset_us)) {
                    FL_LOG_FLEXIO_F("ChannelEngineFlexIO: Failed to init FlexIO for pin %s", (int)pin);
                    continue;
                }

                mCurrentPin = pin;
                mCurrentTiming = timing;
                mCurrentMode = Mode::Clockless;
                mHwInitialized = true;
            }

            // Transmit pixel data
            const auto& payload = ch->getData();
            if (!payload.empty()) {
                mPeripheral->show(payload.data(), static_cast<u32>(payload.size()));

                // Wait for this strip to complete before moving to next
                // (FlexIO can only do one strip at a time)
                mPeripheral->wait();
            }
            continue;
        }

#if defined(FL_IS_TEENSY_4X)
        if (ch->isSpi()) {
            // ----- APA102 / SK9822 SPI master path (#3428) -----
            // Hardware impl is pending; the helpers stub-return false until
            // the follow-up PR lands. canHandle() also rejects SPI today
            // via the stubbed flexio_spi_lookup_pins(), so this branch is
            // wired-up but unreachable in production until then.
            const auto* spi = ch->getChipset().ptr<SpiChipsetConfig>();
            if (!spi) {
                continue;
            }
            const u8 mosi = static_cast<u8>(spi->dataPin);
            const u8 sclk = static_cast<u8>(spi->clockPin);
            const u32 clock_hz = spi->timing.clock_hz;

            const bool need_reinit = !mHwInitialized
                                  || mCurrentMode != Mode::Spi
                                  || mosi != mCurrentSpiMosi
                                  || sclk != mCurrentSpiSclk
                                  || clock_hz != mCurrentSpiClockHz;
            if (need_reinit) {
                if (mCurrentMode == Mode::Clockless) {
                    mPeripheral->deinit();
                }
                flexio_spi_deinit();
                // Reset cached state immediately after deinit so any later
                // `continue` (e.g. pin lookup fails, init fails) doesn't
                // leave mHwInitialized/mCurrentMode advertising a torn-down
                // config. Per coderabbitai review on PR #3431.
                mHwInitialized = false;
                mCurrentMode = Mode::Uninitialized;
                mCurrentSpiMosi = 0xFF;
                mCurrentSpiSclk = 0xFF;
                mCurrentSpiClockHz = 0;

                FlexIOSPIPinInfo info;
                if (!flexio_spi_lookup_pins(mosi, sclk, &info)) {
                    FL_LOG_FLEXIO_F("ChannelEngineFlexIO: SPI pin pair (%s,%s) not FlexIO2-routable",
                                    (int)mosi, (int)sclk);
                    continue;
                }
                if (!flexio_spi_init(info, clock_hz)) {
                    FL_LOG_FLEXIO_F("ChannelEngineFlexIO: flexio_spi_init failed (mosi=%s, sclk=%s, hz=%s)",
                                    (int)mosi, (int)sclk, (int)clock_hz);
                    continue;
                }

                mCurrentSpiMosi = mosi;
                mCurrentSpiSclk = sclk;
                mCurrentSpiClockHz = clock_hz;
                mCurrentMode = Mode::Spi;
                mHwInitialized = true;
            }

            const auto& payload = ch->getData();
            if (!payload.empty()) {
                flexio_spi_show(payload.data(), static_cast<u32>(payload.size()));
                flexio_spi_wait();
            }
            continue;
        }
#endif

        // Unknown chipset family -- silently skip rather than asserting so
        // a future ChipsetVariant addition doesn't break this driver.
    }

    // Clear isInUse flags
    for (auto& ch : mTransmittingChannels) {
        ch->setInUse(false);
    }
    mTransmittingChannels.clear();
}

IChannelDriver::DriverState ChannelEngineFlexIO::poll() FL_NO_EXCEPT {
    if (!mTransmittingChannels.empty()) {
        if (mPeripheral && mPeripheral->isDone()) {
            // Transfer complete — clean up
            for (auto& ch : mTransmittingChannels) {
                ch->setInUse(false);
            }
            mTransmittingChannels.clear();
            return DriverState::READY;
        }
        return DriverState::BUSY;
    }
    return DriverState::READY;
}

} // namespace fl

#endif // CHANNEL_ENGINE_FLEXIO_CPP_HPP_

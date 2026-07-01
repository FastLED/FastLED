// IWYU pragma: private

/// @file channel_engine_i2s_esp32dev.cpp.hpp
/// @brief Impl of `ChannelEngineI2sEsp32Dev` (#3471).

#include "platforms/esp/32/drivers/i2s/channel_engine_i2s_esp32dev.h"

#include "fl/log/log.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/cstring.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"  // for fl::make_shared
#include "platforms/is_platform.h"
#if defined(FL_IS_ESP32)
#include "platforms/esp/32/feature_flags/enabled.h"
#include "platforms/esp/32/drivers/i2s/i2s_peripheral_esp32dev_esp.h"
#if FASTLED_ESP32_HAS_I2S
// FastLED#3526 Phase 2c — SPI batches delegate to the tested I2S-SPI
// driver (which owns its own peripheral instance via the
// `I2sSpiPeripheralEsp` singleton). Both wrap I2S1 hardware, only one
// binds at a time — same-peripheral-one-slot per the parallel-IO rule.
#include "platforms/esp/32/drivers/i2s_spi/channel_driver_i2s_spi.h"
#endif
#endif

namespace fl {

//=============================================================================
// Ctor / dtor
//=============================================================================

ChannelEngineI2sEsp32Dev::ChannelEngineI2sEsp32Dev(
    fl::shared_ptr<II2sPeripheralEsp32Dev> peripheral) FL_NO_EXCEPT
    : mPeripheral(fl::move(peripheral)),
      mEnqueuedChannels(),
      mInFlightChannels(),
      mScratchBuffer(nullptr),
      mScratchSize(0),
      mState(DriverState::READY),
      mTransmitCompleted(false),
      mPeripheralInitialized(false) {
    if (!mPeripheral) {
        FL_WARN_F("ChannelEngineI2sEsp32Dev: null peripheral injected — inert");
        return;
    }
    // Register our completion callback up front. Real hardware fires
    // it from the DMA-done ISR; the mock fires it from
    // `simulateTransmitDone()` on the test thread.
    (void)mPeripheral->registerTransmitCallback(
        &ChannelEngineI2sEsp32Dev::onTransmitDoneTrampoline, this);
}

ChannelEngineI2sEsp32Dev::~ChannelEngineI2sEsp32Dev() {
    // Drain any in-flight transmit so we don't tear down while the
    // peripheral is still writing into the scratch buffer.
    if (mPeripheral && mPeripheral->isBusy()) {
        (void)mPeripheral->waitTransmitDone(/*timeout_ms=*/1000);
    }

    // Release channels' in-use marks.
    for (auto &data : mInFlightChannels) {
        if (data) {
            data->setInUse(false);
        }
    }
    for (auto &data : mEnqueuedChannels) {
        if (data) {
            data->setInUse(false);
        }
    }

    freeScratchBuffer();

    // Clear our callback registration so a future re-init doesn't
    // hold a dangling `this`.
    if (mPeripheral) {
        (void)mPeripheral->registerTransmitCallback(nullptr, nullptr);
    }
    if (mPeripheral && mPeripheralInitialized) {
        mPeripheral->deinitialize();
        mPeripheralInitialized = false;
    }
}

//=============================================================================
// IChannelDriver
//=============================================================================

bool ChannelEngineI2sEsp32Dev::canHandle(const ChannelDataPtr &data) const
    FL_NO_EXCEPT {
    // Per the parallel-IO rule, this engine handles both clockless
    // and SPI on the classic-ESP32 I2S peripheral. Reject only null
    // data.
    return static_cast<bool>(data);
}

void ChannelEngineI2sEsp32Dev::enqueue(ChannelDataPtr data) FL_NO_EXCEPT {
    if (!data) {
        return;
    }
    mEnqueuedChannels.push_back(data);
}

void ChannelEngineI2sEsp32Dev::show() FL_NO_EXCEPT {
    if (!mPeripheral || mEnqueuedChannels.empty()) {
        // Nothing to do — release any pending in-use marks and stay
        // in READY.
        for (auto &data : mEnqueuedChannels) {
            if (data) {
                data->setInUse(false);
            }
        }
        mEnqueuedChannels.clear();
        mState = DriverState::READY;
        return;
    }

    // Lazy peripheral init on first show(). Config values are the
    // Stage 1 defaults — the Stage 2 encoder will pick these from
    // the per-channel timing.
    if (!mPeripheralInitialized) {
        I2sEsp32DevPeripheralConfig cfg(
            /*port=*/1,
            /*clk=*/2400000, // 2.4 MHz — typical wave8 slot rate
            /*width=*/static_cast<u8>(mEnqueuedChannels.size()));
        if (!mPeripheral->initialize(cfg)) {
            FL_WARN_F("ChannelEngineI2sEsp32Dev: peripheral initialize failed");
            for (auto &data : mEnqueuedChannels) {
                if (data) {
                    data->setInUse(false);
                }
            }
            mEnqueuedChannels.clear();
            mState = DriverState::ERROR;
            return;
        }
        mPeripheralInitialized = true;
    }

    // If a prior transmit is still in flight, wait synchronously for
    // it so we don't stomp its scratch buffer. Callers who care about
    // throughput can pump `poll()` first.
    if (mPeripheral->isBusy()) {
        (void)mPeripheral->waitTransmitDone(/*timeout_ms=*/500);
    }

    // Move the enqueued list into the in-flight slot so future
    // enqueue() calls don't stomp the pointers the peripheral is
    // reading from.
    for (auto &data : mInFlightChannels) {
        if (data) {
            data->setInUse(false);
        }
    }
    mInFlightChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    // FastLED#3526 Phase 2c — per-channel mode dispatch. The parallel-IO
    // unified-engine rule (agents/docs/cpp-standards.md) says the peripheral
    // is the dispatch boundary, not the mode. Since I2S1 can only run one
    // mode at a time (clockless bit-shift vs SPI clocked), we check whether
    // the in-flight batch is homogeneous. Mixed batches (some clockless,
    // some SPI) or SPI batches are rejected until Phase 2c-SPI machinery
    // ships — for now the SPI transmit path is a documented stub. Clockless
    // batches take the existing wave8-encoding path.
    bool has_clockless = false;
    bool has_spi = false;
    for (const auto &data : mInFlightChannels) {
        if (!data) continue;
        if (data->isClockless()) has_clockless = true;
        if (data->isSpi()) has_spi = true;
    }
    if (has_spi && has_clockless) {
        FL_WARN_F("ChannelEngineI2sEsp32Dev: mixed clockless+SPI batches not supported yet — FastLED#3526 Phase 2c stub");
        for (auto &data : mInFlightChannels) {
            if (data) {
                data->setInUse(false);
            }
        }
        mInFlightChannels.clear();
        mState = DriverState::ERROR;
        return;
    }
    if (has_spi) {
        // FastLED#3526 Phase 2c — pure-SPI batch delegates to the
        // tested `ChannelDriverI2sSpi` (`createI2sSpiEngine()`
        // factory). Lazy-init so builds that never route SPI channels
        // through the modern engine don't pay for the delegate. Same
        // peripheral (I2S1) — only one owner at a time, arbitration
        // via the peripheral singleton's `initialize()` first-in-wins.
#if defined(FL_IS_ESP_32DEV) && FASTLED_ESP32_HAS_I2S
        if (!mSpiDelegate) {
            mSpiDelegate = fl::createI2sSpiEngine();
        }
        if (!mSpiDelegate) {
            FL_WARN_F("ChannelEngineI2sEsp32Dev: SPI delegate unavailable");
            for (auto &data : mInFlightChannels) {
                if (data) {
                    data->setInUse(false);
                }
            }
            mInFlightChannels.clear();
            mState = DriverState::ERROR;
            return;
        }
        // Forward every in-flight SPI channel to the delegate. The
        // delegate does its own lane-interleave + peripheral
        // transmit; the modern engine tracks state through its own
        // BUSY→READY dispatch, driven by `poll()` polling the
        // delegate's state.
        for (const auto &data : mInFlightChannels) {
            if (data) {
                mSpiDelegate->enqueue(data);
            }
        }
        mSpiDelegate->show();
        mState = DriverState::BUSY;
        return;
#else
        FL_WARN_F("ChannelEngineI2sEsp32Dev: SPI mode unavailable on this target");
        for (auto &data : mInFlightChannels) {
            if (data) {
                data->setInUse(false);
            }
        }
        mInFlightChannels.clear();
        mState = DriverState::ERROR;
        return;
#endif
    }
    // Clockless batch — take the existing wave8-encoding path below.

    // Pack every channel's encoded bytes into the scratch buffer.
    size_t required = 0;
    for (const auto &data : mInFlightChannels) {
        if (data) {
            required += data->getData().size();
        }
    }
    if (required == 0) {
        for (auto &data : mInFlightChannels) {
            if (data) {
                data->setInUse(false);
            }
        }
        mInFlightChannels.clear();
        mState = DriverState::READY;
        return;
    }
    if (!ensureScratchBuffer(required)) {
        FL_WARN_F("ChannelEngineI2sEsp32Dev: scratch buffer allocation failed");
        for (auto &data : mInFlightChannels) {
            if (data) {
                data->setInUse(false);
            }
        }
        mInFlightChannels.clear();
        mState = DriverState::ERROR;
        return;
    }
    size_t written = packScratchBuffer();

    // Mark the channels in-use for the duration of the transmit so
    // upstream code doesn't rewrite the pixel bytes underneath the
    // peripheral.
    for (auto &data : mInFlightChannels) {
        if (data) {
            data->setInUse(true);
        }
    }

    mTransmitCompleted = false;
    if (!mPeripheral->transmit(mScratchBuffer, written)) {
        FL_WARN_F("ChannelEngineI2sEsp32Dev: transmit() refused");
        for (auto &data : mInFlightChannels) {
            if (data) {
                data->setInUse(false);
            }
        }
        mInFlightChannels.clear();
        mState = DriverState::ERROR;
        return;
    }
    mState = DriverState::BUSY;
}

IChannelDriver::DriverState ChannelEngineI2sEsp32Dev::poll() FL_NO_EXCEPT {
    // FastLED#3526 Phase 2c — when the SPI delegate is active, drive
    // completion off its state machine. The delegate owns the SPI
    // transmit lifecycle; we surface its state (BUSY / DRAINING /
    // READY / ERROR) up to our own callers. Switch statement enforces
    // full enum coverage — if a new DriverState value gets added, the
    // compiler flags the missing arm.
    if (mSpiDelegate && (mState == DriverState::BUSY ||
                         mState == DriverState::DRAINING)) {
        const DriverState delegate_state = mSpiDelegate->poll();
        switch (delegate_state.state) {
        case DriverState::READY:
            // Release channels the delegate already released — the
            // delegate does its own setInUse(false), but be defensive.
            for (auto &data : mInFlightChannels) {
                if (data) {
                    data->setInUse(false);
                }
            }
            mInFlightChannels.clear();
            mTransmitCompleted = false;
            mState = DriverState::READY;
            break;
        case DriverState::DRAINING:
            // Transmit kicked off, DMA still running. Surface DRAINING
            // to our callers so `onEndFrame()` can return without
            // blocking (per the channel-manager DMA wait pattern).
            mState = DriverState::DRAINING;
            break;
        case DriverState::ERROR:
            // Delegate hit an error — release channels + surface ERROR.
            for (auto &data : mInFlightChannels) {
                if (data) {
                    data->setInUse(false);
                }
            }
            mInFlightChannels.clear();
            mTransmitCompleted = false;
            mState = DriverState::ERROR;
            break;
        case DriverState::BUSY:
            // Stay in current state; keep polling.
            break;
        }
        return mState;
    }
    if (!mPeripheral) {
        return DriverState::READY;
    }
    // Check the peripheral. If the completion callback has fired
    // OR the peripheral just reports not-busy (e.g. mock hasn't hit
    // the callback path yet), we're done.
    if (mState == DriverState::BUSY) {
        if (mTransmitCompleted || !mPeripheral->isBusy()) {
            // Release channels and drop to READY.
            for (auto &data : mInFlightChannels) {
                if (data) {
                    data->setInUse(false);
                }
            }
            mInFlightChannels.clear();
            mTransmitCompleted = false;
            mState = DriverState::READY;
        }
    }
    return mState;
}

//=============================================================================
// Buffer management
//=============================================================================

bool ChannelEngineI2sEsp32Dev::ensureScratchBuffer(size_t required)
    FL_NO_EXCEPT {
    if (mScratchBuffer != nullptr && mScratchSize >= required) {
        return true;
    }
    // Free the old buffer before allocating a new one (peripheral's
    // allocator may need contiguous DMA-capable memory).
    freeScratchBuffer();
    if (!mPeripheral) {
        return false;
    }
    mScratchBuffer = mPeripheral->allocateBuffer(required);
    if (mScratchBuffer == nullptr) {
        mScratchSize = 0;
        return false;
    }
    mScratchSize = required;
    return true;
}

size_t ChannelEngineI2sEsp32Dev::packScratchBuffer() FL_NO_EXCEPT {
    // Linear byte concatenation. Wave8 encoding happens inside the
    // peripheral's `transmit()` implementation on real hardware — the
    // engine hands over raw bytes; the peripheral is responsible for
    // encoding them into I2S1's DMA-ready pulse-major layout. This
    // keeps the mock peripheral's capture semantics simple (it sees
    // exactly the bytes each channel produced) and lets host tests
    // remain byte-value-sensitive without depending on wave8 details.
    size_t offset = 0;
    for (const auto &data : mInFlightChannels) {
        if (!data) {
            continue;
        }
        const auto &src = data->getData();
        if (src.empty() || (offset + src.size()) > mScratchSize) {
            continue;
        }
        fl::memcpy(mScratchBuffer + offset, src.data(), src.size());
        offset += src.size();
    }
    return offset;
}

void ChannelEngineI2sEsp32Dev::freeScratchBuffer() FL_NO_EXCEPT {
    if (mScratchBuffer != nullptr && mPeripheral) {
        mPeripheral->freeBuffer(mScratchBuffer);
    }
    mScratchBuffer = nullptr;
    mScratchSize = 0;
}

//=============================================================================
// Completion callback
//=============================================================================

void ChannelEngineI2sEsp32Dev::onTransmitDoneTrampoline(void *user_ctx)
    FL_NO_EXCEPT {
    auto *self = static_cast<ChannelEngineI2sEsp32Dev *>(user_ctx);
    if (self == nullptr) {
        return;
    }
    self->onTransmitDone();
}

void ChannelEngineI2sEsp32Dev::onTransmitDone() FL_NO_EXCEPT {
    // ISR-safe: single volatile-store equivalent. The main `poll()`
    // pump picks up the flag and does the cleanup work outside ISR
    // context.
    mTransmitCompleted = true;
}

//=============================================================================
// Factory — FastLED#3526 Phase 2d
//=============================================================================

fl::shared_ptr<IChannelDriver> createI2sEsp32DevEngine() FL_NO_EXCEPT {
#if defined(FL_IS_ESP_32DEV) && FASTLED_ESP32_HAS_I2S
    auto peripheral = fl::make_shared<I2sPeripheralEsp32DevEsp>();
    return fl::make_shared<ChannelEngineI2sEsp32Dev>(peripheral);
#else
    return nullptr;
#endif
}

} // namespace fl

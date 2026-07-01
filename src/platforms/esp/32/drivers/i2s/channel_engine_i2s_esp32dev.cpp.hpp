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
        // Phase 2c-SPI: peripheral needs to be reconfigured for SPI mode
        // (clock rate + FIFO width + DMA slot) and encoding switches from
        // wave8 pulse-major to raw byte stream. The peripheral surface for
        // SPI mode is still bench-blocked; refuse cleanly rather than
        // silently sending garbage.
        FL_WARN_F("ChannelEngineI2sEsp32Dev: SPI mode not yet wired — FastLED#3526 Phase 2c pending bench validation");
        for (auto &data : mInFlightChannels) {
            if (data) {
                data->setInUse(false);
            }
        }
        mInFlightChannels.clear();
        mState = DriverState::ERROR;
        return;
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
    // Stage 1: linear byte concatenation. Sufficient to exercise the
    // full peripheral + state-machine surface against the mock.
    //
    // Stage 2 replaces this with the parallel bit-transpose + wave8
    // slot expansion so the emitted waveform drives real WS2812
    // timing. The peripheral surface is unchanged across stages.
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

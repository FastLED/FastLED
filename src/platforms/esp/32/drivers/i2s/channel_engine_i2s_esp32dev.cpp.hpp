// IWYU pragma: private

/// @file channel_engine_i2s_esp32dev.cpp.hpp
/// @brief Impl of `ChannelEngineI2sEsp32Dev` (#3471).

#include "platforms/esp/32/drivers/i2s/channel_engine_i2s_esp32dev.h"

#include "fl/log/log.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/cstring.h"
#include "fl/stl/move.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"  // for fl::make_shared, fl::no_op_deleter
#include "fl/stl/singleton.h"
#include "fl/channels/wave8.h"
#include "fl/chipsets/led_timing.h"
#include "platforms/esp/32/drivers/i2s/wave8_encoder_i2s1.h"
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
      mPeripheralInitialized(false),
      mWave8ByteLut(),
      mCachedT1(0),
      mCachedT2(0),
      mCachedT3(0),
      mWave8LutValid(false) {
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

    // If a prior clockless transmit is still in flight, wait for it
    // before we potentially reconfigure the peripheral. Callers who care
    // about throughput can pump `poll()` first.
    if (mPeripheralInitialized && mPeripheral->isBusy()) {
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
    // Clockless batch — lazy-initialize the peripheral now (postponed
    // from the show() prologue so a SPI-only batch doesn't waste
    // an I2S1 claim that the SPI delegate would immediately race with).
    if (!mPeripheralInitialized) {
        // Wave8 pixel clock = 8 MHz (WS2812 800 kHz × 8 pulses/bit).
        // Peripheral picks its own N/A/B divider via `solveI2sClockDivider`.
        I2sEsp32DevPeripheralConfig cfg(
            /*port=*/1,
            /*clk=*/8000000u,
            /*width=*/static_cast<u8>(mInFlightChannels.size()));
        if (!mPeripheral->initialize(cfg)) {
            FL_WARN_F("ChannelEngineI2sEsp32Dev: peripheral initialize failed");
            for (auto &data : mInFlightChannels) {
                if (data) {
                    data->setInUse(false);
                }
            }
            mInFlightChannels.clear();
            mState = DriverState::ERROR;
            return;
        }
        mPeripheralInitialized = true;
    }

    // Route each active channel's GPIO pin to its I2S1 DATA_OUT lane so
    // the wave8-encoded bytes actually leave the SoC on the right wire.
    // Called every show() (idempotent from the peripheral's POV) so pin
    // reassignment mid-run works. Follows the mock's contract: -1 pin
    // is a no-op / detach.
    {
        u8 lane_idx = 0;
        for (const auto &data : mInFlightChannels) {
            if (!data) continue;
            if (lane_idx >= 16) break;
            const int pin = data->getPin();
            (void)mPeripheral->routeLanePin(lane_idx, static_cast<i32>(pin));
            ++lane_idx;
        }
    }

    // Size the scratch buffer: max per-lane byte count across all
    // in-flight channels. Three regions (see packScratchBuffer):
    //   [0, 2W)        — 32-bit DMA samples (4 bytes per pulse), the
    //                    region actually handed to transmit()
    //   [2W, 3W)       — raw wave8 pulse pairs (2 bytes per pulse)
    //   [3W, 3W + 16b) — 16-lane-strided transpose input
    // where W = wave8I2s1EncodedFrameSize(bytes_per_lane).
    size_t bytes_per_lane = 0;
    for (const auto &data : mInFlightChannels) {
        if (data) {
            const size_t sz = data->getData().size();
            if (sz > bytes_per_lane) bytes_per_lane = sz;
        }
    }
    const size_t wave8_size = wave8I2s1EncodedFrameSize(bytes_per_lane);
    const size_t output_size = wave8I2s1Encoded32FrameSize(bytes_per_lane);
    const size_t input_size = 16 * bytes_per_lane;
    const size_t required = output_size + wave8_size + input_size;
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
    // FastLED#3526 Phase 2b/D wire-up: wave8 pulse-major encoding runs
    // HERE (engine-side, matching PARLIO's pattern) so the scratch
    // buffer content is the exact DMA byte stream the peripheral emits
    // on the wire.
    //
    // Two stages (FastLED#3569 root-cause fix):
    //   1. `encodeChannelWave8_i2s1` — shared 16-wide transpose kernel,
    //      2 bytes per pulse period, written to the middle region.
    //   2. `wave8I2s1ExpandTo32Samples` — rewrite each pulse pair into
    //      one 32-bit sample with lanes at bits 8..23, because I2S1 in
    //      `tx_bits_mod = 32` LCD mode clocks one u32 per pixel clock
    //      and presents sample bit (n + 8) on DATA_OUT(n).
    //
    // Chipset timing: for now assumes WS2812B 800kHz across all
    // channels. Per-channel timing plumbing (build LUT from
    // ChannelData::getTiming()) is a follow-up.

    // Find max bytes across active lanes and count active lanes.
    size_t bytes_per_lane = 0;
    size_t num_lanes = 0;
    for (const auto &data : mInFlightChannels) {
        if (!data) continue;
        const size_t sz = data->getData().size();
        if (sz > bytes_per_lane) bytes_per_lane = sz;
        ++num_lanes;
    }
    if (bytes_per_lane == 0 || num_lanes == 0) return 0;
    if (num_lanes > 16) num_lanes = 16;  // wave8 kernel is 16-wide

    // Region layout — see the matching pre-alloc math in show().
    const size_t input_size = 16 * bytes_per_lane;
    const size_t wave8_size = wave8I2s1EncodedFrameSize(bytes_per_lane);
    const size_t output_size = wave8I2s1Encoded32FrameSize(bytes_per_lane);
    if (mScratchSize < output_size + wave8_size + input_size) {
        // Belt-and-suspenders: defense against a stale scratch buffer
        // surviving a show() that used a much smaller frame.
        return 0;
    }
    fl::u8* const output = mScratchBuffer;                             // 32-bit DMA samples
    fl::u8* const wave8_tmp = mScratchBuffer + output_size;           // raw pulse pairs
    fl::u8* const input = mScratchBuffer + output_size + wave8_size;  // lane-strided input

    // Build lane-strided input: lane 0's bytes contiguous, then lane 1,
    // etc. Zero first so inactive-lane slots don't feed uninitialised
    // memory into the 16-wide transpose.
    fl::memset(input, 0, input_size);
    size_t lane_idx = 0;
    for (const auto &data : mInFlightChannels) {
        if (!data) continue;
        if (lane_idx >= 16) break;
        const auto &src = data->getData();
        const size_t copy_len = src.size() > bytes_per_lane ? bytes_per_lane : src.size();
        fl::memcpy(input + lane_idx * bytes_per_lane, src.data(), copy_len);
        ++lane_idx;
    }

    // Build the wave8 LUT for the target chipset timing, cached on the
    // engine instance and filled IN PLACE via the out-param
    // `buildWave8ByteExpansionLUT` overload — the by-value overload
    // materialises a 2 KB temporary on the loop-task stack, which is
    // what tripped the FastLED#3569 stack-canary panic inside the
    // already-deep RPC → show() call chain. Rebuild when the timing
    // changes (exact T1/T2/T3 comparison). Default chipset is fixed
    // WS2812B 800 kHz until per-channel timing dispatch lands.
    const ChipsetTiming timing = to_runtime_timing<TIMING_WS2812_800KHZ>();
    if (!mWave8LutValid || mCachedT1 != timing.T1 || mCachedT2 != timing.T2 ||
        mCachedT3 != timing.T3) {
        const Wave8BitExpansionLut bit_lut = buildWave8ExpansionLUT(timing);  // 64 bytes — stack-safe
        buildWave8ByteExpansionLUT(bit_lut, mWave8ByteLut);
        mCachedT1 = timing.T1;
        mCachedT2 = timing.T2;
        mCachedT3 = timing.T3;
        mWave8LutValid = true;
    }

    if (!encodeChannelWave8_i2s1(
            fl::span<const fl::u8>(input, input_size),
            bytes_per_lane, num_lanes, mWave8ByteLut,
            fl::span<fl::u8>(wave8_tmp, wave8_size))) {
        return 0;
    }
    if (!wave8I2s1ExpandTo32Samples(
            fl::span<const fl::u8>(wave8_tmp, wave8_size),
            fl::span<fl::u8>(output, output_size))) {
        return 0;
    }
    return output_size;
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
    // FastLED#3526 Phase 2e — the peripheral is now a `fl::Singleton<>`
    // (process-lifetime, never destroyed) so the ownership record is
    // simply the singleton's `mInitialized`. Wrap the singleton
    // reference in a `shared_ptr` via `make_shared_no_tracking` — no
    // control block, no delete, zero-overhead non-owning handle.
    auto& singleton = fl::Singleton<I2sPeripheralEsp32DevEsp>::instance();
    fl::shared_ptr<II2sPeripheralEsp32Dev> peripheral =
        fl::make_shared_no_tracking<II2sPeripheralEsp32Dev>(singleton);
    return fl::make_shared<ChannelEngineI2sEsp32Dev>(peripheral);
#else
    return nullptr;
#endif
}

} // namespace fl

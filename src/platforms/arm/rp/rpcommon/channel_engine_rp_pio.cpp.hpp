// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/channel_engine_rp_pio.h"

#include "fl/log/log.h"
#include "fl/stl/utility.h"

namespace fl {

namespace {

// Keep native PL022 SPI preferred whenever the selected pair has a legal
// hardware mux. FLEX_IO owns the remaining arbitrary PIO pin pairs.
bool isNativeSpiPinPair(const SpiChipsetConfig& config) FL_NO_EXCEPT {
    static constexpr u8 kMosi[] = {3, 7, 19, 23, 11, 15, 27};
    static constexpr u8 kSck[] = {2, 6, 18, 22, 10, 14, 26};
    for (size_t index = 0; index < sizeof(kMosi) / sizeof(kMosi[0]); ++index) {
        if (config.dataPin == kMosi[index] && config.clockPin == kSck[index]) {
            return true;
        }
    }
    return false;
}

}  // namespace

ChannelEngineRpPio::ChannelEngineRpPio(
    fl::shared_ptr<IRpPioTxPeripheral> peripheral,
    fl::shared_ptr<IRpPioSpiPeripheral> spi_peripheral,
    const char* driver_name,
    TxPeripheralFactory extra_tx_factory) FL_NO_EXCEPT
    : mSpiPeripheral(fl::move(spi_peripheral)), mTxFactory(fl::move(extra_tx_factory)),
      mDriverName(driver_name != nullptr ? driver_name : "FLEX_IO"),
      mNextJob(0), mSpiActive(false), mTxExhausted(false), mActive(false),
      mFailed(false), mLastStartAttempted(false), mLastStartSucceeded(false),
      mLastWordCount(0) {
    if (peripheral) {
        TxSlot slot;
        slot.peripheral = fl::move(peripheral);
        mTxSlots.push_back(fl::move(slot));
    }
}

ChannelEngineRpPio::~ChannelEngineRpPio() {
    releaseInFlight();
    for (TxSlot& slot : mTxSlots) {
        slot.peripheral->abort();
        slot.peripheral->deinitialize();
    }
    if (mSpiPeripheral) {
        mSpiPeripheral->abort();
        mSpiPeripheral->deinitialize();
    }
}

bool ChannelEngineRpPio::canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT {
    if (!data) return false;
    if (data->isClockless()) {
        if (mTxSlots.empty() || data->getPin() < 0 || data->getPin() > 29) return false;
        const ChipsetTimingConfig& timing = data->getTiming();
        // pio_gen needs at least one cycle in each segment and subtracts two from
        // the low tail instruction. Reject impossible runtime programs early.
        return timing.t1_ns != 0 && timing.t2_ns != 0 && timing.t3_ns != 0 &&
               timing.total_period_ns() >= 250;
    }
    const SpiChipsetConfig* spi = data->getChipset().ptr<SpiChipsetConfig>();
    return mSpiPeripheral && spi != nullptr && spi->dataPin >= 0 && spi->dataPin <= 29 &&
           spi->clockPin >= 0 && spi->clockPin <= 29 && spi->dataPin != spi->clockPin &&
           spi->timing.clock_hz != 0 && !isNativeSpiPinPair(*spi);
}
void ChannelEngineRpPio::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData && canHandle(channelData)) {
        mPendingChannels.push_back(fl::move(channelData));
    }
}

void ChannelEngineRpPio::show() FL_NO_EXCEPT {
    if (mActive || mPendingChannels.empty()) {
        return;
    }
    mInFlightChannels = fl::move(mPendingChannels);
    mPendingChannels.clear();
    mFailed = false;
    mLastStartAttempted = false;
    mLastStartSucceeded = false;
    mLastWordCount = 0;
    mError.clear();
    mLastError.clear();
    for (const ChannelDataPtr& channel : mInFlightChannels) {
        if (channel) channel->setInUse(true);
    }
    buildJobs();
    mActive = true;
    if (!dispatch()) {
        mFailed = true;
        mError = "RP PIO: unable to start queued channel";
    }
}

void ChannelEngineRpPio::buildJobs() FL_NO_EXCEPT {
    mJobs.clear();
    mNextJob = 0;
    mTxExhausted = false;
    size_t index = 0;
    while (index < mInFlightChannels.size()) {
        const ChannelDataPtr& channel = mInFlightChannels[index];
        Job job;
        job.first = index;
        if (channel && channel->isSpi()) {
            job.spi = true;
            mJobs.push_back(job);
            ++index;
            continue;
        }
        // Batch only an adjacent run that is provably safe: same wire timing,
        // XTRA0, byte count, and consecutive pins. This keeps a
        // short/mismatched strip from receiving padding or a different timing
        // waveform; such strips become their own concurrent jobs instead.
        u8 run = 1;
        while (channel && run < 8 && index + run < mInFlightChannels.size()) {
            const ChannelDataPtr& next = mInFlightChannels[index + run];
            if (!next || next->isSpi() || next->getPin() != channel->getPin() + run ||
                next->getTiming() != channel->getTiming() ||
                next->getExtraZeroBitsPerByte() != channel->getExtraZeroBitsPerByte() ||
                next->getData().size() != channel->getData().size()) break;
            ++run;
        }
        job.lanes = run >= 8 ? 8 : run >= 4 ? 4 : run >= 2 ? 2 : 1;
        mJobs.push_back(job);
        index += job.lanes;
    }
}

bool ChannelEngineRpPio::anyActive() const FL_NO_EXCEPT {
    if (mSpiActive) return true;
    for (const TxSlot& slot : mTxSlots) {
        if (slot.active) return true;
    }
    return false;
}

ChannelEngineRpPio::TxSlot* ChannelEngineRpPio::idleTxSlot() FL_NO_EXCEPT {
    for (TxSlot& slot : mTxSlots) {
        if (!slot.active) return &slot;
    }
    if (!mTxFactory || mTxSlots.size() >= kMaxTxSlots) return nullptr;
    fl::shared_ptr<IRpPioTxPeripheral> peripheral = mTxFactory();
    if (!peripheral) return nullptr;
    TxSlot slot;
    slot.peripheral = fl::move(peripheral);
    mTxSlots.push_back(fl::move(slot));
    return &mTxSlots.back();
}

// Start queued jobs, in order, while a free peripheral exists. Returns false
// only when a job cannot start and nothing is running that could free a
// resource for it, i.e. the frame cannot make progress.
bool ChannelEngineRpPio::dispatch() FL_NO_EXCEPT {
    while (mNextJob < mJobs.size()) {
        const Job& job = mJobs[mNextJob];
        const ChannelDataPtr& channel = mInFlightChannels[job.first];
        // Permanent rejections fail the frame now instead of waiting for a
        // resource that will never help (#4620 review).
        if (!canHandle(channel) || channel->getData().empty()) return false;
        // Resources ran out earlier this frame: retry only after a running
        // batch frees its state machine / DMA channel.
        if (mTxExhausted && anyActive()) return true;
        bool started = false;
        if (job.spi) {
            if (mSpiActive) return true;  // wait for the SPI peripheral
            started = startSpi(mNextJob);
        } else {
            TxSlot* slot = idleTxSlot();
            if (slot == nullptr) {
                return anyActive();  // wait for a slot to free up
            }
            started = startClockless(*slot, mNextJob);
        }
        if (!started) {
            if (!anyActive()) return false;
            // Out of PIO/DMA resources while other batches are on the wire
            // (their SMs can starve PIO SPI too): wait and retry when one
            // finishes.
            mTxExhausted = true;
            return true;
        }
        ++mNextJob;
    }
    return true;
}

IChannelDriver::DriverState ChannelEngineRpPio::poll() FL_NO_EXCEPT {
    if (mFailed) return fail(mError.c_str());
    if (!mActive) return DriverState::READY;

    bool busy = false;
    if (mSpiActive) {
        if (!mSpiPeripheral) return fail("RP PIO SPI: missing peripheral");
        if (mSpiPeripheral->hasError()) return fail("RP PIO SPI: peripheral error");
        if (mSpiPeripheral->isDmaBusy()) {
            busy = true;
        } else if (mSpiPeripheral->isTerminalComplete()) {
            mSpiPeripheral->deinitialize();
            mSpiActive = false;
            mTxExhausted = false;  // freed resources: queued jobs may start
        }
    }
    for (TxSlot& slot : mTxSlots) {
        if (!slot.active) continue;
        IRpPioTxPeripheral& peripheral = *slot.peripheral;
        if (peripheral.hasError()) return fail("RP PIO: peripheral error");
        if (peripheral.isDmaBusy()) {
            busy = true;
            continue;
        }
        if (!peripheral.isTerminalComplete()) continue;
        if (!slot.latchPending) {
            slot.latchDurationUs =
                mInFlightChannels[mJobs[slot.job].first]->getTiming().reset_us;
            slot.latchStartUs = peripheral.nowMicros();
            slot.latchPending = true;
        }
        if (static_cast<u32>(peripheral.nowMicros() - slot.latchStartUs) <
            slot.latchDurationUs) {
            continue;
        }
        slot.latchPending = false;
        slot.active = false;
        slot.words.clear();
        peripheral.deinitialize();
        mTxExhausted = false;  // freed resources: queued jobs may start
    }

    const size_t started_before = mNextJob;
    if (!dispatch()) {
        return fail(mJobs[mNextJob].spi ? "RP PIO SPI: unable to start queued channel"
                                        : "RP PIO: unable to start queued channel");
    }
    if (mNextJob != started_before) busy = true;
    if (mNextJob >= mJobs.size() && !anyActive()) {
        releaseInFlight();
        mActive = false;
        return DriverState::READY;
    }
    return busy ? DriverState::BUSY : DriverState::DRAINING;
}

bool ChannelEngineRpPio::startSpi(size_t job_index) FL_NO_EXCEPT {
    const ChannelDataPtr& channel = mInFlightChannels[mJobs[job_index].first];
    if (!canHandle(channel) || !mSpiPeripheral) return false;
    const fl::vector_psram<u8>& input = channel->getData();
    const SpiChipsetConfig& chipset = channel->getChipset().get<SpiChipsetConfig>();
    if (input.empty()) return false;
    RpPioSpiConfig config;
    config.mosi_pin = static_cast<u8>(chipset.dataPin);
    config.sck_pin = static_cast<u8>(chipset.clockPin);
    config.clock_hz = chipset.timing.clock_hz;
    if (!mSpiPeripheral->configure(config)) return false;
    mSpiWords.clear();
    mSpiWords.reserve(input.size());
    for (u8 byte : input) {
        mSpiWords.push_back(static_cast<u32>(byte) << 24);
    }
    mLastStartAttempted = true;
    mLastWordCount = mSpiWords.size();
    mLastStartSucceeded = mSpiPeripheral->startTxDma(mSpiWords.data(), mSpiWords.size());
    mSpiActive = mLastStartSucceeded;
    return mLastStartSucceeded;
}

bool ChannelEngineRpPio::startClockless(TxSlot& slot, size_t job_index) FL_NO_EXCEPT {
    const Job& job = mJobs[job_index];
    const ChannelDataPtr& channel = mInFlightChannels[job.first];
    if (!canHandle(channel)) return false;
    const fl::vector_psram<u8>& input = channel->getData();
    if (input.empty()) return false;
    RpPioTxConfig config;
    config.tx_pin = static_cast<u8>(channel->getPin());
    config.lane_count = job.lanes;
    config.timing = channel->getTiming();
    if (!slot.peripheral->configure(config)) {
        // configure() also rejects unencodable timing or an out-of-range
        // pin/lane layout; resource exhaustion (every PIO block's state
        // machines, instruction memory, or the DMA channels taken, e.g. by a
        // USB stack) is the usual cause for an otherwise valid strip.
        if (!anyActive()) {
            FL_WARN_ONCE("RP PIO: cannot set up pin " << static_cast<u32>(config.tx_pin)
                    << " x" << static_cast<u32>(config.lane_count)
                    << " lanes: invalid pin/timing, or no free PIO state machine, "
                       "program space or DMA channel on any PIO block. Frame "
                       "dropped. FASTLED_RP2040_CLOCKLESS_PIO=0 selects the "
                       "blocking bit-bang controller.");
        }
        return false;
    }

    // Autopull is configured for lane_count bits. Every bit-plane occupies
    // the MSBs of one DMA word, so the PIO emits exactly the requested bytes:
    // no final zero-padding can become a partial extra LED symbol. XTRA0
    // chipsets (GE8822, GW6205) expect that many zero bits after every byte;
    // they are emitted as all-lanes-zero planes.
    const u8 extra_zero_bits = channel->getExtraZeroBitsPerByte();
    fl::vector<u32>& words = slot.words;
    words.clear();
    words.reserve(input.size() * (8u + extra_zero_bits));
    for (size_t byte_index = 0; byte_index < input.size(); ++byte_index) {
        for (int bit = 7; bit >= 0; --bit) {
            u32 plane = 0;
            for (u8 lane = 0; lane < job.lanes; ++lane) {
                const u8 value = mInFlightChannels[job.first + lane]->getData()[byte_index];
                plane |= static_cast<u32>((value >> bit) & 1u)
                         << (job.lanes - 1u - lane);
            }
            words.push_back(plane << (32u - job.lanes));
        }
        for (u8 extra = 0; extra < extra_zero_bits; ++extra) {
            words.push_back(0u);
        }
    }
    mLastStartAttempted = true;
    mLastWordCount = words.size();
    mLastStartSucceeded = slot.peripheral->startTxDma(words.data(), words.size());
    if (!mLastStartSucceeded) {
        slot.peripheral->deinitialize();
        words.clear();
        return false;
    }
    slot.job = job_index;
    slot.active = true;
    slot.latchPending = false;
    return true;
}

void ChannelEngineRpPio::releaseInFlight() FL_NO_EXCEPT {
    for (const ChannelDataPtr& channel : mInFlightChannels) {
        if (channel) channel->setInUse(false);
    }
    mInFlightChannels.clear();
    mJobs.clear();
    mNextJob = 0;
    mSpiWords.clear();
    mSpiActive = false;
    mTxExhausted = false;
    for (TxSlot& slot : mTxSlots) {
        slot.words.clear();
        slot.active = false;
        slot.latchPending = false;
    }
}

IChannelDriver::DriverState ChannelEngineRpPio::fail(const char* message) FL_NO_EXCEPT {
    mLastError = message;
    for (TxSlot& slot : mTxSlots) {
        slot.peripheral->abort();
        slot.peripheral->deinitialize();
    }
    if (mSpiPeripheral) {
        mSpiPeripheral->abort();
        mSpiPeripheral->deinitialize();
    }
    releaseInFlight();
    mActive = false;
    mFailed = false;
    return DriverState(DriverState::ERROR, fl::string(message));
}

}  // namespace fl

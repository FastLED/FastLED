// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/rx_pio_channel.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "platforms/arm/rp/rpcommon/rp_pio_dma_resource_manager.h"
#include "platforms/arm/rp/rpcommon/pio_asm.h"
#include "fl/channels/rx/decode_ws2812.h"
#include "fl/system/delay.h"
#include "fl/stl/cstring.h"
// IWYU pragma: begin_keep
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/structs/dma.h"
// IWYU pragma: end_keep

namespace fl {

namespace {

constexpr size_t kPioRxProgramLength = 3;
// After synchronizing to the first rising edge, the PIO runs one IN PINS
// instruction per cycle. 20 MHz gives 50 ns samples, enough to distinguish
// WS2812 timing phases without losing the first phase to counter setup.
constexpr u32 kPioRxClockHz = 20000000u;
constexpr size_t kPioRxDmaTailWords = 64u;
// AutoResearch's RP tier is capped at 100 RGB LEDs. A static DMA target keeps
// capture independent of the Arduino-Pico heap, which is not usable while the
// full RPC harness owns its transient result objects.
struct PioRxCaptureBuffers {
    u32 words[kRpPioRxEdgeCapacity];
    RpPioRxEdgeStorage edges;
};

PioRxCaptureBuffers gPioRxCaptureBuffers;

const pio_instr kPioRxDurationTemplate[kPioRxProgramLength] = {};

const pio_program kPioRxDurationProgramTemplate = {
    .instructions = kPioRxDurationTemplate,
    .length = kPioRxProgramLength,
    .origin = -1,
#if defined(PICO_SDK_VERSION_MAJOR) && PICO_SDK_VERSION_MAJOR >= 2
    .pio_version = 0,
    #if defined(PICO_PIO_VERSION) && PICO_PIO_VERSION > 0
    .used_gpio_ranges = 0,
    #endif
#endif
};

pio_program makePioRxDurationProgram(const pio_instr* instructions) FL_NO_EXCEPT {
    return {
        .instructions = instructions,
        .length = kPioRxProgramLength,
        .origin = -1,
#if defined(PICO_SDK_VERSION_MAJOR) && PICO_SDK_VERSION_MAJOR >= 2
        .pio_version = 0,
    #if defined(PICO_PIO_VERSION) && PICO_PIO_VERSION > 0
        .used_gpio_ranges = 0,
    #endif
#endif
    };
}

void buildPioRxSampleProgram(pio_instr* instructions, bool idle_low) FL_NO_EXCEPT {
    const uint idle_polarity = idle_low ? PIO_WAIT_POLARITY_0 : PIO_WAIT_POLARITY_1;
    const uint start_polarity = idle_low ? PIO_WAIT_POLARITY_1 : PIO_WAIT_POLARITY_0;
    // Synchronize on a complete idle-to-active transition, then sample each
    // PIO cycle through the wrap at instruction 2.
    instructions[0] = static_cast<pio_instr>(PIO_INSTR_WAIT | idle_polarity |
                                             PIO_WAIT_SRC_PIN | PIO_WAIT_IDX(0));
    instructions[1] = static_cast<pio_instr>(PIO_INSTR_WAIT | start_polarity |
                                             PIO_WAIT_SRC_PIN | PIO_WAIT_IDX(0));
    instructions[2] = static_cast<pio_instr>(PIO_INSTR_IN | PIO_IN_SRC_PINS |
                                             PIO_IN_CNT(1));
}

}  // namespace

fl::shared_ptr<RxDevice> RpPioRxDevice::create(int pin) FL_NO_EXCEPT {
    if (pin < 0 || pin >= RpResourceLedger::kMaxPins) return {};
    return fl::make_shared<RpPioRxDevice>(pin);
}

RpPioRxDevice::RpPioRxDevice(int pin) FL_NO_EXCEPT
    : mPin(pin), mPio(nullptr), mStateMachine(-1), mDmaChannel(-1),
      mProgramOffset(-1), mPioClockHz(0), mTailLimitNs(1), mLastTransitionUs(0),
      mCapacity(0), mArmed(false), mFinished(false), mOverflow(false),
      mProgramLoaded(false), mIdleHigh(false), mSampleHigh(false), mHaveSample(false),
      mLastBeginError(""), mDmaWordCount(0), mDmaWords(nullptr), mEdges(nullptr),
      mDmaWordsProcessed(0), mSampleRunTicks(0) {}

RpPioRxDevice::~RpPioRxDevice() FL_NO_EXCEPT { stop(); }

bool RpPioRxDevice::begin(const RxConfig& config) FL_NO_EXCEPT {
    stop();
    mLastBeginError = "";
    // The Phase 2 sampler configures one direct PIO input mapping. RP2350B's
    // alternate GPIO bank needs PIO-base coordination, which is deliberately
    // deferred until the shared allocator owns that global setting.
    if (config.buffer_size == 0 || mPin < 0 ||
        static_cast<uint>(mPin) >= NUM_BANK0_GPIOS || mPin >= 32) {
        mLastBeginError = "invalid_config";
        return false;
    }
    if (config.buffer_size > kRpPioRxEdgeCapacity) {
        mLastBeginError = "dma_buffer_size";
        return false;
    }
    pio_instr* instructions = reinterpret_cast<pio_instr*>(mProgramInstructions);
    buildPioRxSampleProgram(instructions, config.start_low);
    const pio_program program = makePioRxDurationProgram(instructions);
    auto& resources = RpPioDmaResourceManager::instance();
    PIO pio = nullptr;
    int state_machine = -1;
    int dma_channel = -1;
    int program_offset = -1;
    if (!resources.claimPioStateMachineAndAddProgram(&program, &pio, &state_machine,
                                                      &program_offset)) {
        mLastBeginError = "pio_state_machine";
        return false;
    }
    if (!resources.claimDmaChannel(&dma_channel)) {
        mLastBeginError = "dma_channel";
        resources.removePioProgram(pio, &program, program_offset);
        resources.releasePioStateMachine(pio, state_machine);
        return false;
    }
    if (!resources.claimPins(static_cast<u8>(mPin), 1)) {
        mLastBeginError = "pin_claim";
        if (dma_channel >= 0) resources.releaseDmaChannel(dma_channel);
        resources.removePioProgram(pio, &program, program_offset);
        if (state_machine >= 0) resources.releasePioStateMachine(pio, state_machine);
        return false;
    }
    gpio_init(static_cast<uint>(mPin));
    gpio_set_dir(static_cast<uint>(mPin), GPIO_IN);
    // Clockless LED data is idle-low.  Keep the input at that defined level
    // until the transmitter takes ownership; otherwise a preceding GPIO
    // connectivity probe can leave the jumper floating/high and make the
    // duration sampler treat the entire inter-test gap as its first pulse.
    gpio_pull_down(static_cast<uint>(mPin));
    // The Pico SDK's PIO GPIO initializer selects this PIO block's input
    // path. Keeping the pin's direction input prevents any stale PIO output
    // latch from driving the physical loopback while still routing pad input
    // to WAIT PIN and JMP PIN.
    pio_gpio_init(pio, static_cast<uint>(mPin));
    pio_sm_set_consecutive_pindirs(pio, static_cast<uint>(state_machine),
                                   static_cast<uint>(mPin), 1, false);
    pio_sm_config pio_config = pio_get_default_sm_config();
    sm_config_set_wrap(&pio_config, static_cast<uint>(program_offset + 2),
                       static_cast<uint>(program_offset + 2));
    sm_config_set_in_pins(&pio_config, static_cast<uint>(mPin));
    sm_config_set_in_shift(&pio_config, false, true, 32);
    sm_config_set_fifo_join(&pio_config, PIO_FIFO_JOIN_RX);
    const u32 system_clock_hz = clock_get_hz(clk_sys);
    sm_config_set_clkdiv(&pio_config, static_cast<float>(system_clock_hz) / kPioRxClockHz);
    pio_sm_init(pio, static_cast<uint>(state_machine), static_cast<uint>(program_offset),
                &pio_config);
    mPio = pio;
    mStateMachine = state_machine;
    mDmaChannel = dma_channel;
    mProgramOffset = program_offset;
    mPioClockHz = kPioRxClockHz;
    mTailLimitNs = config.signal_range_max_ns == 0 ? 1 : config.signal_range_max_ns;
    mLastTransitionUs = micros();
    mCapacity = config.buffer_size;
    mCapture.reset(config.signal_range_min_ns, config.skip_signals);
    // Each 32-bit DMA word stores 32 samples. A WS2812 byte uses roughly
    // seven words at 20 MHz; the edge capacity is sixteen phases per byte,
    // so half that capacity plus a reset tail is safely conservative.
    mDmaWordCount = (mCapacity + 1u) / 2u + kPioRxDmaTailWords;
    mDmaWordsProcessed = 0;
    mIdleHigh = !config.start_low;
    mSampleHigh = mIdleHigh;
    mHaveSample = false;
    mSampleRunTicks = 0;
    mArmed = true;
    mFinished = false;
    mOverflow = false;
    mProgramLoaded = true;
    if (mDmaWordCount > kRpPioRxEdgeCapacity) {
        mLastBeginError = "dma_buffer_size";
        stop();
        return false;
    }
    if (!resources.claimPioRxCaptureBuffer(this)) {
        mLastBeginError = "dma_buffer_busy";
        stop();
        return false;
    }
    // DMA needs one stable, contiguous destination. This shared pool is
    // exclusive while a capture is armed; PIO TX and PIO RX use separate
    // resource-manager claims, so a PIO1 TX can run while PIO0 owns it.
    mDmaWords = gPioRxCaptureBuffers.words;
    mEdges = &gPioRxCaptureBuffers.edges;
    mEdges->clear();
    fl::memset(mDmaWords, 0, mDmaWordCount * sizeof(*mDmaWords));
    dma_channel_config dma_config = dma_channel_get_default_config(static_cast<uint>(dma_channel));
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_dreq(&dma_config, pio_get_dreq(pio, static_cast<uint>(state_machine), false));
    dma_channel_configure(static_cast<uint>(dma_channel), &dma_config, mDmaWords,
                          &pio->rxf[state_machine], static_cast<uint>(mDmaWordCount), false);
    pio_sm_set_enabled(pio, static_cast<uint>(state_machine), true);
    dma_start_channel_mask(1u << dma_channel);
    return true;
}

void RpPioRxDevice::stop() FL_NO_EXCEPT {
    if (mDmaChannel >= 0) dma_channel_abort(static_cast<uint>(mDmaChannel));
    if (mPio != nullptr && mStateMachine >= 0) {
        pio_sm_set_enabled(static_cast<PIO>(mPio), static_cast<uint>(mStateMachine), false);
        if (mProgramLoaded && mProgramOffset >= 0) {
            RpPioDmaResourceManager::instance().removePioProgram(
                static_cast<PIO>(mPio), &kPioRxDurationProgramTemplate, mProgramOffset);
        }
        RpPioDmaResourceManager::instance().releasePioStateMachine(static_cast<PIO>(mPio), mStateMachine);
    }
    if (mDmaChannel >= 0) RpPioDmaResourceManager::instance().releaseDmaChannel(mDmaChannel);
    if (mArmed) RpPioDmaResourceManager::instance().releasePins(static_cast<u8>(mPin), 1);
    if (mPin >= 0) gpio_disable_pulls(static_cast<uint>(mPin));
    RpPioDmaResourceManager::instance().releasePioRxCaptureBuffer(this);
    mDmaWords = nullptr;
    mEdges = nullptr;
    mPio = nullptr;
    mStateMachine = -1;
    mDmaChannel = -1;
    mProgramOffset = -1;
    mPioClockHz = 0;
    mArmed = false;
    mProgramLoaded = false;
}

bool RpPioRxDevice::finished() const FL_NO_EXCEPT { return mFinished; }

RxWaitResult RpPioRxDevice::wait(u32 timeout_ms) FL_NO_EXCEPT {
    const u32 started = millis();
    while (!mFinished && millis() - started < timeout_ms) {
        collectDurations();
        finishTailIfIdle();
        if (!mFinished) delay(1);
    }
    if (!mFinished) return RxWaitResult::TIMEOUT;
    return mOverflow ? RxWaitResult::BUFFER_OVERFLOW : RxWaitResult::SUCCESS;
}

fl::result<u32, DecodeError> RpPioRxDevice::decode(const ChipsetTiming4Phase& timing,
                                                    fl::span<u8> out) FL_NO_EXCEPT {
    collectDurations();
    finishTailIfIdle();
    if (mEdges == nullptr) {
        return fl::result<u32, DecodeError>::failure(DecodeError::INVALID_ARGUMENT);
    }
    return fl::channels::rx::decodeWs2812Edges(
        timing, fl::span<const EdgeTime>(mEdges->data(), mEdges->size()), out);
}

size_t RpPioRxDevice::getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset) FL_NO_EXCEPT {
    collectDurations();
    finishTailIfIdle();
    if (mEdges == nullptr || offset >= mEdges->size()) return 0;
    size_t count = mEdges->size() - offset;
    if (count > out.size()) count = out.size();
    for (size_t i = 0; i < count; ++i) out[i] = (*mEdges)[offset + i];
    return count;
}

const char* RpPioRxDevice::name() const FL_NO_EXCEPT { return "PIO_RX"; }
int RpPioRxDevice::getPin() const FL_NO_EXCEPT { return mPin; }

bool RpPioRxDevice::injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT {
    if (!mArmed || mEdges == nullptr) return false;
    for (size_t i = 0; i < edges.size(); ++i) {
        if (mEdges->size() == mCapacity) { mOverflow = true; mFinished = true; return false; }
        mEdges->push_back(edges[i]);
    }
    mFinished = !edges.empty();
    return true;
}

void RpPioRxDevice::collectDurations() FL_NO_EXCEPT {
    if (!mArmed || mPio == nullptr || mStateMachine < 0 || mEdges == nullptr || mFinished) return;
    const size_t transferred = mDmaWordCount - dma_hw->ch[mDmaChannel].transfer_count;
    while (mDmaWordsProcessed < transferred) {
        const u32 samples = mDmaWords[mDmaWordsProcessed];
        for (u32 bit = 0; bit < 32; ++bit) {
            const bool high = (samples & (1u << (31u - bit))) != 0;
            if (!mHaveSample) {
                mSampleHigh = high;
                mHaveSample = true;
                mSampleRunTicks = 1;
                continue;
            }
            if (high == mSampleHigh) {
                ++mSampleRunTicks;
                const u64 run_ns = (mSampleRunTicks * 1000000000ull) / mPioClockHz;
                // The sampler remains active after the transmitter returns.
                // Stop from the sampled reset-low run itself instead of
                // waiting for the CPU polling interval, which would let DMA
                // overwrite a short frame with idle samples.
                if (mSampleHigh == mIdleHigh && run_ns >= mTailLimitNs) {
                    if (!mCapture.appendDuration(mSampleHigh,
                                                 run_ns > 0x7fffffffu ? 0x7fffffffu :
                                                                         static_cast<u32>(run_ns),
                                                 mEdges, mCapacity) || mCapture.saturated()) {
                        mOverflow = true;
                    }
                    pio_sm_set_enabled(static_cast<PIO>(mPio),
                                       static_cast<uint>(mStateMachine), false);
                    dma_channel_abort(static_cast<uint>(mDmaChannel));
                    mFinished = true;
                    return;
                }
                continue;
            }
            const u64 duration_ns = (mSampleRunTicks * 1000000000ull) / mPioClockHz;
            if (!mCapture.appendDuration(mSampleHigh,
                                         duration_ns > 0x7fffffffu ? 0x7fffffffu :
                                                                     static_cast<u32>(duration_ns),
                                         mEdges, mCapacity) || mCapture.saturated()) {
                mOverflow = true;
                mFinished = true;
                dma_channel_abort(static_cast<uint>(mDmaChannel));
                pio_sm_set_enabled(static_cast<PIO>(mPio), static_cast<uint>(mStateMachine), false);
                return;
            }
            mSampleHigh = high;
            mSampleRunTicks = 1;
            mLastTransitionUs = micros();
        }
        ++mDmaWordsProcessed;
    }
    if (mDmaWordsProcessed == mDmaWordCount) {
        mOverflow = true;
        mFinished = true;
        dma_channel_abort(static_cast<uint>(mDmaChannel));
        pio_sm_set_enabled(static_cast<PIO>(mPio), static_cast<uint>(mStateMachine), false);
    }
}

void RpPioRxDevice::finishTailIfIdle() FL_NO_EXCEPT {
    if (!mArmed || mEdges == nullptr || mFinished || !mHaveSample ||
        mSampleHigh != mIdleHigh) return;
    if ((micros() - mLastTransitionUs) * 1000u < mTailLimitNs) return;
    if (!mCapture.appendDuration(mSampleHigh, mTailLimitNs, mEdges, mCapacity) ||
        mCapture.saturated()) {
        mOverflow = true;
    }
    pio_sm_set_enabled(static_cast<PIO>(mPio), static_cast<uint>(mStateMachine), false);
    dma_channel_abort(static_cast<uint>(mDmaChannel));
    mFinished = true;
}

}  // namespace fl

#endif

// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/rx_pio_channel.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "platforms/arm/rp/rpcommon/rp_pio_dma_resource_manager.h"
#include "platforms/arm/rp/rpcommon/pio_asm.h"
#include "fl/channels/rx/decode_ws2812.h"
#include "fl/system/delay.h"
// IWYU pragma: begin_keep
#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/structs/dma.h"
// IWYU pragma: end_keep

namespace fl {

namespace {

constexpr size_t kPioRxProgramLength = 13;
constexpr u32 kPioRxClockHz = 20000000u;

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

void buildPioRxDurationProgram(pio_instr* instructions, bool idle_low) FL_NO_EXCEPT {
    const uint wait_polarity = idle_low ? PIO_WAIT_POLARITY_1 : PIO_WAIT_POLARITY_0;
    instructions[0] = static_cast<pio_instr>(PIO_INSTR_WAIT | wait_polarity |
                                             PIO_WAIT_SRC_PIN | PIO_WAIT_IDX(0));
    instructions[1] = static_cast<pio_instr>(PIO_INSTR_SET | PIO_SET_DST_X | PIO_SET_DATA(0));
    instructions[2] = static_cast<pio_instr>(PIO_INSTR_MOV | PIO_MOV_DST_X |
                                             PIO_MOV_OP_INVERT | PIO_MOV_SRC_X);
    instructions[3] = static_cast<pio_instr>(PIO_INSTR_JMP | PIO_JMP_CND_PIN |
                                             PIO_JMP_ADR(idle_low ? 4 : 5));
    instructions[4] = static_cast<pio_instr>(PIO_INSTR_JMP | PIO_JMP_CND_X_DEC |
                                             PIO_JMP_ADR(3));
    instructions[5] = static_cast<pio_instr>(PIO_INSTR_MOV | PIO_MOV_DST_ISR |
                                             PIO_MOV_SRC_X);
    instructions[6] = static_cast<pio_instr>(PIO_INSTR_PUSH | PIO_PUSH_BLOCK);
    instructions[7] = static_cast<pio_instr>(PIO_INSTR_SET | PIO_SET_DST_X | PIO_SET_DATA(0));
    instructions[8] = static_cast<pio_instr>(PIO_INSTR_MOV | PIO_MOV_DST_X |
                                             PIO_MOV_OP_INVERT | PIO_MOV_SRC_X);
    instructions[9] = static_cast<pio_instr>(PIO_INSTR_JMP | PIO_JMP_CND_PIN |
                                             PIO_JMP_ADR(idle_low ? 11 : 10));
    instructions[10] = static_cast<pio_instr>(PIO_INSTR_JMP | PIO_JMP_CND_X_DEC |
                                              PIO_JMP_ADR(9));
    instructions[11] = static_cast<pio_instr>(PIO_INSTR_MOV | PIO_MOV_DST_ISR |
                                              PIO_MOV_SRC_X);
    instructions[12] = static_cast<pio_instr>(PIO_INSTR_PUSH | PIO_PUSH_BLOCK);
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
      mProgramLoaded(false), mNextHigh(true), mIdleHigh(false), mFirstDuration(true),
      mDmaWordCount(0),
      mDmaWordsProcessed(0) {}

RpPioRxDevice::~RpPioRxDevice() FL_NO_EXCEPT { stop(); }

bool RpPioRxDevice::begin(const RxConfig& config) FL_NO_EXCEPT {
    stop();
    // The Phase 2 sampler configures one direct PIO input mapping. RP2350B's
    // alternate GPIO bank needs PIO-base coordination, which is deliberately
    // deferred until the shared allocator owns that global setting.
    if (config.buffer_size == 0 || mPin < 0 ||
        static_cast<uint>(mPin) >= NUM_BANK0_GPIOS || mPin >= 32) return false;
    auto& resources = RpPioDmaResourceManager::instance();
    PIO pio = nullptr;
    int state_machine = -1;
    int dma_channel = -1;
    while (resources.claimPioStateMachine(&pio, &state_machine)) {
        if (pio_can_add_program(pio, &kPioRxDurationProgramTemplate)) break;
        resources.releasePioStateMachine(pio, state_machine);
        pio = nullptr;
        state_machine = -1;
    }
    if (state_machine < 0 || !resources.claimDmaChannel(&dma_channel) ||
        !resources.claimPins(static_cast<u8>(mPin), 1)) {
        if (dma_channel >= 0) resources.releaseDmaChannel(dma_channel);
        if (state_machine >= 0) resources.releasePioStateMachine(pio, state_machine);
        return false;
    }
    gpio_init(static_cast<uint>(mPin));
    gpio_set_dir(static_cast<uint>(mPin), GPIO_IN);
    pio_gpio_init(pio, static_cast<uint>(mPin));
    pio_sm_set_consecutive_pindirs(pio, static_cast<uint>(state_machine),
                                   static_cast<uint>(mPin), 1, false);
    // Pico SDK does not expose its instruction-memory allocator's chosen
    // offset. Reserve the exact slot with a same-sized template, release it,
    // then install the offset-relocated duration program in that slot.
    const int program_offset = pio_add_program(pio, &kPioRxDurationProgramTemplate);
    if (program_offset < 0) {
        resources.releasePins(static_cast<u8>(mPin), 1);
        resources.releaseDmaChannel(dma_channel);
        resources.releasePioStateMachine(pio, state_machine);
        return false;
    }
    pio_remove_program(pio, &kPioRxDurationProgramTemplate,
                       static_cast<uint>(program_offset));
    pio_instr* instructions = reinterpret_cast<pio_instr*>(mProgramInstructions);
    buildPioRxDurationProgram(instructions, config.start_low);
    const pio_program program = makePioRxDurationProgram(instructions);
    if (pio_add_program_at_offset(pio, &program, static_cast<uint>(program_offset)) < 0) {
        resources.releasePins(static_cast<u8>(mPin), 1);
        resources.releaseDmaChannel(dma_channel);
        resources.releasePioStateMachine(pio, state_machine);
        return false;
    }
    pio_sm_config pio_config = pio_get_default_sm_config();
    sm_config_set_wrap(&pio_config, static_cast<uint>(program_offset + 1),
                       static_cast<uint>(program_offset + kPioRxProgramLength - 1));
    sm_config_set_in_pins(&pio_config, static_cast<uint>(mPin));
    sm_config_set_jmp_pin(&pio_config, static_cast<uint>(mPin));
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
    mEdges.clear();
    mEdges.reserve(mCapacity);
    mCapture.reset(config.signal_range_min_ns, config.skip_signals);
    mDmaWordCount = mCapacity + 1;  // one sentinel makes a full edge buffer honest overflow.
    mDmaWords.resize(mDmaWordCount);
    mDmaWordsProcessed = 0;
    mNextHigh = config.start_low;
    mIdleHigh = !config.start_low;
    mFirstDuration = true;
    mArmed = true;
    mFinished = false;
    mOverflow = false;
    mProgramLoaded = true;
    dma_channel_config dma_config = dma_channel_get_default_config(static_cast<uint>(dma_channel));
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, false);
    channel_config_set_write_increment(&dma_config, true);
    channel_config_set_dreq(&dma_config, pio_get_dreq(pio, static_cast<uint>(state_machine), false));
    dma_channel_configure(static_cast<uint>(dma_channel), &dma_config, mDmaWords.data(),
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
            pio_remove_program(static_cast<PIO>(mPio), &kPioRxDurationProgramTemplate,
                               static_cast<uint>(mProgramOffset));
        }
        RpPioDmaResourceManager::instance().releasePioStateMachine(static_cast<PIO>(mPio), mStateMachine);
    }
    if (mDmaChannel >= 0) RpPioDmaResourceManager::instance().releaseDmaChannel(mDmaChannel);
    if (mArmed) RpPioDmaResourceManager::instance().releasePins(static_cast<u8>(mPin), 1);
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
    return fl::channels::rx::decodeWs2812Edges(timing, fl::span<const EdgeTime>(mEdges), out);
}

size_t RpPioRxDevice::getRawEdgeTimes(fl::span<EdgeTime> out, size_t offset) FL_NO_EXCEPT {
    collectDurations();
    finishTailIfIdle();
    if (offset >= mEdges.size()) return 0;
    size_t count = mEdges.size() - offset;
    if (count > out.size()) count = out.size();
    for (size_t i = 0; i < count; ++i) out[i] = mEdges[offset + i];
    return count;
}

const char* RpPioRxDevice::name() const FL_NO_EXCEPT { return "PIO_RX"; }
int RpPioRxDevice::getPin() const FL_NO_EXCEPT { return mPin; }

bool RpPioRxDevice::injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT {
    if (!mArmed) return false;
    for (size_t i = 0; i < edges.size(); ++i) {
        if (mEdges.size() == mCapacity) { mOverflow = true; mFinished = true; return false; }
        mEdges.push_back(edges[i]);
    }
    mFinished = !edges.empty();
    return true;
}

void RpPioRxDevice::collectDurations() FL_NO_EXCEPT {
    if (!mArmed || mPio == nullptr || mStateMachine < 0 || mFinished) return;
    const size_t transferred = mDmaWordCount - dma_hw->ch[mDmaChannel].transfer_count;
    while (mDmaWordsProcessed < transferred) {
        // The first phase starts after SET+MOV (2 PIO cycles); the opposite
        // phase starts after MOV+PUSH+SET+MOV+JMP (5 cycles). Account for
        // that fixed instruction-path bias before exposing nanoseconds.
        if (mDmaWordsProcessed >= mCapacity ||
            !mCapture.appendCounter(mNextHigh, mDmaWords[mDmaWordsProcessed], mPioClockHz,
                                    mFirstDuration ? 2u : 5u,
                                    &mEdges, mCapacity) || mCapture.saturated()) {
            mOverflow = true;
            mFinished = true;
            dma_channel_abort(static_cast<uint>(mDmaChannel));
            pio_sm_set_enabled(static_cast<PIO>(mPio), static_cast<uint>(mStateMachine), false);
            return;
        }
        mNextHigh = !mNextHigh;
        mFirstDuration = false;
        ++mDmaWordsProcessed;
        mLastTransitionUs = micros();
    }
}

void RpPioRxDevice::finishTailIfIdle() FL_NO_EXCEPT {
    if (!mArmed || mFinished || mNextHigh != mIdleHigh) return;
    if ((micros() - mLastTransitionUs) * 1000u < mTailLimitNs) return;
    if (!mCapture.appendDuration(mNextHigh, mTailLimitNs, &mEdges, mCapacity) ||
        mCapture.saturated()) {
        mOverflow = true;
    }
    pio_sm_set_enabled(static_cast<PIO>(mPio), static_cast<uint>(mStateMachine), false);
    dma_channel_abort(static_cast<uint>(mDmaChannel));
    mFinished = true;
}

}  // namespace fl

#endif

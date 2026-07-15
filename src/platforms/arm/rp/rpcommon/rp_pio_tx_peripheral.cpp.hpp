// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/rp_pio_tx_peripheral.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "fl/stl/chrono.h"
#include "platforms/arm/rp/rpcommon/pio_asm.h"
#include "platforms/arm/rp/rpcommon/rp_pio_dma_resource_manager.h"

// IWYU pragma: begin_keep
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
// IWYU pragma: end_keep

namespace fl {

struct RpPioTxPeripheral::ProgramStorage {
    pio_instr instructions[4];
    pio_program program;
    u32 terminal_pc;

    ProgramStorage() FL_NO_EXCEPT : instructions{0, 0, 0, 0}, program{}, terminal_pc(0) {}
};

namespace {

u32 scaledCycles(u32 ns, u32 clock_hz, float divider) FL_NO_EXCEPT {
    const float cycles = (static_cast<float>(ns) * static_cast<float>(clock_hz)) /
                         1000000000.0f / divider;
    const u32 rounded = static_cast<u32>(cycles + 0.5f);
    return rounded == 0 ? 1 : rounded;
}

}  // namespace

RpPioTxPeripheral::RpPioTxPeripheral(u8 pio_index) FL_NO_EXCEPT
    : mPio(nullptr), mStateMachine(-1), mDmaChannel(-1), mPin(-1),
      mProgramOffset(-1), mLaneCount(1), mPioIndex(pio_index), mProgram(nullptr), mInitialized(false) {}

RpPioTxPeripheral::~RpPioTxPeripheral() { deinitialize(); }

bool RpPioTxPeripheral::createProgram(const ChipsetTimingConfig& timing) FL_NO_EXCEPT {
    const u32 clock_hz = clock_get_hz(clk_sys);
    if (clock_hz == 0) return false;
    const float raw_max = static_cast<float>(timing.t1_ns > timing.t2_ns
                                                  ? (timing.t1_ns > timing.t3_ns ? timing.t1_ns : timing.t3_ns)
                                                  : (timing.t2_ns > timing.t3_ns ? timing.t2_ns : timing.t3_ns)) *
                          static_cast<float>(clock_hz) / 1000000000.0f;
    // Four instructions use a five-bit delay field. The low-tail instruction
    // consumes two cycles outside its delay field, so preserve at least 2.
    const float divider = raw_max > 16.0f ? raw_max / 16.0f : 1.0f;
    const u32 t1 = scaledCycles(timing.t1_ns, clock_hz, divider);
    const u32 t2 = scaledCycles(timing.t2_ns, clock_hz, divider);
    const u32 t3 = scaledCycles(timing.t3_ns, clock_hz, divider);
    if (t1 > 16 || t2 > 16 || t3 < 2 || t3 > 17) return false;

    mProgram = new ProgramStorage(); // owned and freed by deinitialize
    if (mProgram == nullptr) return false;
    mProgram->instructions[0] = static_cast<pio_instr>(
        PIO_INSTR_OUT | PIO_OUT_DST_X | PIO_OUT_CNT(mLaneCount));
    mProgram->instructions[1] = static_cast<pio_instr>(
        PIO_INSTR_SET | PIO_SET_DST_PINS | PIO_SET_DATA(1) | PIO_DELAY(t1 - 1, 0));
    mProgram->instructions[2] = static_cast<pio_instr>(
        PIO_INSTR_MOV | PIO_MOV_DST_PINS | PIO_MOV_SRC_X | PIO_DELAY(t2 - 1, 0));
    mProgram->instructions[3] = static_cast<pio_instr>(
        PIO_INSTR_SET | PIO_SET_DST_PINS | PIO_SET_DATA(0) | PIO_DELAY(t3 - 2, 0));
    mProgram->program.instructions = mProgram->instructions;
    mProgram->program.length = 4;
    mProgram->program.origin = -1;
#if defined(PICO_SDK_VERSION_MAJOR) && PICO_SDK_VERSION_MAJOR >= 2
    mProgram->program.pio_version = 0;
    #if defined(PICO_PIO_VERSION) && PICO_PIO_VERSION > 0
    mProgram->program.used_gpio_ranges = 0;
    #endif
#endif
    PIO pio = static_cast<PIO>(mPio);
    if (!RpPioDmaResourceManager::instance().addPioProgram(
            pio, &mProgram->program, &mProgramOffset)) return false;
    mProgram->terminal_pc = static_cast<u32>(mProgramOffset);

    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_wrap(&config, static_cast<uint>(mProgramOffset),
                       static_cast<uint>(mProgramOffset + 3));
    sm_config_set_set_pins(&config, static_cast<uint>(mPin), mLaneCount);
    sm_config_set_out_pins(&config, static_cast<uint>(mPin), mLaneCount);
    // One DMA word per emitted bit-plane. Autopull after lane_count shifts
    // preserves exact byte boundaries and avoids final zero padding.
    sm_config_set_out_shift(&config, false, true, mLaneCount);
    sm_config_set_clkdiv(&config, divider);
    pio_gpio_init(pio, static_cast<uint>(mPin));
    pio_sm_set_consecutive_pindirs(pio, static_cast<uint>(mStateMachine),
                                   static_cast<uint>(mPin), mLaneCount, true);
    pio_sm_init(pio, static_cast<uint>(mStateMachine),
                static_cast<uint>(mProgramOffset), &config);
    // A PIO output latch survives a prior state-machine use.  The program
    // initially blocks at OUT waiting for its first DMA word, so establish
    // the clockless idle-low level before exposing that blocked state on the
    // GPIO.  This also gives an external RX sampler an unambiguous frame
    // boundary after a GPIO connectivity probe.
    const u32 pin_mask = ((1u << mLaneCount) - 1u) << mPin;
    pio_sm_set_pins_with_mask(pio, static_cast<uint>(mStateMachine), 0, pin_mask);
    pio_sm_set_enabled(pio, static_cast<uint>(mStateMachine), true);
    return true;
}

bool RpPioTxPeripheral::configure(const RpPioTxConfig& config) FL_NO_EXCEPT {
    deinitialize();
    if (config.tx_pin > 29 || (config.lane_count != 1 && config.lane_count != 2 &&
        config.lane_count != 4 && config.lane_count != 8) ||
        static_cast<u16>(config.tx_pin) + config.lane_count > 30) return false;
    PIO pio = nullptr;
    int sm = -1;
    int dma = -1;
    if (!RpPioDmaResourceManager::instance().claimPioDmaAndPin(
            &pio, &sm, &dma, config.tx_pin, config.lane_count, mPioIndex)) {
        return false;
    }
    mPio = pio;
    mStateMachine = sm;
    mDmaChannel = dma;
    mPin = config.tx_pin;
    mLaneCount = config.lane_count;
    if (!createProgram(config.timing)) {
        deinitialize();
        return false;
    }
    dma_channel_config dma_config = dma_channel_get_default_config(static_cast<uint>(mDmaChannel));
    channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_config, true);
    channel_config_set_write_increment(&dma_config, false);
    channel_config_set_dreq(&dma_config, pio_get_dreq(static_cast<PIO>(mPio),
                                                       static_cast<uint>(mStateMachine), true));
    dma_channel_configure(static_cast<uint>(mDmaChannel), &dma_config,
                          &static_cast<PIO>(mPio)->txf[mStateMachine], nullptr,
                          0, false);
    mInitialized = true;
    return true;
}

bool RpPioTxPeripheral::startTxDma(const u32* words, size_t word_count) FL_NO_EXCEPT {
    if (!mInitialized || words == nullptr || word_count == 0 ||
        dma_channel_is_busy(static_cast<uint>(mDmaChannel))) return false;
    dma_channel_set_read_addr(static_cast<uint>(mDmaChannel), words, false);
    dma_channel_set_trans_count(static_cast<uint>(mDmaChannel),
                                static_cast<uint32_t>(word_count), true);
    return true;
}

bool RpPioTxPeripheral::isDmaBusy() const FL_NO_EXCEPT {
    return mDmaChannel >= 0 && dma_channel_is_busy(static_cast<uint>(mDmaChannel));
}

bool RpPioTxPeripheral::isTerminalComplete() const FL_NO_EXCEPT {
    if (!mInitialized || isDmaBusy()) return false;
    const PIO pio = static_cast<PIO>(mPio);
    // At offset 0 the next `out x, 1` is blocked by autopull because FIFO is
    // empty. Reaching it proves the last bit's T3 low tail executed.
    return pio_sm_is_tx_fifo_empty(pio, static_cast<uint>(mStateMachine)) &&
           pio_sm_get_pc(pio, static_cast<uint>(mStateMachine)) ==
               static_cast<uint>(mProgram->terminal_pc);
}

bool RpPioTxPeripheral::hasError() const FL_NO_EXCEPT { return false; }
u32 RpPioTxPeripheral::nowMicros() const FL_NO_EXCEPT { return fl::micros(); }

void RpPioTxPeripheral::abort() FL_NO_EXCEPT {
    if (mDmaChannel >= 0) dma_channel_abort(static_cast<uint>(mDmaChannel));
}

void RpPioTxPeripheral::deinitialize() FL_NO_EXCEPT {
    abort();
    PIO pio = static_cast<PIO>(mPio);
    if (pio != nullptr && mStateMachine >= 0) {
        pio_sm_set_enabled(pio, static_cast<uint>(mStateMachine), false);
        pio_sm_clear_fifos(pio, static_cast<uint>(mStateMachine));
    }
    if (pio != nullptr && mProgram != nullptr && mProgramOffset >= 0) {
        RpPioDmaResourceManager::instance().removePioProgram(
            pio, &mProgram->program, mProgramOffset);
    }
    if (mDmaChannel >= 0) RpPioDmaResourceManager::instance().releaseDmaChannel(mDmaChannel);
    if (pio != nullptr && mStateMachine >= 0) {
        RpPioDmaResourceManager::instance().releasePioStateMachine(pio, mStateMachine);
    }
    if (mPin >= 0) {
        gpio_set_function(static_cast<uint>(mPin), GPIO_FUNC_SIO);
        gpio_set_dir(static_cast<uint>(mPin), GPIO_IN);
        RpPioDmaResourceManager::instance().releasePins(static_cast<u8>(mPin), mLaneCount);
    }
    delete mProgram;
    mPio = nullptr;
    mStateMachine = -1;
    mDmaChannel = -1;
    mPin = -1;
    mProgramOffset = -1;
    mLaneCount = 1;
    mProgram = nullptr;
    mInitialized = false;
}

}  // namespace fl

#endif

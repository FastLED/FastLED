// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/rp_pio_spi_peripheral.h"

#include "platforms/arm/rp/is_rp.h"

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

struct RpPioSpiPeripheral::ProgramStorage {
    pio_instr instructions[3];
    pio_program program;
    u32 terminal_pc;

    ProgramStorage() FL_NO_EXCEPT : instructions{0, 0, 0}, program{}, terminal_pc(0) {}
};

RpPioSpiPeripheral::RpPioSpiPeripheral(u8 pio_index) FL_NO_EXCEPT
    : mPio(nullptr), mStateMachine(-1), mDmaChannel(-1), mMosiPin(-1),
      mSckPin(-1), mProgramOffset(-1), mPioIndex(pio_index),
      mProgram(nullptr), mInitialized(false) {}

RpPioSpiPeripheral::~RpPioSpiPeripheral() { deinitialize(); }

bool RpPioSpiPeripheral::createProgram(u32 clock_hz) FL_NO_EXCEPT {
    const u32 system_hz = clock_get_hz(clk_sys);
    if (system_hz == 0 || clock_hz == 0) return false;

    mProgram = new ProgramStorage(); // ok bare allocation
    if (mProgram == nullptr) return false;
    // Mode-0 SPI: set data while SCK is low, rise for one PIO cycle, then
    // return low. Autopull after eight bits preserves byte boundaries.
    mProgram->instructions[0] = static_cast<pio_instr>(
        PIO_INSTR_OUT | PIO_OUT_DST_PINS | PIO_OUT_CNT(1) | PIO_SIDESET(0, 1));
    mProgram->instructions[1] = static_cast<pio_instr>(PIO_NOP | PIO_SIDESET(1, 1));
    mProgram->instructions[2] = static_cast<pio_instr>(PIO_NOP | PIO_SIDESET(0, 1));
    mProgram->program.instructions = mProgram->instructions;
    mProgram->program.length = 3;
    mProgram->program.origin = -1;
#if defined(PICO_SDK_VERSION_MAJOR) && PICO_SDK_VERSION_MAJOR >= 2
    mProgram->program.pio_version = 0;
    #if defined(PICO_PIO_VERSION) && PICO_PIO_VERSION > 0
    mProgram->program.used_gpio_ranges = 0;
    #endif
#endif

    PIO pio = static_cast<PIO>(mPio);
    if (!RpPioDmaResourceManager::instance().addPioProgram(
            pio, &mProgram->program, &mProgramOffset)) {
        return false;
    }
    mProgram->terminal_pc = static_cast<u32>(mProgramOffset);

    pio_sm_config config = pio_get_default_sm_config();
    sm_config_set_wrap(&config, static_cast<uint>(mProgramOffset),
                       static_cast<uint>(mProgramOffset + 2));
    sm_config_set_sideset(&config, 1, false, false);
    sm_config_set_out_pins(&config, static_cast<uint>(mMosiPin), 1);
    sm_config_set_sideset_pins(&config, static_cast<uint>(mSckPin));
    sm_config_set_out_shift(&config, false, true, 8);
    const float divider = static_cast<float>(system_hz) /
                          (static_cast<float>(clock_hz) * 3.0f);
    if (divider < 1.0f) return false;
    sm_config_set_clkdiv(&config, divider);

    pio_gpio_init(pio, static_cast<uint>(mMosiPin));
    pio_gpio_init(pio, static_cast<uint>(mSckPin));
    pio_sm_set_consecutive_pindirs(pio, static_cast<uint>(mStateMachine),
                                   static_cast<uint>(mMosiPin), 1, true);
    pio_sm_set_consecutive_pindirs(pio, static_cast<uint>(mStateMachine),
                                   static_cast<uint>(mSckPin), 1, true);
    pio_sm_init(pio, static_cast<uint>(mStateMachine),
                static_cast<uint>(mProgramOffset), &config);
    const u32 pins = (1u << mMosiPin) | (1u << mSckPin);
    pio_sm_set_pins_with_mask(pio, static_cast<uint>(mStateMachine), 0, pins);
    pio_sm_set_enabled(pio, static_cast<uint>(mStateMachine), true);
    return true;
}

bool RpPioSpiPeripheral::configure(const RpPioSpiConfig& config) FL_NO_EXCEPT {
    deinitialize();
    if (config.mosi_pin > 29 || config.sck_pin > 29 ||
        config.mosi_pin == config.sck_pin || config.clock_hz == 0) {
        return false;
    }
    PIO pio = nullptr;
    int state_machine = -1;
    int dma_channel = -1;
    if (!RpPioDmaResourceManager::instance().claimPioDmaAndTwoPins(
            &pio, &state_machine, &dma_channel, config.mosi_pin, config.sck_pin,
            mPioIndex)) {
        return false;
    }
    mPio = pio;
    mStateMachine = state_machine;
    mDmaChannel = dma_channel;
    mMosiPin = config.mosi_pin;
    mSckPin = config.sck_pin;
    if (!createProgram(config.clock_hz)) {
        deinitialize();
        return false;
    }

    dma_channel_config dma_config = dma_channel_get_default_config(
        static_cast<uint>(mDmaChannel));
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

bool RpPioSpiPeripheral::startTxDma(const u32* words, size_t word_count) FL_NO_EXCEPT {
    if (!mInitialized || words == nullptr || word_count == 0 ||
        dma_channel_is_busy(static_cast<uint>(mDmaChannel))) {
        return false;
    }
    dma_channel_set_read_addr(static_cast<uint>(mDmaChannel), words, false);
    dma_channel_set_trans_count(static_cast<uint>(mDmaChannel),
                                static_cast<u32>(word_count), true);
    return true;
}

bool RpPioSpiPeripheral::isDmaBusy() const FL_NO_EXCEPT {
    return mDmaChannel >= 0 && dma_channel_is_busy(static_cast<uint>(mDmaChannel));
}

bool RpPioSpiPeripheral::isTerminalComplete() const FL_NO_EXCEPT {
    if (!mInitialized || isDmaBusy()) return false;
    const PIO pio = static_cast<PIO>(mPio);
    return pio_sm_is_tx_fifo_empty(pio, static_cast<uint>(mStateMachine)) &&
           pio_sm_get_pc(pio, static_cast<uint>(mStateMachine)) ==
               static_cast<uint>(mProgram->terminal_pc);
}

bool RpPioSpiPeripheral::hasError() const FL_NO_EXCEPT { return false; }
u32 RpPioSpiPeripheral::nowMicros() const FL_NO_EXCEPT { return fl::micros(); }

void RpPioSpiPeripheral::abort() FL_NO_EXCEPT {
    if (mDmaChannel >= 0) dma_channel_abort(static_cast<uint>(mDmaChannel));
}

void RpPioSpiPeripheral::deinitialize() FL_NO_EXCEPT {
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
    if (mDmaChannel >= 0) {
        RpPioDmaResourceManager::instance().releaseDmaChannel(mDmaChannel);
    }
    if (pio != nullptr && mStateMachine >= 0) {
        RpPioDmaResourceManager::instance().releasePioStateMachine(pio, mStateMachine);
    }
    if (mMosiPin >= 0) {
        gpio_set_function(static_cast<uint>(mMosiPin), GPIO_FUNC_SIO);
        gpio_set_dir(static_cast<uint>(mMosiPin), GPIO_IN);
        RpPioDmaResourceManager::instance().releasePins(mMosiPin, 1);
    }
    if (mSckPin >= 0) {
        gpio_set_function(static_cast<uint>(mSckPin), GPIO_FUNC_SIO);
        gpio_set_dir(static_cast<uint>(mSckPin), GPIO_IN);
        RpPioDmaResourceManager::instance().releasePins(mSckPin, 1);
    }
    delete mProgram; // ok bare allocation
    mPio = nullptr;
    mStateMachine = -1;
    mDmaChannel = -1;
    mMosiPin = -1;
    mSckPin = -1;
    mProgramOffset = -1;
    mProgram = nullptr;
    mInitialized = false;
}

}  // namespace fl

#endif  // defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

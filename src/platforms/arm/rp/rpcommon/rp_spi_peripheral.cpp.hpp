// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/rp_spi_peripheral.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "platforms/arm/rp/rpcommon/rp_pio_dma_resource_manager.h"
#include "fl/stl/chrono.h"

// IWYU pragma: begin_keep
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"
// IWYU pragma: end_keep

namespace fl {

namespace {

spi_inst_t* spiForIndex(int index) FL_NO_EXCEPT {
    return index == 0 ? spi0 : (index == 1 ? spi1 : nullptr);
}

bool validPins(const RpSpiConfig& config) FL_NO_EXCEPT {
    static constexpr u8 kSpi0Mosi[] = {3, 7, 19, 23};
    static constexpr u8 kSpi0Miso[] = {0, 4, 16, 20};
    static constexpr u8 kSpi0Sck[] = {2, 6, 18, 22};
    static constexpr u8 kSpi1Mosi[] = {11, 15, 27};
    static constexpr u8 kSpi1Miso[] = {8, 12, 24};
    static constexpr u8 kSpi1Sck[] = {10, 14, 26};
    const u8* mosi = config.spi_index == 0 ? kSpi0Mosi : kSpi1Mosi;
    const u8* miso = config.spi_index == 0 ? kSpi0Miso : kSpi1Miso;
    const u8* sck = config.spi_index == 0 ? kSpi0Sck : kSpi1Sck;
    const size_t count = config.spi_index == 0 ? 4 : (config.spi_index == 1 ? 3 : 0);
    for (size_t index = 0; index < count; ++index) {
        if (config.mosi_pin == mosi[index] && config.miso_pin == miso[index] &&
            config.sck_pin == sck[index]) return true;
    }
    return false;
}

}  // namespace

RpSpiPeripheral::RpSpiPeripheral() FL_NO_EXCEPT
    : mTxDmaChannel(-1), mRxDmaChannel(-1), mSpiIndex(-1), mMosiPin(-1),
      mMisoPin(-1), mSckPin(-1), mRxSink(0), mActualClockHz(0),
      mInitialized(false), mOwnsSpi(false), mOwnsPins(false) {}

RpSpiPeripheral::~RpSpiPeripheral() { deinitialize(); }

bool RpSpiPeripheral::configure(const RpSpiConfig& config) FL_NO_EXCEPT {
    deinitialize();
    if (!validPins(config) || config.clock_hz == 0 || config.data_bits != 8 ||
        config.cpol || config.cpha || !config.msb_first) return false;
    RpPioDmaResourceManager& resources = RpPioDmaResourceManager::instance();
    if (!resources.claimSpiDmaAndPins(config.spi_index, config.mosi_pin,
                                      config.miso_pin, config.sck_pin,
                                      &mTxDmaChannel, &mRxDmaChannel)) {
        return false;
    }
    mOwnsSpi = true;
    mOwnsPins = true;
    mSpiIndex = config.spi_index;
    mMosiPin = config.mosi_pin;
    mMisoPin = config.miso_pin;
    mSckPin = config.sck_pin;
    spi_inst_t* spi = spiForIndex(mSpiIndex);
    if (spi == nullptr) {
        deinitialize();
        return false;
    }
    gpio_set_function(static_cast<uint>(mMosiPin), GPIO_FUNC_SPI);
    gpio_set_function(static_cast<uint>(mMisoPin), GPIO_FUNC_SPI);
    gpio_set_function(static_cast<uint>(mSckPin), GPIO_FUNC_SPI);
    mActualClockHz = spi_init(spi, config.clock_hz);
    if (mActualClockHz == 0) {
        deinitialize();
        return false;
    }
    // LED chipsets use Motorola SPI mode 0 and wire-order MSB first.
    spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    mActualClockHz = spi_get_baudrate(spi);
    mInitialized = mActualClockHz != 0;
    if (!mInitialized) deinitialize();
    return mInitialized;
}

u32 RpSpiPeripheral::actualClockHz() const FL_NO_EXCEPT { return mActualClockHz; }

bool RpSpiPeripheral::startTxDma(const u8* data, size_t size) FL_NO_EXCEPT {
    if (!mInitialized || data == nullptr || size == 0 || mTxDmaChannel < 0 ||
        mRxDmaChannel < 0 || size > 0xffffffffu) {
        return false;
    }
    spi_inst_t* spi = spiForIndex(mSpiIndex);
    if (spi == nullptr || dma_channel_is_busy(static_cast<uint>(mTxDmaChannel)) ||
        dma_channel_is_busy(static_cast<uint>(mRxDmaChannel))) return false;
    dma_channel_config rx_config = dma_channel_get_default_config(
        static_cast<uint>(mRxDmaChannel));
    channel_config_set_transfer_data_size(&rx_config, DMA_SIZE_8);
    channel_config_set_read_increment(&rx_config, false);
    channel_config_set_write_increment(&rx_config, false);
    channel_config_set_dreq(&rx_config, spi_get_dreq(spi, false));
    dma_channel_configure(static_cast<uint>(mRxDmaChannel), &rx_config,
                          &mRxSink, &spi_get_hw(spi)->dr,
                          static_cast<uint32_t>(size), false);
    dma_channel_config tx_config = dma_channel_get_default_config(
        static_cast<uint>(mTxDmaChannel));
    channel_config_set_transfer_data_size(&tx_config, DMA_SIZE_8);
    channel_config_set_read_increment(&tx_config, true);
    channel_config_set_write_increment(&tx_config, false);
    channel_config_set_dreq(&tx_config, spi_get_dreq(spi, true));
    // Arm RX first: every transmitted byte creates an RX FIFO entry.
    dma_start_channel_mask(1u << static_cast<uint>(mRxDmaChannel));
    dma_channel_configure(static_cast<uint>(mTxDmaChannel), &tx_config,
                          &spi_get_hw(spi)->dr, data,
                          static_cast<uint32_t>(size), true);
    return true;
}

bool RpSpiPeripheral::isTxDmaBusy() const FL_NO_EXCEPT {
    return mTxDmaChannel >= 0 && dma_channel_is_busy(static_cast<uint>(mTxDmaChannel));
}

bool RpSpiPeripheral::isRxDmaBusy() const FL_NO_EXCEPT {
    return mRxDmaChannel >= 0 && dma_channel_is_busy(static_cast<uint>(mRxDmaChannel));
}

bool RpSpiPeripheral::isWireBusy() const FL_NO_EXCEPT {
    spi_inst_t* spi = spiForIndex(mSpiIndex);
    return spi != nullptr && spi_is_busy(spi);
}

bool RpSpiPeripheral::hasError() const FL_NO_EXCEPT { return false; }

u32 RpSpiPeripheral::nowMicros() const FL_NO_EXCEPT { return fl::micros(); }

void RpSpiPeripheral::abort() FL_NO_EXCEPT {
    if (mTxDmaChannel >= 0) dma_channel_abort(static_cast<uint>(mTxDmaChannel));
    if (mRxDmaChannel >= 0) dma_channel_abort(static_cast<uint>(mRxDmaChannel));
}

void RpSpiPeripheral::deinitialize() FL_NO_EXCEPT {
    abort();
    RpPioDmaResourceManager& resources = RpPioDmaResourceManager::instance();
    if (mInitialized) {
        spi_inst_t* spi = spiForIndex(mSpiIndex);
        if (spi != nullptr) spi_deinit(spi);
    }
    if (mTxDmaChannel >= 0) resources.releaseDmaChannel(mTxDmaChannel);
    if (mRxDmaChannel >= 0) resources.releaseDmaChannel(mRxDmaChannel);
    if (mOwnsPins) {
        resources.releasePins(static_cast<u8>(mMosiPin), 1);
        resources.releasePins(static_cast<u8>(mMisoPin), 1);
        resources.releasePins(static_cast<u8>(mSckPin), 1);
    }
    if (mOwnsSpi) resources.releaseSpi(static_cast<u8>(mSpiIndex));
    mTxDmaChannel = -1;
    mRxDmaChannel = -1;
    mSpiIndex = -1;
    mMosiPin = -1;
    mMisoPin = -1;
    mSckPin = -1;
    mRxSink = 0;
    mActualClockHz = 0;
    mInitialized = false;
    mOwnsSpi = false;
    mOwnsPins = false;
}

}  // namespace fl

#endif  // defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

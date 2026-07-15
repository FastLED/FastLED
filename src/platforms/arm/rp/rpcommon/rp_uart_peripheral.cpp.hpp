// IWYU pragma: private

#include "platforms/arm/rp/rpcommon/rp_uart_peripheral.h"

#if defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

#include "platforms/arm/rp/rpcommon/rp_pio_dma_resource_manager.h"
#include "fl/stl/chrono.h"
#include "fl/stl/stdint.h"

// IWYU pragma: begin_keep
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/regs/uart.h"
#include "hardware/structs/uart.h"
#include "hardware/uart.h"
// IWYU pragma: end_keep

namespace fl {

namespace {

uart_inst_t* uartForIndex(int index) FL_NO_EXCEPT {
    return index == 0 ? uart0 : (index == 1 ? uart1 : nullptr);
}

bool pinInList(u8 pin, const u8* pins, size_t count) FL_NO_EXCEPT {
    for (size_t index = 0; index < count; ++index) {
        if (pins[index] == pin) {
            return true;
        }
    }
    return false;
}

}  // namespace

RpUartPeripheral::RpUartPeripheral() FL_NO_EXCEPT
    : mDmaChannel(-1), mUartIndex(-1), mTxPin(-1), mInitialized(false),
      mOwnsUart(false), mOwnsPin(false), mActualBaudRate(0) {}

RpUartPeripheral::~RpUartPeripheral() { deinitialize(); }

bool RpUartPeripheral::isValidTxPin(const RpUartConfig& config) const FL_NO_EXCEPT {
    // RP2040 datasheet table 2.19.2.  These mux assignments are also valid
    // for the RP2350 compatibility UART blocks; do not infer USB CDC pins.
    static constexpr u8 kUart0TxPins[] = {0, 12, 16, 28};
    static constexpr u8 kUart1TxPins[] = {4, 8, 20, 24};
    return config.uart_index == 0
               ? pinInList(config.tx_pin, kUart0TxPins, 4)
               : config.uart_index == 1 && pinInList(config.tx_pin, kUart1TxPins, 4);
}

bool RpUartPeripheral::configure(const RpUartConfig& config) FL_NO_EXCEPT {
    deinitialize();
    if (!isValidTxPin(config) || config.baud_rate == 0 ||
        config.data_bits < 5 || config.data_bits > 8) {
        return false;
    }

    RpPioDmaResourceManager& resources = RpPioDmaResourceManager::instance();
    if (!resources.claimUartDmaAndPin(config.uart_index, config.tx_pin,
                                      &mDmaChannel)) {
        return false;
    }
    mOwnsUart = true;
    mOwnsPin = true;
    mUartIndex = config.uart_index;
    mTxPin = config.tx_pin;

    uart_inst_t* uart = uartForIndex(mUartIndex);
    if (uart == nullptr) {
        deinitialize();
        return false;
    }
    gpio_set_function(static_cast<uint>(mTxPin), UART_FUNCSEL_NUM(uart, mTxPin));
    gpio_set_outover(static_cast<uint>(mTxPin),
                     config.invert_tx ? GPIO_OVERRIDE_INVERT : GPIO_OVERRIDE_NORMAL);
    mActualBaudRate = uart_init(uart, config.baud_rate);
    if (mActualBaudRate == 0) {
        deinitialize();
        return false;
    }
    uart_set_format(uart, config.data_bits, 1, UART_PARITY_NONE);
    uart_set_hw_flow(uart, false, false);
    uart_set_fifo_enabled(uart, true);
    mInitialized = true;
    return true;
}

u32 RpUartPeripheral::actualBaudRate() const FL_NO_EXCEPT {
    return mActualBaudRate;
}

bool RpUartPeripheral::startTxDma(const u8* data, size_t size) FL_NO_EXCEPT {
    if (!mInitialized || data == nullptr || size == 0 || mDmaChannel < 0) {
        return false;
    }
    uart_inst_t* uart = uartForIndex(mUartIndex);
    if (uart == nullptr || dma_channel_is_busy(static_cast<uint>(mDmaChannel))) {
        return false;
    }
    dma_channel_config config = dma_channel_get_default_config(static_cast<uint>(mDmaChannel));
    channel_config_set_transfer_data_size(&config, DMA_SIZE_8);
    channel_config_set_read_increment(&config, true);
    channel_config_set_write_increment(&config, false);
    channel_config_set_dreq(&config, uart_get_dreq_num(uart, true));
    dma_channel_configure(static_cast<uint>(mDmaChannel), &config,
                          &uart_get_hw(uart)->dr, data,
                          static_cast<uint32_t>(size), true);
    return true;
}

bool RpUartPeripheral::isDmaBusy() const FL_NO_EXCEPT {
    return mDmaChannel >= 0 && dma_channel_is_busy(static_cast<uint>(mDmaChannel));
}

bool RpUartPeripheral::isWireBusy() const FL_NO_EXCEPT {
    uart_inst_t* uart = uartForIndex(mUartIndex);
    return uart != nullptr && (uart_get_hw(uart)->fr & UART_UARTFR_BUSY_BITS) != 0;
}

bool RpUartPeripheral::hasError() const FL_NO_EXCEPT { return false; }

u32 RpUartPeripheral::nowMicros() const FL_NO_EXCEPT { return fl::micros(); }

void RpUartPeripheral::abort() FL_NO_EXCEPT {
    if (mDmaChannel >= 0) {
        dma_channel_abort(static_cast<uint>(mDmaChannel));
    }
}

void RpUartPeripheral::deinitialize() FL_NO_EXCEPT {
    abort();
    RpPioDmaResourceManager& resources = RpPioDmaResourceManager::instance();
    if (mInitialized) {
        uart_inst_t* uart = uartForIndex(mUartIndex);
        if (uart != nullptr) {
            uart_deinit(uart);
        }
    }
    if (mDmaChannel >= 0) {
        resources.releaseDmaChannel(mDmaChannel);
    }
    if (mOwnsPin) {
        gpio_set_outover(static_cast<uint>(mTxPin), GPIO_OVERRIDE_NORMAL);
        resources.releasePins(static_cast<u8>(mTxPin), 1);
    }
    if (mOwnsUart) {
        resources.releaseUart(static_cast<u8>(mUartIndex));
    }
    mDmaChannel = -1;
    mUartIndex = -1;
    mTxPin = -1;
    mInitialized = false;
    mOwnsPin = false;
    mOwnsUart = false;
    mActualBaudRate = 0;
}

}  // namespace fl

#endif  // defined(FL_IS_RP2040) || defined(FL_IS_RP2350)

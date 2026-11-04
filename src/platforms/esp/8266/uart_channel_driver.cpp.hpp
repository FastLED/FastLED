// IWYU pragma: private

/// @file platforms/esp/8266/uart_channel_driver.cpp.hpp
/// @brief Implementation of the ESP8266 UART1 channel driver.

#include "platforms/esp/8266/uart_channel_driver.h"
#include "platforms/is_platform.h"

#if defined(FL_IS_ESP8266)

#include "fl/channels/config.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/noexcept.h"
#include "fl/system/delay.h"
#include "platforms/esp/8266/fastled_esp8266_uart.h"

namespace fl {

namespace {

/// UART1 TX is only ever routed to GPIO2 on the ESP8266.
constexpr int kUartTxPin = 2;

/// Four UART bytes encode one LED byte (two LED bits per UART frame).
constexpr fl::u8 kUartBytesPerLedByte = 4;

}  // namespace

Esp8266UartChannelDriver::Esp8266UartChannelDriver() FL_NO_EXCEPT {}
Esp8266UartChannelDriver::~Esp8266UartChannelDriver() = default;

bool Esp8266UartChannelDriver::canHandle(
    const ChannelDataPtr& data) const FL_NO_EXCEPT {
    return static_cast<bool>(data) && data->isClockless() &&
           data->getPin() == kUartTxPin;
}

void Esp8266UartChannelDriver::enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT {
    if (channelData) {
        mEnqueuedChannels.push_back(fl::move(channelData));
    }
}

IChannelDriver::DriverState Esp8266UartChannelDriver::poll() FL_NO_EXCEPT {
    return DriverState(DriverState::READY);
}

fl::string Esp8266UartChannelDriver::getName() const FL_NO_EXCEPT {
    // Matches `busDriverName(fl::Bus::UART)` so an explicit `Bus::UART`
    // channel binds to this driver.
    return fl::string::from_literal("UART");
}

IChannelDriver::Capabilities Esp8266UartChannelDriver::getCapabilities()
    const FL_NO_EXCEPT {
    return Capabilities(/*supportsClockless=*/true, /*supportsSpi=*/false);
}

void Esp8266UartChannelDriver::transmit(
    const ChannelDataPtr& channel) FL_NO_EXCEPT {
    if (!channel) {
        return;
    }

    esp8266_uart::begin(FASTLED_ESP8266_UART_BAUD);

    const fl::vector_psram<fl::u8>& bytes = channel->getData();
    fl::u8 enc[kUartBytesPerLedByte];
    for (fl::size i = 0; i < bytes.size(); ++i) {
        esp8266_uart::encodeByte(bytes[i], enc);
        esp8266_uart::write(enc, kUartBytesPerLedByte);
    }

    esp8266_uart::flush();
    fl::delayMicroseconds(FASTLED_ESP8266_UART_RESET_US);
}

void Esp8266UartChannelDriver::show() FL_NO_EXCEPT {
    if (mEnqueuedChannels.empty()) {
        return;
    }

    mTransmittingChannels = fl::move(mEnqueuedChannels);
    mEnqueuedChannels.clear();

    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        if (mTransmittingChannels[i]) {
            mTransmittingChannels[i]->setInUse(true);
        }
    }

    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        transmit(mTransmittingChannels[i]);
    }

    for (fl::size i = 0; i < mTransmittingChannels.size(); ++i) {
        if (mTransmittingChannels[i]) {
            mTransmittingChannels[i]->setInUse(false);
        }
    }
    mTransmittingChannels.clear();
}

}  // namespace fl

#endif  // FL_IS_ESP8266

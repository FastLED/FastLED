#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/uart_channel_driver.h
/// @brief ESP8266 UART1 channel driver for clockless WS2812-class strips.
///
/// Streams a clockless channel's LED byte stream over UART1 (TX fixed to
/// GPIO2), the "one UART frame carries two LED bits" scheme the legacy
/// `UARTController_ESP8266` used: 6N1 inverted at 3.2 Mbaud, so each wire byte
/// is 8 slots of 312.5 ns. Four UART bytes encode one LED byte.
///
/// Selected by template (`BusTraits<Bus::UART>`, see `uart_bus_traits.h`) and
/// dead-stripped unless named — no `FASTLED_ESP8266_UART` define (FastLED
/// #4944 / #4949).

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

namespace fl {

/// UART1 (GPIO2) WS2812/WS2811 channel driver for ESP8266.
class Esp8266UartChannelDriver : public IChannelDriver {
public:
    Esp8266UartChannelDriver() FL_NO_EXCEPT;
    ~Esp8266UartChannelDriver() override;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;
    fl::string getName() const FL_NO_EXCEPT override;
    Capabilities getCapabilities() const FL_NO_EXCEPT override;

private:
    /// Encode this channel's LED bytes to UART frames and shift them out.
    static void transmit(const ChannelDataPtr& channel) FL_NO_EXCEPT;

    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
};

}  // namespace fl

#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/hspi_channel_driver.h
/// @brief ESP8266 hardware HSPI (SPI1) channel driver for clocked SPI chipsets.
///
/// Streams the already-encoded SPI byte stream (start frame + pixels + end
/// frame, produced by the chipset's SPI encoder) over the hardware HSPI
/// peripheral on the fixed GPIO13 (MOSI) / GPIO14 (SCK) pads. This is the
/// hardware-accelerated counterpart to the shared GPIO bit-bang SPI path
/// (`Bus::BIT_BANG`), reachable through `fl::Bus::SPI`.
///
/// Selected by template (`BusTraits<Bus::SPI>`, see `hspi_bus_traits.h`) and
/// dead-stripped unless something names it — no code-enabling define (FastLED
/// #4942 / #4949). A sketch that drives the HSPI pair with
/// `add<SpiChipsetConfig>()` (or `FastLED.addLeds<APA102, 13, 14>()`) keeps
/// this translation unit; every other sketch drops it.

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

namespace fl {

/// Hardware HSPI output for the ESP8266's fixed SPI LED pin pair (GPIO13/14).
class Esp8266HspiChannelDriver : public IChannelDriver {
public:
    Esp8266HspiChannelDriver() FL_NO_EXCEPT;
    ~Esp8266HspiChannelDriver() override;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;
    fl::string getName() const FL_NO_EXCEPT override;
    Capabilities getCapabilities() const FL_NO_EXCEPT override;

private:
    /// Configure the peripheral for this channel's clock rate and shift its
    /// encoded bytes out. Blocking (the SPI1 engine finishes before return).
    static void transmit(const ChannelDataPtr& channel) FL_NO_EXCEPT;

    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
};

}  // namespace fl

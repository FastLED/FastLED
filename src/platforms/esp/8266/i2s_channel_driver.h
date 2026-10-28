#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/i2s_channel_driver.h
/// @brief ESP8266 I2S output engine for clockless (WS2812-class) strips.
///
/// Drives the ESP8266 I2S peripheral as a bit-pulse generator: 16-bit samples
/// are streamed at the WS2812 slot rate (see `i2s_ws2812_codec.h`), so the CPU
/// is free while the DMA shifts the frame out. Selected through
/// `BusTraits<Bus::FLEX_IO>` (the ESP8266 "I2S" bus, mirroring ESP32's naming)
/// and dead-stripped unless named.
///
/// It links the Arduino core's verified I2S implementation
/// (`core_esp8266_i2s.h` — `i2s_rxtxdrive_begin`, `i2s_set_rate`,
/// `i2s_write_buffer_nb`, `i2s_read_sample`) rather than hand-rolling the I2S
/// registers, which also supplies the **RX** path used to validate the emitted
/// waveform on-device (`validateLoopback`).

#include "fl/channels/data.h"
#include "fl/channels/driver.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"

namespace fl {

/// ESP8266 I2S output engine for clockless strips.
class Esp8266I2sChannelDriver : public IChannelDriver {
public:
    Esp8266I2sChannelDriver() FL_NO_EXCEPT;
    ~Esp8266I2sChannelDriver() override;

    bool canHandle(const ChannelDataPtr& data) const FL_NO_EXCEPT override;
    void enqueue(ChannelDataPtr channelData) FL_NO_EXCEPT override;
    void show() FL_NO_EXCEPT override;
    DriverState poll() FL_NO_EXCEPT override;
    fl::string getName() const FL_NO_EXCEPT override;
    Capabilities getCapabilities() const FL_NO_EXCEPT override;

    /// @brief Encode @p ledCount LED bytes of a WS2812 frame into I2S samples.
    /// Host-testable (no SDK): the same encode the engine streams out.
    /// @param ledBytes LED wire bytes; @param ledCount their count.
    /// @param outSamples destination; @param outCapacity its capacity in samples.
    static void encodeFrame(const fl::u8* ledBytes, fl::size ledCount,
                            fl::u16* outSamples,
                            fl::size outCapacity) FL_NO_EXCEPT;

    /// @brief Decode a captured I2S sample run back into LED bytes (RX side).
    /// Host-testable: the inverse of `encodeFrame`.
    static void decodeFrame(const fl::u16* samples, fl::size sampleCount,
                            fl::u8* outLedBytes,
                            fl::size outCapacity) FL_NO_EXCEPT;

private:
    static void transmit(const ChannelDataPtr& channel) FL_NO_EXCEPT;

    fl::vector<ChannelDataPtr> mEnqueuedChannels;
    fl::vector<ChannelDataPtr> mTransmittingChannels;
};

}  // namespace fl

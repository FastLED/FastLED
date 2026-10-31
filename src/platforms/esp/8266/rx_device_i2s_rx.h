#pragma once

// IWYU pragma: private

/// @file platforms/esp/8266/rx_device_i2s_rx.h
/// @brief ESP8266 hardware RX device: I2S **RX** capture for `fl::RxChannel`.
///
/// ESP8266 had no `fl::RxChannel` backend; the AutoResearch benches hand-rolled
/// a GPIO poll instead (FastLED#4945). This device uses the I2S peripheral's
/// **receive** path — the one hardware receiver the ESP8266 actually has — so RX
/// is asynchronous and does not block the TX it measures. It captures 16-bit
/// samples at the WS2812 slot rate (the same rate the #4943 I2S engine emits),
/// reconstructs the slot-level bit stream, converts it to edge timings, and
/// decodes through the shared `decodeWs2812Edges` path — so the I2S output
/// engine can be validated by looping its output back into I2S RX.
///
/// Unlike the hand-rolled `captureEdges` poll (synchronous, CPU-bound, IRAM,
/// 160 MHz, hardware-TX-only), this is a real async device matching the ESP32
/// backends' shape.

#include "fl/channels/rx.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/vector.h"

namespace fl {

/// I2S-RX-based WS2812 receiver for ESP8266 (`fl::RxChannel`).
class Esp8266I2sRxDevice : public RxDevice {
public:
    static fl::shared_ptr<Esp8266I2sRxDevice> create(int pin) FL_NO_EXCEPT;
    explicit Esp8266I2sRxDevice(int pin) FL_NO_EXCEPT;
    ~Esp8266I2sRxDevice() override = default;

    bool begin(const RxConfig& config) FL_NO_EXCEPT override;
    bool finished() const FL_NO_EXCEPT override;
    RxWaitResult wait(u32 timeout_ms) FL_NO_EXCEPT override;
    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase& timing,
                                       fl::span<u8> out) FL_NO_EXCEPT override;
    size_t getRawEdgeTimes(fl::span<EdgeTime> out,
                           size_t offset = 0) FL_NO_EXCEPT override;
    const char* name() const FL_NO_EXCEPT override;
    int getPin() const FL_NO_EXCEPT override;
    bool injectEdges(fl::span<const EdgeTime> edges) FL_NO_EXCEPT override;

    /// Reconstruct edge timings from captured I2S samples (host-testable).
    /// Each 16-bit sample is four slots (MSB first); runs of equal slots become
    /// `EdgeTime` entries tagged high/low with their ns duration.
    static void samplesToEdges(const fl::u16* samples, fl::size sampleCount,
                               fl::vector<EdgeTime>& out) FL_NO_EXCEPT;

private:
    int mPin;
    bool mArmed;
    bool mFinished;
    RxConfig mConfig;
    fl::vector<EdgeTime> mEdges;
};

}  // namespace fl

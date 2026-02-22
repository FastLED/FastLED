/// @file flexpwm_rx_channel.h
/// @brief Teensy 4.x FlexPWM input-capture RX driver for WS2812-like signals

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "fl/rx_device.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

/// @brief FlexPWM input-capture based RX device for Teensy 4.x
///
/// Uses the i.MXRT1062's FlexPWM dual-edge capture hardware + eDMA to
/// capture WS2812-like self-clocked waveforms with minimal CPU overhead.
///
/// Supported pins (Teensy 4.0 + 4.1): 2, 4, 5, 6, 8, 22, 23, 29
/// Additional pins (Teensy 4.1 only): 36, 49, 53, 54
///
/// Based on the approach from Paul Stoffregen's WS2812Capture library,
/// wrapped in the fl::RxDevice interface.
class FlexPwmRxChannel : public RxDevice {
  public:
    /// Factory method
    /// @param pin GPIO pin number for receiving signals
    /// @return Shared pointer to FlexPwmRxChannel, or nullptr on failure
    static fl::shared_ptr<FlexPwmRxChannel> create(int pin);

    // RxDevice interface
    bool begin(const RxConfig &config) override;
    bool finished() const override;
    RxWaitResult wait(u32 timeout_ms) override;
    fl::Result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                        fl::span<u8> out) override;
    size_t getRawEdgeTimes(fl::span<EdgeTime> out,
                           size_t offset = 0) override;
    const char *name() const override;
    int getPin() const override;
    bool injectEdges(fl::span<const EdgeTime> edges) override;

  protected:
    friend class fl::shared_ptr<FlexPwmRxChannel>;
    FlexPwmRxChannel() = default;
    ~FlexPwmRxChannel() override = default;
};

} // namespace fl

#endif // FL_IS_TEENSY_4X

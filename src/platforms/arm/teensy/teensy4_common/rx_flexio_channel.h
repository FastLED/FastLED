/// @file rx_flexio_channel.h
/// @brief Teensy 4.x FlexIO shifter-based RX driver skeleton for WS2812-like signals
///
/// Phase 1A — skeleton only. Public API surface that unblocks the FastLED#2764
/// Phase 2 RPC harness (`flexioRxBenchmark`) and the Phase 3 ObjectFLED loopback
/// verification. The actual FLEXIO1 register programming + DMAMUX/eDMA wiring
/// lands in the follow-up Phase 1B PR.

#pragma once

// IWYU pragma: private

#include "platforms/arm/teensy/is_teensy.h"

#if defined(FL_IS_TEENSY_4X)

#include "fl/channels/rx.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/noexcept.h"

namespace fl {

/// @brief FlexIO shifter-based RX device for Teensy 4.x
///
/// Targets the iMXRT1062 **FLEXIO1** peripheral. FLEXIO2 is owned by FastLED's
/// existing WS2812 TX driver (`flexio_driver.h`), and FLEXIO3 has fewer mapped
/// GPIO pins on Teensy 4.0, so FLEXIO1 is the chosen RX host.
///
/// Design intent (per FastLED#2764):
/// - 1 shifter configured in RX mode
/// - 1 timer in edge-capture / dual-8-bit-counter mode driving the shifter
/// - eDMA channel + DMAMUX source mirroring the FlexPWM RX driver pattern
/// - Pin restrictions: only Teensy GPIO pins that mux to a FLEXIO1 pin index
class FlexIoRxChannel : public RxDevice {
  public:
    /// Factory method
    /// @param pin GPIO pin number for receiving signals
    /// @return Shared pointer to FlexIoRxChannel, or nullptr if pin has no FLEXIO1 mapping
    static fl::shared_ptr<FlexIoRxChannel> create(int pin) FL_NOEXCEPT;

    // RxDevice interface
    bool begin(const RxConfig &config) FL_NOEXCEPT override;
    bool finished() const FL_NOEXCEPT override;
    RxWaitResult wait(u32 timeout_ms) FL_NOEXCEPT override;
    fl::result<u32, DecodeError> decode(const ChipsetTiming4Phase &timing,
                                        fl::span<u8> out) override FL_NOEXCEPT;
    size_t getRawEdgeTimes(fl::span<EdgeTime> out,
                           size_t offset = 0) override FL_NOEXCEPT;
    const char *name() const override;
    int getPin() const FL_NOEXCEPT override;
    bool injectEdges(fl::span<const EdgeTime> edges) FL_NOEXCEPT override;

  protected:
    friend class fl::shared_ptr<FlexIoRxChannel>;
    FlexIoRxChannel() = default;
    ~FlexIoRxChannel() override = default;
};

} // namespace fl

#endif // FL_IS_TEENSY_4X

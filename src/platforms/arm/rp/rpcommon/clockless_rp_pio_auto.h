#pragma once

// IWYU pragma: private

/// @file clockless_rp_pio_auto.h
/// @brief WS2812 "auto parallel" controller for RP2040/RP2350 (slim-bridge alias)
///
/// `ClocklessController_RP2040_PIO_WS2812<PIN, ORDER>` is now a thin alias
/// over the `ClocklessRpPio` slim bridge (issue #4635). The old
/// RP2040ParallelGroup manager is gone: ChannelEngineRpPio itself batches
/// consecutive same-length/same-timing lanes and runs independent batches
/// concurrently (#4620), so plain `FastLED.addLeds<WS2812, PIN>()` on
/// consecutive GPIOs still gets parallel PIO output.
///
/// Requires FASTLED_RP2040_CLOCKLESS_PIO (the default on RP2040/RP2350).

#include "fl/chipsets/led_timing.h"
#include "platforms/arm/rp/rpcommon/clockless_rp_pio.h"

#if !FASTLED_RP2040_CLOCKLESS_PIO
#error "FASTLED_RP2040_CLOCKLESS_PIO_AUTO requires FASTLED_RP2040_CLOCKLESS_PIO=1 (the PIO channel engine does the parallel grouping)"
#endif

namespace fl {

template <int DATA_PIN, EOrder RGB_ORDER = RGB>
class ClocklessController_RP2040_PIO_WS2812
    : public ClocklessRpPio<DATA_PIN, TIMING_WS2812_800KHZ, RGB_ORDER> {};

} // namespace fl

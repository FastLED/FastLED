/// @file parlio_rx_sampler.h
/// @brief PARLIO-RX 1-bit oversampling capture backend (FastLED#3586).
///
/// ## Why
///
/// ESP32-C6's RMT RX cannot capture PARLIO/RMT clockless TX: the silicon
/// returns the whole frame as one merged HIGH symbol even though the
/// GPIO input on the same pad sees every edge (#3586). Routing that
/// validation to the GPIO-ISR timestamp backend does not help either —
/// its measured capture ceiling is ~2 us between edges (min=2000 ns,
/// under1us=0 across 1137 intervals on a 100-LED frame), while WS2812
/// needs ~300 ns resolution. Both paths are interrupt- or
/// symbol-limited.
///
/// This backend sidesteps both by taking no interrupt per edge: the
/// PARLIO RX unit oversamples ONE data line at a fixed internal sample
/// clock (default 16 MHz -> 62.5 ns resolution) straight into DMA. One
/// pin sample costs one bit, so a 100-LED WS2812 frame (~3 ms) is only
/// ~6 KB of samples.
///
/// The sample stream is run-length-decoded into level/duration runs and
/// then decoded with the same 4-phase pair semantics the RMT, GPIO-ISR
/// and I2S-RX backends use, so byte-exactness matches.
///
/// This is the C6 counterpart to the classic-ESP32 I2S-RX oversampler
/// (#3576 Phase 3) and reuses its CLZ run-scan extraction verbatim.
///
/// ## Hardware notes (ESP32-C6)
///
/// - `SOC_PARLIO_RX_UNITS_PER_GROUP == 1` and the RX unit is separate
///   from the TX unit, so PARLIO clockless TX and this capture backend
///   can run concurrently on the same chip (they only share an
///   interrupt source number).
/// - Sampling uses `PARLIO_CLK_SRC_DEFAULT` with a software ("soft")
///   delimiter, so no external clock or valid-signal pin is required —
///   `clk_in_gpio_num` stays -1.

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#include "fl/channels/rx.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"

namespace fl {

/// @brief Factory. Returns nullptr on targets without a PARLIO RX unit.
fl::shared_ptr<RxDevice> createParlioRxSampler(int pin) FL_NO_EXCEPT;

} // namespace fl

#endif // FL_IS_ESP32

/// @file i2s_rx_sampler.h
/// @brief I2S-RX 1-bit oversampling capture backend for classic ESP32
///        (FastLED#3576 Phase 3).
///
/// ## Why
///
/// Classic ESP32's RMT RX has no ping-pong and no DMA: a receive is
/// one-shot into the channel's on-chip RAM, hard-capping captures at
/// 256 symbols ≈ 10 WS2812B LEDs (#3571). This backend removes that
/// ceiling by a completely different mechanism: the I2S0 peripheral in
/// **standard master-RX serial mode** oversamples the RX pin at a
/// fixed sample clock (default 16 MHz → 62.5 ns resolution) straight
/// into a circular DMA ring in DRAM. One pin sample costs ONE BIT, so
/// a 100-LED WS2812 frame (3 ms) is ~6 KB of samples — the capture
/// length is limited only by RAM, not peripheral FIFO depth.
///
/// The sample stream is run-length-decoded into level/duration edges
/// on the draining task (`wait()`), then decoded with the shared
/// `decodeEdgeTimestamps()` 4-phase decoder (same one the GPIO-ISR
/// backend uses), so byte-exactness semantics match the RMT backend.
///
/// ## Hardware notes (classic ESP32)
///
/// - Serial data-in maps to the `I2S0I_DATA_IN15_IDX` matrix signal
///   (the classic-ESP32 quirk: SD-in rides data_in bit 15).
/// - I2S0 is a shared block: the second clockless TX bank and the
///   clocked-SPI driver also want it. Ownership goes through the
///   cross-driver port-claim registry (`i2s/i2s_port_claim.h`) — when
///   I2S0 is busy transmitting, `begin()` fails cleanly and callers
///   fall back to the RMT RX backend.

#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "platforms/is_platform.h"

#ifdef FL_IS_ESP32

#include "platforms/esp/32/feature_flags/enabled.h"

#if FASTLED_ESP32_HAS_I2S

#include "fl/channels/rx.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/vector.h"

namespace fl {

/// @brief Factory. Returns nullptr on non-classic targets.
fl::shared_ptr<RxDevice> createI2sRxSampler(int pin) FL_NO_EXCEPT;

} // namespace fl

#endif // FASTLED_ESP32_HAS_I2S
#endif // FL_IS_ESP32

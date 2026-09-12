/// @file fl/channels/rx/pio_geometry.h
/// @brief Sampler geometry of the RP PIO RX capture.
///
/// These numbers are the contract between the capture device and anything that
/// has to predict whether a frame will fit in it, and they bound a capture in
/// two independent ways (FastLED#4371):
///
///  - `kRpPioRxEdgeCapacity` is a pool of signal phases, two per bit.
///  - the DMA word count derived from it is a fixed quantity of *wall clock*,
///    because each word holds `kRpPioRxSamplesPerDmaWord` pin samples taken at
///    `kRpPioRxClockHz`.
///
/// They live here, portable and free of the RP build guard, so the device, the
/// bound in `fl/channels/validation.h` and the host tests all read the same
/// definitions instead of copies that can drift apart.

#pragma once

#include "fl/stl/int.h"

namespace fl {

/// Phase slots in the shared capture pool. Sized for the AutoResearch RP tier:
/// 100 RGB LEDs x 3 bytes x 16 phases per byte, plus one.
constexpr size_t kRpPioRxEdgeCapacity = 100u * 3u * 16u + 1u;

/// After synchronizing to the first rising edge the PIO runs one IN PINS
/// instruction per cycle, so 20 MHz gives 50 ns samples -- enough to separate
/// WS2812 timing phases without losing the first phase to counter setup.
constexpr u32 kRpPioRxClockHz = 20000000u;

/// Pin samples packed into each 32-bit DMA word.
constexpr u32 kRpPioRxSamplesPerDmaWord = 32u;

/// Reset-tail words appended to the DMA transfer.
constexpr size_t kRpPioRxDmaTailWords = 64u;

}  // namespace fl

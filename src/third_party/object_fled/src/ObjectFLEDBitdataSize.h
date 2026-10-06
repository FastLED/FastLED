#pragma once

// Host-testable sizing for ObjectFLEDDmaManager's DMA bit buffer (#4711).
// Kept free of Teensy headers so tests/ can exercise it on the host.

#include "fl/stl/stdint.h"

namespace objectfled {

/// Words of bitdata a frame of `numbytes` bytes per strip needs.
/// showInternal() fills min(numbytes, 2*bytesPerDma) bytes, 32 words each.
/// A frame larger than 2*bytesPerDma uses the ESG double buffer, which
/// needs the full 2*bytesPerDma*32 words, so this is also its size; the
/// ISR's half-buffer refill relies on that. numbytes == 0 still gets one
/// byte's worth, because fillbits() is a do/while that writes at least
/// one byte (32 words).
constexpr uint32_t bitdataWordsFor(uint32_t numbytes, uint32_t bytesPerDma) {
    return numbytes == 0                ? 32
           : numbytes < 2 * bytesPerDma ? numbytes * 32
                                        : 2 * bytesPerDma * 32;
}

/// New capacity after seeing a frame of `numbytes`: grows, never shrinks.
constexpr uint32_t grownBitdataWords(uint32_t currentWords, uint32_t numbytes,
                                     uint32_t bytesPerDma) {
    return bitdataWordsFor(numbytes, bytesPerDma) > currentWords
               ? bitdataWordsFor(numbytes, bytesPerDma)
               : currentWords;
}

} // namespace objectfled

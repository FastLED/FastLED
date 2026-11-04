#pragma once

// IWYU pragma: private

#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/span.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace detail {

/// Partition an encoded SPI frame without changing framing or pixel bytes.
/// One staging buffer is refilled only after the previous DMA has completed.
/// Descriptor count scales with frame size and is bounded by the existing
/// native SPI 64-KiB frame limit; staging storage never exceeds 512 bytes.
struct SpiDmaStreamPlan {
    static constexpr size_t kChunkBytes = 512;
    static constexpr size_t kMaxFrameBytes = 65536;

    static size_t chunkCount(size_t bytes) FL_NO_EXCEPT;
    static size_t chunkSize(size_t bytes, size_t index) FL_NO_EXCEPT;
    static bool copyChunk(fl::span<const u8> source, size_t index,
                          fl::span<u8> staging) FL_NO_EXCEPT;
};

} // namespace detail
} // namespace fl

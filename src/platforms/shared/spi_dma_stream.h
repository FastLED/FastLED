#pragma once

// IWYU pragma: private

#include "fl/stl/cstddef.h"
#include "fl/stl/cstring.h"
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

    static size_t chunkCount(size_t bytes) FL_NO_EXCEPT {
        return bytes <= kMaxFrameBytes ? (bytes + kChunkBytes - 1) / kChunkBytes : 0;
    }

    static size_t chunkSize(size_t bytes, size_t index) FL_NO_EXCEPT {
        if (index >= chunkCount(bytes)) return 0;
        const size_t remaining = bytes - index * kChunkBytes;
        return remaining < kChunkBytes ? remaining : kChunkBytes;
    }

    static bool copyChunk(fl::span<const u8> source, size_t index,
                          fl::span<u8> staging) FL_NO_EXCEPT {
        const size_t bytes = chunkSize(source.size(), index);
        if (bytes == 0 || bytes > staging.size()) return false;
        fl::memcpy(staging.data(), source.data() + index * kChunkBytes, bytes);
        return true;
    }
};

} // namespace detail
} // namespace fl

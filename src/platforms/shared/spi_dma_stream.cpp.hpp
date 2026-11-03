#pragma once

// IWYU pragma: private

#include "platforms/shared/spi_dma_stream.h"
#include "fl/stl/cstring.h"

namespace fl {
namespace detail {

size_t SpiDmaStreamPlan::chunkCount(size_t bytes) FL_NO_EXCEPT {
        return bytes <= kMaxFrameBytes ? (bytes + kChunkBytes - 1) / kChunkBytes : 0;
    }

size_t SpiDmaStreamPlan::chunkSize(size_t bytes, size_t index) FL_NO_EXCEPT {
        if (index >= chunkCount(bytes)) return 0;
        const size_t remaining = bytes - index * kChunkBytes;
        return remaining < kChunkBytes ? remaining : kChunkBytes;
    }

bool SpiDmaStreamPlan::copyChunk(fl::span<const u8> source, size_t index,
                          fl::span<u8> staging) FL_NO_EXCEPT {
        const size_t bytes = chunkSize(source.size(), index);
        if (bytes == 0 || bytes > staging.size()) return false;
        fl::memcpy(staging.data(), source.data() + index * kChunkBytes, bytes);
        return true;
    }

} // namespace detail
} // namespace fl

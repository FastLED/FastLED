#pragma once

// IWYU pragma: private

#include "platforms/esp/32/drivers/rmt/rmt_4/rmt_memory_manager_4.h"

namespace fl {

bool RmtMemoryManager4::tryReserveBlocks(int first, u8 count) FL_NO_EXCEPT {
    const size_t blockCount = mWordsPerBlock ? mPoolWords / mWordsPerBlock : 0;
    if (first < 0 || count == 0 || count >= 32 || blockCount > 32 ||
        static_cast<size_t>(first) >= blockCount ||
        count > blockCount - static_cast<size_t>(first)) {
        return false;
    }
    const u32 mask = ((static_cast<u32>(1) << count) - 1) << first;
    if (mReservedBlocks & mask) {
        return false;
    }
    mReservedBlocks |= mask;
    return true;
}

void RmtMemoryManager4::releaseBlocks(int first, u8 count) FL_NO_EXCEPT {
    const size_t blockCount = mWordsPerBlock ? mPoolWords / mWordsPerBlock : 0;
    if (first < 0 || count == 0 || count >= 32 || blockCount > 32 ||
        static_cast<size_t>(first) >= blockCount ||
        count > blockCount - static_cast<size_t>(first)) {
        return;
    }
    const u32 mask = ((static_cast<u32>(1) << count) - 1) << first;
    mReservedBlocks &= ~mask;
}

size_t RmtMemoryManager4::wordsPerBlock() const FL_NO_EXCEPT {
    return mWordsPerBlock;
}

} // namespace fl

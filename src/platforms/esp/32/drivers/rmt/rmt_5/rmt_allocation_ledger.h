#pragma once

// IWYU pragma: private

#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {
namespace detail {

/// Remove one matching RMT allocation from a fixed or dynamic ledger.
/// Returns the removed record so the owner can release its accounting too.
template <typename AllocationContainer, typename Allocation>
bool rollbackLastRmtAllocation(AllocationContainer& allocations,
                               u8 channel_id, bool is_tx,
                               Allocation& extracted) FL_NO_EXCEPT {
    if (allocations.empty()) {
        return false;
    }
    const Allocation& last = allocations.back();
    if (last.channel_id != channel_id || last.is_tx != is_tx) {
        return false;
    }
    extracted = last;
    allocations.pop_back();
    return true;
}

/// Release one matching RMT DMA slot without diagnostics.
template <typename DMAAllocation>
bool releaseRmtDmaAllocation(DMAAllocation& allocation, u8 channel_id,
                             bool is_tx) FL_NO_EXCEPT {
    if (!allocation.allocated || allocation.channel_id != channel_id ||
        allocation.is_tx != is_tx) {
        return false;
    }
    allocation.allocated = false;
    allocation.channel_id = 0;
    allocation.is_tx = false;
    return true;
}

} // namespace detail
} // namespace fl

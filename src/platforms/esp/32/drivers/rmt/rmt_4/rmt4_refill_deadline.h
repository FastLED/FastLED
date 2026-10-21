#pragma once

// IWYU pragma: private

#include "fl/stl/atomic.h"
#include "fl/stl/compiler_control.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/stdint.h"

namespace fl {

// Inline state shared by the SDK caller task and its RMT refill ISR.
class Rmt4RefillDeadline {
public:
    FASTLED_FORCE_INLINE void reset(u32 maximumRefillUs) FL_NO_EXCEPT {
        maxRefillUs = maximumRefillUs;
        lastFillUs.store(0, memory_order_release);
        preloadUs = 0;
    }

    FASTLED_FORCE_INLINE u32 previousFillUs() const FL_NO_EXCEPT {
        return lastFillUs.load(memory_order_acquire);
    }

    // Capture previousFillUs() before reading the hardware clock. A concurrent
    // post-start publication must not appear newer than the sampled clock.
    FASTLED_FORCE_INLINE bool tryRefill(u32 previousUs, u32 nowUs,
                                       bool inIsr) FL_NO_EXCEPT {
        if (!inIsr) { preloadUs = nowUs; }
        if (previousUs != 0 && nowUs - previousUs > maxRefillUs) {
            return false;
        }
        lastFillUs.store(nowUs, memory_order_release);
        return true;
    }

    FASTLED_FORCE_INLINE void recordHardwareStart(u32 startedUs) FL_NO_EXCEPT {
        u32 expected = preloadUs;
        lastFillUs.compare_exchange_strong(expected, startedUs, memory_order_seq_cst);
    }

    u32 maxRefillUs = 0;

private:
    atomic<u32> lastFillUs{0};
    u32 preloadUs = 0; // SDK caller task only.
};

} // namespace fl

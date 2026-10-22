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
    struct Snapshot {
        u32 timeUs;
        bool armed;
    };

    FASTLED_FORCE_INLINE void reset(u32 maximumRefillUs) FL_NO_EXCEPT {
        maxRefillUs = maximumRefillUs;
        lastFillUs.store(0, memory_order_release);
        hardwareClockArmed.store(false, memory_order_release);
        preloadUs = 0;
    }

    FASTLED_FORCE_INLINE Snapshot previousFill() const FL_NO_EXCEPT {
        // The task preload timestamp is not a hardware deadline. Before
        // start publication, the first ISR establishes its own clock baseline.
        const bool armed = hardwareClockArmed.load(memory_order_acquire);
        const u32 timeUs = armed ? lastFillUs.load(memory_order_acquire) : 0;
        return {timeUs, armed};
    }

    // Capture previousFill() before reading the hardware clock. A concurrent
    // post-start publication must not appear newer than the sampled clock.
    FASTLED_FORCE_INLINE bool tryRefill(Snapshot previous, u32 nowUs,
                                       bool inIsr) FL_NO_EXCEPT {
        if (!inIsr) { preloadUs = nowUs; }
        if (previous.armed && nowUs - previous.timeUs > maxRefillUs) {
            return false;
        }
        lastFillUs.store(nowUs, memory_order_release);
        if (inIsr) { hardwareClockArmed.store(true, memory_order_release); }
        return true;
    }

    FASTLED_FORCE_INLINE void recordHardwareStart(u32 startedUs) FL_NO_EXCEPT {
        u32 expected = preloadUs;
        if (lastFillUs.compare_exchange_strong(expected, startedUs, memory_order_seq_cst)) {
            hardwareClockArmed.store(true, memory_order_release);
        }
    }

    u32 maxRefillUs = 0;

private:
    atomic<u32> lastFillUs{0};
    atomic_bool hardwareClockArmed{false};
    u32 preloadUs = 0; // SDK caller task only.
};

} // namespace fl

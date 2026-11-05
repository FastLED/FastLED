#pragma once

// IWYU pragma: private

#include "fl/stl/compiler_control.h"
#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"
#include "fl/system/delay.h"

namespace fl {

/// One atomic GPIO update, without a data-dependent branch. Platforms with
/// write-one-to-set/clear registers can use this instead of a port snapshot.
template <u8 BIT>
FASTLED_FORCE_INLINE void writeBitSetClear(u8 value, u32 mask,
                                         volatile u32* set,
                                         volatile u32* clear) FL_NO_EXCEPT {
    volatile u32* target = (value & (1U << BIT)) ? set : clear;
    *target = mask;
}

/// Default pin access; a platform may specialize the data-write operation
/// while the generic SPI writer continues to own bit order and clock timing.
template <class Pin> struct BitbangPin {
    static constexpr bool kAtomicWrite = false;
    template <fl::cycle_t CYCLES>
    FASTLED_FORCE_INLINE static void delay() FL_NO_EXCEPT {
        fl::delaycycles<CYCLES>();
    }
    template <u8 BIT>
    FASTLED_FORCE_INLINE static void write(u8 value) FL_NO_EXCEPT {
        if (value & (1U << BIT)) Pin::hi();
        else Pin::lo();
    }
};

} // namespace fl

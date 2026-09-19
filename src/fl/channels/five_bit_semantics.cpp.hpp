// ok no header - implementation for fl/channels/five_bit_semantics.h

#include "fl/channels/five_bit_semantics.h"
#include "fl/stl/singleton.h"

namespace fl {

FiveBitSemantics fiveBitSemanticsFor(SpiChipset chip,
                                     FiveBitSemantics profile) FL_NO_EXCEPT {
    FiveBitSemantics chip_default = FiveBitSemantics::NotApplicable;
    switch (chip) {
        case SpiChipset::APA102HD:
        case SpiChipset::DOTSTARHD:
            chip_default = FiveBitSemantics::SecondarySlowPwm;
            break;
        case SpiChipset::SK9822HD:
            chip_default = FiveBitSemantics::CurrentGain;
            break;
        case SpiChipset::HD107HD:
            // Protocol-compatible with APA102, but what its field does to
            // light is not documented: B1's conservative default.
            chip_default = FiveBitSemantics::Unknown;
            break;
        default:
            return FiveBitSemantics::NotApplicable;
    }
    return profile != FiveBitSemantics::NotApplicable ? profile : chip_default;
}

u8 hdMinimumField(FiveBitSemantics semantics, u8 floor_setting) FL_NO_EXCEPT {
    if (semantics != FiveBitSemantics::SecondarySlowPwm) {
        return 31;
    }
    if (floor_setting < 1) {
        return 1;
    }
    return floor_setting > 31 ? 31 : floor_setting;
}

namespace detail {
namespace {
struct HdFieldFloorStorage {
    u8 value = 31;
};
}  // namespace

u8& hdFieldFloor() FL_NO_EXCEPT {
    return Singleton<HdFieldFloorStorage>::instance().value;
}
}  // namespace detail

}  // namespace fl

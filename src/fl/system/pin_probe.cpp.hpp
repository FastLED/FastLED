/// @file fl/system/pin_probe.cpp.hpp
/// Compilation boundary for fl/system/pin_probe.h: the platform table is
/// pulled in here only.

#include "fl/system/pin_probe.h"
#include "platforms/pin_probe.h"

namespace fl {

const char* pinProbeSkipReason(int pin) FL_NO_EXCEPT {
    return platforms::pinProbeSkipReason(pin);
}

const char* pinProbeDriveSkipReason(int pin) FL_NO_EXCEPT {
    return platforms::pinProbeDriveSkipReason(pin);
}

bool isPinUnsafeToProbe(int pin) FL_NO_EXCEPT {
    return pinProbeSkipReason(pin) != nullptr;
}

u64 pinUnsafeForProbeMask() FL_NO_EXCEPT {
    u64 mask = 0;
    for (int p = 0; p < 64; ++p) {
        if (isPinUnsafeToProbe(p)) {
            mask |= pinMaskBit(p);
        }
    }
    return mask;
}

}  // namespace fl

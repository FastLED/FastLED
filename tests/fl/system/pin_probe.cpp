// ok cpp include
/// @file pin_probe.cpp
/// @brief Host (stub) behavior of the pin-probe safety API.

#include "test.h"
#include "fl/system/pin_probe.h"

FL_TEST_CASE("pinMaskBit - in range and out of range") {
    FL_CHECK(fl::pinMaskBit(0) == fl::u64(1));
    FL_CHECK(fl::pinMaskBit(63) == (fl::u64(1) << 63));
    FL_CHECK(fl::pinMaskBit(-1) == fl::u64(0));
    FL_CHECK(fl::pinMaskBit(64) == fl::u64(0));
}

FL_TEST_CASE("pin probe - host has no unsafe or undrivable pins") {
    FL_CHECK(fl::pinUnsafeForProbeMask() == fl::u64(0));
    for (int p = -1; p <= 64; ++p) {
        FL_CHECK(fl::pinProbeSkipReason(p) == nullptr);
        FL_CHECK(fl::pinProbeDriveSkipReason(p) == nullptr);
        FL_CHECK_FALSE(fl::isPinUnsafeToProbe(p));
    }
}

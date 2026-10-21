#pragma once

// IWYU pragma: private

/// @file platforms/shared/pin_probe_null.hpp
/// Default pin-probe table: no pin is unsafe, every pin is drivable.

#include "fl/stl/noexcept.h"

namespace fl {
namespace platforms {

inline const char* pinProbeSkipReason(int /*pin*/) FL_NO_EXCEPT { return nullptr; }
inline const char* pinProbeDriveSkipReason(int /*pin*/) FL_NO_EXCEPT { return nullptr; }

}  // namespace platforms
}  // namespace fl

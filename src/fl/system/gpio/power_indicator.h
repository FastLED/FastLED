#pragma once

// IWYU pragma: private

#include "fl/stl/int.h"

namespace fl {
namespace detail {
extern fl::u8 powerIndicatorPin;
extern void (*powerIndicatorWrite)(fl::u8 pin, bool overLimit);
} // namespace detail
} // namespace fl

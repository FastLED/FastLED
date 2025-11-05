#pragma once

#include "fl/int.h"

namespace fl {

/// All lanes mask (template parameter for bulk controllers)
/// Used to indicate that all available lanes should be used
constexpr fl::u32 ALL_LANES_MASK = 0xFFFFFFFF;

/// Max GPIO pin number
/// Upper bound for valid GPIO pin numbers
constexpr int MAX_GPIO_PIN = 256;

} // namespace fl

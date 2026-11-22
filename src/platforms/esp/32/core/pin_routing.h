#pragma once

// IWYU pragma: private

#include "fl/stl/int.h"
#include "fl/stl/noexcept.h"

namespace fl {
namespace detail {

// Output signal, output enable and IOMUX function, matching the routing
// snapshots used by RMT and SPI. Returns 0xFFFFFFFF when unsupported/invalid.
u32 readPinRouting(int pin) FL_NO_EXCEPT;

} // namespace detail
} // namespace fl

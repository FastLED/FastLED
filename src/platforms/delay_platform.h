#pragma once

#include "fl/stl/stdint.h"

namespace fl {
namespace platform {

/// Raw platform delay without async task pumping
/// @param ms Milliseconds to delay
void delay(fl::u32 ms);

}  // namespace platform
}  // namespace fl

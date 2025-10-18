#pragma once

/// ARM Cortex-M interrupt control interface
/// Minimal bindings using PRIMASK register

#include "fl/namespace.h"

namespace fl {

/// Disable interrupts on ARM Cortex-M
void noInterrupts();

/// Enable interrupts on ARM Cortex-M
void interrupts();

}  // namespace fl

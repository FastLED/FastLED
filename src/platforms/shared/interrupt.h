#pragma once

/// Shared/generic interrupt control interface for desktop platforms
/// Desktop platforms don't have hardware interrupts - no-op implementation

#include "fl/namespace.h"

namespace fl {

/// No-op for generic/desktop platforms
void noInterrupts();

/// No-op for generic/desktop platforms
void interrupts();

}  // namespace fl

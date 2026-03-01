#pragma once

// IWYU pragma: no_include "platforms/arm/is_arm.h"

// Trampoline header - forwards to platform-specific placement new implementation
// This header must be included BEFORE namespace fl opens to properly define the global operator new
#include "platforms/new.h"  // IWYU pragma: export

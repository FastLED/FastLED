#pragma once

#include "platforms/trig8.h"

/// @file trig8.h
/// Backward compatibility header for trigonometry functions.
/// This header includes platforms/trig8.h which provides all trigonometry functions.
/// New code should use platforms/trig8.h directly.

// Bring trig8 functions into global namespace for backward compatibility
using fl::sin16;
using fl::cos16;
using fl::sin8;
using fl::cos8;

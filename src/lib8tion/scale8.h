#pragma once

#include "fl/scale8.h"

/// @file scale8.h
/// Backward compatibility header for 8-bit scaling functions.
/// This header provides using declarations for backward compatibility
/// with code that uses the lib8tion namespace.
/// New code should use fl/scale8.h directly.

/// @addtogroup lib8tion
/// @{

/// @defgroup Scaling Scaling Functions
/// @see fl::scale8.h for the canonical scaling function implementations
/// @{

// Bring scale functions into global scope for backward compatibility
using fl::scale8;
using fl::scale8_video;
using fl::scale8_LEAVING_R1_DIRTY;
using fl::nscale8_LEAVING_R1_DIRTY;
using fl::scale8_video_LEAVING_R1_DIRTY;
using fl::nscale8_video_LEAVING_R1_DIRTY;
using fl::cleanup_R1;
using fl::scale16by8;
using fl::scale16;
using fl::nscale8x3_constexpr;

/// @} Scaling
/// @} lib8tion
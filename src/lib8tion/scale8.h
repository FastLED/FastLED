#pragma once

#include "crgb.h"
#include "fastled_config.h"
#include "lib8tion/lib8static.h"
#include "fl/compiler_control.h"

#include "platforms/scale8.h"

/// @file scale8.h
/// Backward compatibility header for 8-bit scaling functions.
/// This header includes platforms/scale8.h which provides all scaling functions.
/// New code should use platforms/scale8.h directly.

// Bring scale8 functions into global namespace for backward compatibility
using fl::scale8;
using fl::scale8_video;
using fl::scale8_LEAVING_R1_DIRTY;
using fl::nscale8_LEAVING_R1_DIRTY;
using fl::scale8_video_LEAVING_R1_DIRTY;
using fl::nscale8_video_LEAVING_R1_DIRTY;
using fl::cleanup_R1;
using fl::scale16by8;
using fl::scale16;
using fl::nscale8x3;
using fl::nscale8x3_video;
using fl::nscale8x2;
using fl::nscale8x2_video;
using fl::nscale8x3_constexpr;
using fl::dim8_raw;
using fl::dim8_video;
using fl::dim8_lin;
using fl::brighten8_raw;
using fl::brighten8_video;
using fl::brighten8_lin;
#pragma once

/// @file fl/stl/scratch_config.h
/// @brief Build-wide fl::scratch configuration (FL_SCRATCH_STACK_BYTES).
///
/// The capacity of fl::scratch's side stack, split out of fl/stl/scratch.h
/// so headers that only need the number (e.g. fl/gfx/canvas.h, which picks
/// the default drawPolygon tier from it in the preprocessor) stay light.
/// Semantics and platform defaults are documented in fl/stl/scratch.h.

#include "fl/system/sketch_macros.h"  // platform detection (FL_IS_*)
#include "fl/stl/thread_config.h"

#ifndef FL_SCRATCH_STACK_BYTES
#if defined(FL_IS_ESP8266) && !FASTLED_MULTITHREADED
#define FL_SCRATCH_STACK_BYTES 4608
#else
#define FL_SCRATCH_STACK_BYTES 0
#endif
#endif

#ifndef FL_SCRATCH_ALLOW_MULTITHREADED
#define FL_SCRATCH_ALLOW_MULTITHREADED 0
#endif

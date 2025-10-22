#pragma once

#include "scale8.h"
#include "lib8tion/lib8static.h"
#include "intmap.h"
#include "fl/compiler_control.h"

#include "platforms/math8.h"

/// @file math8.h
/// Backward compatibility header for 8-bit math functions.
/// This header includes platforms/math8.h which provides all math functions.
/// New code should use platforms/math8.h directly.

// Bring math8 functions into global namespace for backward compatibility
using fl::qadd8;
using fl::qsub8;
using fl::qadd7;
using fl::qmul8;
using fl::add8;
using fl::add8to16;
using fl::sub8;
using fl::avg8;
using fl::avg16;
using fl::avg8r;
using fl::avg16r;
using fl::avg7;
using fl::avg15;
using fl::mul8;
using fl::abs8;
using fl::blend8;
using fl::mod8;
using fl::addmod8;
using fl::submod8;
using fl::sqrt16;
using fl::sqrt8;

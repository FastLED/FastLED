/// @file _build.cpp
/// @brief Master unity build for fl/ library
/// Compiles entire fl/ namespace into single object file
/// This is the ONLY .cpp file in src/fl/** that should be compiled

// Include placement new early to prevent __cxa_guard_* signature conflicts
// on Teensy 3.x. The platforms/new.h dispatch header handles all platforms.
// See CLAUDE.md "Function-local statics and Teensy 3.x" for details.
#include "platforms/new.h"

// IWYU pragma: begin_keep
#include "fl/stl/undef.h"  // Undefine Arduino macros (min, max, abs, etc.)
// IWYU pragma: end_keep

// Root directory implementations
#include "fl/_build.cpp.hpp"

// Subdirectory implementations (alphabetical order)
#include "fl/audio/_build.cpp.hpp"
#include "fl/channels/_build.cpp.hpp"
#include "fl/font/_build.cpp.hpp"
#include "fl/channels/adapters/_build.cpp.hpp"
#include "fl/channels/detail/validation/_build.cpp.hpp"
#include "fl/chipsets/_build.cpp.hpp"
#include "fl/codec/_build.cpp.hpp"
#include "fl/detail/_build.cpp.hpp"
#include "fl/details/_build.cpp.hpp"
#include "fl/fx/1d/_build.cpp.hpp"
#include "fl/fx/2d/_build.cpp.hpp"
#include "fl/fx/_build.cpp.hpp"
#include "fl/fx/audio/_build.cpp.hpp"
#include "fl/fx/audio/detectors/_build.cpp.hpp"
#include "fl/fx/detail/_build.cpp.hpp"
#include "fl/fx/video/_build.cpp.hpp"
#include "fl/fx/wled/_build.cpp.hpp"
#include "fl/sensors/_build.cpp.hpp"
#include "fl/spi/_build.cpp.hpp"
#include "fl/stl/_build.cpp.hpp"
#include "fl/stl/detail/_build.cpp.hpp"

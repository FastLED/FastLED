/// @file _build.hpp
/// @brief Unity build header for fl/ directory
/// Includes all implementation files in alphabetical order

// Note: Teensy 3.x <new.h> compatibility is handled in _build.cpp
// which includes is_teensy.h before this header.

// begin current directory includes
#include "fl/asset.cpp.hpp"
#include "fl/fastled_internal.cpp.hpp"
#include "fl/fltest.cpp.hpp"
#include "fl/id_tracker.cpp.hpp"
#include "fl/static_constexpr_defs.cpp.hpp"
#include "fl/str_ui.cpp.hpp"
#include "fl/time_alpha.cpp.hpp"
#include "fl/trace.cpp.hpp"
#include "fl/ui.cpp.hpp"

// begin sub directory includes
#include "fl/audio/_build.cpp.hpp"
#include "fl/channels/_build.cpp.hpp"
#include "fl/chipsets/_build.cpp.hpp"
#include "fl/codec/_build.cpp.hpp"
#include "fl/control/_build.cpp.hpp"
#include "fl/detail/_build.cpp.hpp"
#include "fl/details/_build.cpp.hpp"
#include "fl/font/_build.cpp.hpp"
#include "fl/fx/_build.cpp.hpp"
#include "fl/gfx/_build.cpp.hpp"
#include "fl/math/_build.cpp.hpp"
#include "fl/net/_build.cpp.hpp"
#include "fl/remote/_build.cpp.hpp"
#include "fl/sensors/_build.cpp.hpp"

#include "fl/stl/_build.cpp.hpp"
#include "fl/system/_build.cpp.hpp"
#include "fl/task/_build.cpp.hpp"
#include "fl/video/_build.cpp.hpp"

/// @file _build.cpp
/// @brief Master unity build for src/ root directory
/// Compiles entire src/ namespace into single object file
/// This is the ONLY .cpp file in src/ root that should be compiled

// IWYU pragma: begin_keep
#include "fl/stl/undef.h"  // Undefine Arduino macros (min, max, abs, etc.)
// IWYU pragma: end_keep

// Root directory implementations
#include "_build.cpp.hpp"

// Subdirectory implementations (already compiled by their own _build.cpp files)
// - fl/_build.cpp
// - platforms/_build.cpp
// - third_party/*/_build.cpp

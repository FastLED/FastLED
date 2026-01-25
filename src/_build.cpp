/// @file _build.cpp
/// @brief Master unity build for src/ root directory
/// Compiles entire src/ namespace into single object file
/// This is the ONLY .cpp file in src/ root that should be compiled

// Root directory implementations
#include "_build.hpp"

// Subdirectory implementations (already compiled by their own _build.cpp files)
// - fl/_build.cpp
// - platforms/_build.cpp
// - third_party/*/_build.cpp

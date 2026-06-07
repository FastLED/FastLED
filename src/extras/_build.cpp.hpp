/// @file _build.cpp.hpp
/// @brief Unity build header for src/extras/
///
/// `src/extras/` collects build-flag-gated shims. Each header below
/// emits nothing unless a specific opt-in macro is defined in the
/// user's build flags. Including this aggregator from the FastLED
/// unity build is always safe — without the opt-in macro set, each
/// shim expands to nothing.
///
/// Per-shim documentation lives at the top of each `*.cpp.hpp` file.

#include "extras/suppress_arduino_chip_debug_report.cpp.hpp"

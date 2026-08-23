#pragma once

/// @file fl/system/littlefs/fs_littlefs.h
/// @brief Header pair for `fs_littlefs.cpp.hpp`. The public API surface
/// lives in `fl/system/file_system.h` — `make_littlefs_filesystem(bool)`
/// is declared there. This header exists to satisfy the
/// `CppHppHeaderPairChecker` lint rule (every `.cpp.hpp` needs a matching
/// `.h` beside it) and to keep Arduino's `<LittleFS.h>` out of FastLED's
/// header graph.

#include "fl/system/file_system.h" // IWYU pragma: export

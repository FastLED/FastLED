#pragma once

/// @file fl/system/sd/file_system_sd.h
/// @brief Header pair for `file_system_sd.cpp.hpp`. The actual SD-card
/// API surface lives in `fl/system/file_system.h` —
/// `FileSystem::beginSd(int)` and `make_sdcard_filesystem(int)` are
/// declared there. This header exists only to satisfy the
/// `CppHppHeaderPairChecker` lint rule (every `.cpp.hpp` needs a
/// matching `.h` next to it). The SD implementation is split into its
/// own translation unit so the linker can dead-strip it when nobody
/// calls `beginSd`. See `file_system_sd.cpp.hpp` for the mechanism.

#include "fl/system/file_system.h"  // IWYU pragma: export

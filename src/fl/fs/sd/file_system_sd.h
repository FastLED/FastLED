#pragma once

/// @file fl/fs/sd/file_system_sd.h
/// @brief Header pair for `file_system_sd.cpp.hpp`. The actual SD-card
/// API surface lives in `fl/fs/fs.h` —
/// `FileSystem::beginSd(int)` and `make_sdcard_filesystem(int)` are
/// declared there. This header exists only to satisfy the
/// `CppHppHeaderPairChecker` lint rule (every `.cpp.hpp` needs a
/// matching `.h` next to it). The SD implementation is split into its
/// own translation unit so the linker can dead-strip it when nobody
/// calls `beginSd`. See `file_system_sd.cpp.hpp` for the mechanism.

#include "fl/fs/fs.h"  // IWYU pragma: export

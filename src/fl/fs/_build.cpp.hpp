// IWYU pragma: private

/// @file fl/fs/_build.cpp.hpp
/// @brief Unity-build aggregate for the filesystem subsystem.
///
/// Covers the generic layer only -- `FileSystem`, the stream types, and the
/// format helpers. Storage backends live in sibling directories with their
/// own aggregates (`fl/fs/sd/`, `fl/fs/embedded/`) and their own translation
/// units, so the linker can drop a backend a sketch never mounts
/// (FastLED #2773 item 1.2).

// begin current directory includes
#include "fl/fs/file_handle.cpp.hpp"
#include "fl/fs/fs.cpp.hpp"
#include "fl/fs/fstream.cpp.hpp"
#include "fl/fs/read.cpp.hpp"

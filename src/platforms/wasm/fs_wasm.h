#pragma once

// IWYU pragma: private

#include "fl/stl/stdint.h"
// IWYU pragma: begin_keep
#include <string>
// IWYU pragma: end_keep

#include "fl/system/file_system.h"
#include "fl/stl/memory.h"
#include "fl/stl/span.h"
#include "fl/stl/noexcept.h"

namespace fl {

FsImplPtr make_sdcard_filesystem(int cs_pin) FL_NOEXCEPT;

}

extern "C" {
// Called from the browser side, this is intended to create a file
// at the given path with the given data. You can only do this once.
bool jsInjectFile(const char *path, const fl::u8 *data, size_t len) FL_NOEXCEPT;

// Declare a file with the given path and length. Then jsAppendFile can
// be called to fill in the data later.
bool jsDeclareFile(const char *path, size_t len) FL_NOEXCEPT;

// After a file is declared, it can be appended with more data.
bool jsAppendFile(const char *path, const fl::u8 *data, size_t len) FL_NOEXCEPT;

void fastled_declare_files(const char* jsonStr) FL_NOEXCEPT;
} // extern "C"

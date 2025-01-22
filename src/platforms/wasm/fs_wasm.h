#pragma once

#include <string>
#include <stdint.h>

#include "fl/file_system.h"
#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fl/slice.h"

namespace fl {

FsImplPtr make_sdcard_filesystem(int cs_pin);

}

extern "C" {
// Called from the browser side, this is intended to create a file
// at the given path with the given data. You can only do this once.
bool jsInjectFile(const char *path, const uint8_t *data, size_t len);


// Declare a file with the given path and length. Then jsAppendFile can
// be called to fill in the data later.
bool jsDeclareFile(const char* path, size_t len);

// After a file is declared, it can be appended with more data.
void jsAppendFile(const char *path, const uint8_t *data, size_t len);

void fastled_declare_files(std::string jsonStr);
}  // extern "C"

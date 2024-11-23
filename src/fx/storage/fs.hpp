#pragma once

#include "fl/file_system.h"
#include "namespace.h"

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/fs_wasm.h"
#elif __has_include(<SD.h>)
#include "fs_sdcard_arduino.hpp"
#else

namespace fl {
inline FsImplPtr make_sdcard_filesystem(int cs_pin) {
    return FsImplPtr::Null();
}
}  // namespace fl

#endif




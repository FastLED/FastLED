#pragma once

#include "fs.h"

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/fs_wasm.h"
#elif __has_include(<SD.h>)
#include "fs_sdcard_arduino.hpp"
#else
inline FsImplPtr make_filesystem(int cs_pin) {
    return FsImplPtr::Null();
}
#endif




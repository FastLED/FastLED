#pragma once

#include "fl/file_system.h"
#include "namespace.h"

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/fs_wasm.h"
#elif __has_include(<SD.h>)
#include "fs_sdcard_arduino.hpp"
#else
FASTLED_NAMESPACE_BEGIN
inline FsImplPtr make_sdcard_filesystem(int cs_pin) {
    return FsImplPtr::Null();
}
FASTLED_NAMESPACE_END
#endif




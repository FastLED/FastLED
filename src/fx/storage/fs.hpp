#pragma once

#include "fs.h"

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/fs.h"
#elif __has_include(<SD.h>)
#include "fs_sdcard_arduino.hpp"
#else
FsImplPtr FsImpl::New(int cs_pin) {
    return FsImplPtr::Null();

}
#endif




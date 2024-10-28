#pragma once

#include "fs.h"

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/fs.h"
#elif __has_include(<SD.h>)
#include "fs_sdcard_arduino.hpp"
#else
FsPtr Fs::New(int cs_pin) {
    FsPtr out;
    return out;

}
#endif




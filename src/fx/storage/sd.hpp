#pragma once

#include "sd.h"

#ifdef __EMSCRIPTEN__
#include "platforms/wasm/sd.h"
#elif __has_include(<SD.h>)
#include "sd_arduino.hpp"
#else
SdCardPtr SdCard::New(int cs_pin) {
    SdCardPtr out;
    return out;

}
#endif




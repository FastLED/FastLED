#pragma once

#include "sd.h"


#if __has_include(<SD.h>)
#include "sd_arduino.hpp"
#else
SdCardPtr SdCard::New(int cs_pin) {
    SdCardPtr out;
    return out;

}
#endif




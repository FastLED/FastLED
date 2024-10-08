#pragma once

#include "sd.h"


#if __has_include(<SD.h>)
#include "sd_arduino.hpp"
#else
SdCardSpiPtr SdCardSpi::New(int cs_pin) {
    SdCardSpiPtr out;
    return out;

}
#endif




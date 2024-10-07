#pragma once

#include "fx/storage/sd.h"

#include <SPI.h>
#ifdef USE_SDFAT
#include <SdFat.h>
#else
#include <SD.h>
#endif

#include "namespace.h"
#include "ptr.h"

FASTLED_NAMESPACE_BEGIN

storage::ISdCardSpi *createSdCardSpi(int cs_pin) {
    return nullptr;
}

FASTLED_NAMESPACE_END

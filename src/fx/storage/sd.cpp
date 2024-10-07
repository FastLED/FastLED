#include "fx/storage/sd.h"

#if FASTLED_HAS_SD
#include "sd_std.hpp"
bool storage::SdCardSpi::isCompiledIn() {
    return true;
}
#else
bool storage::SdCardSpi::isCompiledIn() {
    return false;
}
#endif




#pragma once


#include <stdint.h>
#include <stddef.h>
#include "filehandle.h"
#include "namespace.h"


#if __has_include(<SdFat.h>)
#define FASTLED_HAS_SD 1
#else
#define FASTLED_HAS_SD 0
#endif

FASTLED_NAMESPACE_BEGIN

namespace storage
{

class SdCardSpi {
  public:
    SdCardSpi(int clock, int data) : mClockPin(clock), mDataPin(data) {}
    SdCardSpi() {}  // Use default pins for spi.
    bool begin(int chipSelect);
    //  End use of card
    void end();

    // Not implemented yet.
    // bool restart();

    FileHandlePtr open(const char *name, uint8_t oflag);
    void close(FileHandle *file);
    bool isOpen() const { return mChipSelect >= 0; }

  private:
    int mClockPin = -1;
    int mDataPin = -1;
    int mChipSelect = -1;
};

} // namespace storage

FASTLED_NAMESPACE_END
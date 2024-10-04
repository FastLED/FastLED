#pragma once


#include <stdint.h>
#include <stddef.h>
#include "filehandle.h"


#if __has_include(<SdFat.h>)
#define FASTLED_HAS_SD 1
#else
#define FASTLED_HAS_SD 0
#endif

namespace storage
{

class SdCardSpi {
  public:
    SdCardSpi(int clock, int data) : mClockPin(clock), mDataPin(data) {}
    SdCardSpi() {}  // Use default pins for spi.
    bool begin();
    //  End use of card
    void end();

    // Not implemented yet.
    // bool restart();

    FileHandle* open(const char *name, uint8_t oflag);
    void close(FileHandle *file);
  private:
    int mClockPin = -1;
    int mDataPin = -1;
    int mActive = false;
};

} // namespace storage
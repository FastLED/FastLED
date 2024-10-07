#pragma once


#include <stdint.h>
#include <stddef.h>
#include "filehandle.h"
#include "namespace.h"


FASTLED_NAMESPACE_BEGIN

namespace storage
{

class ISdCardSpi {
  public:
    virtual ~ISdCardSpi() {}  // Use default pins for spi.
    virtual bool begin(int chipSelect) = 0;
    //  End use of card
    virtual void end() = 0;
    virtual FileHandlePtr openRead(const char *name) = 0;
    virtual void close(FileHandle *file) = 0;
};


ISdCardSpi *createSdCardSpi(int cs_pin);

} // namespace storage

FASTLED_NAMESPACE_END

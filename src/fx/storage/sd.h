#pragma once


#include <stdint.h>
#include <stddef.h>
#include "filehandle.h"
#include "namespace.h"
#include "ptr.h"
#include "filehandle.h"


FASTLED_NAMESPACE_BEGIN


DECLARE_SMART_PTR(SdCardSpi)

class FileReader {
  public:
    virtual FileHandlePtr openRead(const char *path) = 0;
};

class SdCardSpi : public Referent, FileReader {
  public:
    static SdCardSpiPtr make(int cs_pin);
    virtual ~SdCardSpi() {}  // Use default pins for spi.
    virtual bool begin(int chipSelect) = 0;
    //  End use of card
    virtual void end() = 0;
    virtual void close(FileHandlePtr file) = 0;
};

FASTLED_NAMESPACE_END

#pragma once


#include <stdint.h>
#include <stddef.h>
#include "filehandle.h"
#include "namespace.h"
#include "ptr.h"
#include "filehandle.h"


FASTLED_NAMESPACE_BEGIN


DECLARE_SMART_PTR(SdCardSpi);

class FileReader {
  public:
    struct Visitor {
      virtual void accept(const char* path) = 0;
    };
  
    virtual FileHandlePtr openRead(const char *path) = 0;
    virtual void close(FileHandlePtr file) = 0;

    // False signals to stop the file iteration.
    virtual bool ls(Visitor &visitor) = 0;
};

class SdCardSpi : public Referent, public FileReader {
  public:
    static SdCardSpiPtr make(int cs_pin);

    virtual ~SdCardSpi() {}  // Use default pins for spi.
    virtual bool begin(int chipSelect) = 0;
    //  End use of card
    virtual void end() = 0;
    virtual void close(FileHandlePtr file) = 0;

    virtual bool ls(Visitor &visitor) {
      // todo: implement.
      return false;
    }

};

FASTLED_NAMESPACE_END

#pragma once


#include <stdint.h>
#include <stddef.h>
#include "filehandle.h"
#include "namespace.h"
#include "ptr.h"
#include "filehandle.h"
#include "filereader.h"


FASTLED_NAMESPACE_BEGIN


DECLARE_SMART_PTR(SdCardSpi);

class SdCardSpi : public Referent, public FileReader {
  public:
    static SdCardSpiPtr make(int cs_pin);

    virtual ~SdCardSpi() {}  // Use default pins for spi.
    virtual bool begin(int chipSelect) = 0;
    //  End use of card
    virtual void end() = 0;
    virtual void close(FileHandlePtr file) = 0;

    virtual bool ls(FileReader::Visitor &visitor) {
      // todo: implement.
      return false;
    }

};

FASTLED_NAMESPACE_END

#pragma once


#include <stdint.h>
#include <stddef.h>
#include "filehandle.h"
#include "namespace.h"
#include "ptr.h"
#include "filehandle.h"
#include "filereader.h"


FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(Fs);

// A filesystem interface that abstracts the underlying filesystem, usually
// an sd card.
class Fs : public Referent, public FileReader {
  public:
    static FsPtr New(int cs_pin);

    virtual ~Fs() {}
    //  End use of card
    virtual void end() = 0;
    virtual void close(FileHandlePtr file) = 0;
    virtual FileHandlePtr openRead(const char *path) = 0;

    virtual bool ls(FileReader::Visitor &visitor) {
      // todo: implement.
      return false;
    }
};

DECLARE_SMART_PTR_CONSTRUCTOR(Fs, Fs::New);

FASTLED_NAMESPACE_END

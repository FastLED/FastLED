#pragma once


#include <stdint.h>
#include <stddef.h>
#include "namespace.h"
#include "ptr.h"
#include "filehandle.h"
#include "filereader.h"


FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(FsImpl);

// Platforms eed to implement this to create an instance of the filesystem.
FsImplPtr make_filesystem(int cs_pin);


// A filesystem interface that abstracts the underlying filesystem, usually
// an sd card.
class FsImpl : public Referent, public FileReader {
  public:
    FsImpl() = default;
    virtual ~FsImpl() {}  // Use default pins for spi.
    virtual bool begin() = 0;
    //  End use of card
    virtual void end() = 0;
    virtual void close(FileHandlePtr file) = 0;
    virtual FileHandlePtr openRead(const char *path) = 0;

    virtual bool ls(FileReader::Visitor &visitor) {
      // todo: implement.
      return false;
    }
};


class Fs {
  public:
    Fs(int cs_pin) : mFs(make_filesystem(cs_pin)) {}
    Fs(FsImplPtr fs) : mFs(fs) {}
    bool begin() {
      if (!mFs) {
        return false;
      }
      mFs->begin();
      return true;
    }
    void end() { mFs->end(); }
    void close(FileHandlePtr file) { mFs->close(file); }
    FileHandlePtr openRead(const char *path) { return mFs->openRead(path); }
    
  private:
    Ptr<FsImpl> mFs;
};

FASTLED_NAMESPACE_END

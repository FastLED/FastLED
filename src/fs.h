#pragma once


#include <stdint.h>
#include <stddef.h>
#include "namespace.h"
#include "ptr.h"


FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(FsImpl);
DECLARE_SMART_PTR(FileHandle);

// Platforms eed to implement this to create an instance of the filesystem.
FsImplPtr make_filesystem(int cs_pin);


// An abstract class that represents a file handle.
// Devices like the SD card will return one of these.
class FileHandle: public Referent {
  public:
    virtual ~FileHandle() {}
    virtual bool available() const = 0;
    virtual size_t bytesLeft() const { return size() - pos(); }
    virtual size_t size() const = 0;
    virtual size_t read(uint8_t *dst, size_t bytesToRead) = 0;
    virtual size_t pos() const = 0;
    virtual const char* path() const = 0;
    virtual void seek(size_t pos) = 0;
    virtual void close() = 0;
};

// A filesystem interface that abstracts the underlying filesystem, usually
// an sd card.
class FsImpl : public Referent {
  public:
    struct Visitor {
      virtual void accept(const char* path) = 0;
    };
    FsImpl() = default;
    virtual ~FsImpl() {}  // Use default pins for spi.
    virtual bool begin() = 0;
    //  End use of card
    virtual void end() = 0;
    virtual void close(FileHandlePtr file) = 0;
    virtual FileHandlePtr openRead(const char *path) = 0;

    virtual bool ls(Visitor &visitor) {
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

#pragma once

// Note, fs.h breaks ESPAsyncWebServer so we use file_system.h instead.

#include <stdint.h>
#include <stddef.h>

#include "namespace.h"
#include "ref.h"


FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(FsImpl);
FASTLED_SMART_REF(FileHandle);

// Instantiate this with a pin number to create a filesystem.
class Fs {
  public:
    Fs(int cs_pin);  // Initializes this as a spi sd card file system.
    Fs(FsImplRef fs);  // Use this to provide a custom filesystem.
    bool begin(); // Signal to begin using the filesystem resource.
    void end(); // Signal to end use of the file system.
    
    FileHandleRef openRead(const char *path);  // Null if file could not be opened.
    void close(FileHandleRef file);
    
  private:
    FsImplRef mFs;  // System dependent filesystem.
};


/// Implementation details

// Platforms eed to implement this to create an instance of the filesystem.
FsImplRef make_filesystem(int cs_pin);


// An abstract class that represents a file handle.
// Devices like the SD card will return one of these.
class FileHandle: public Referent {
  public:
    virtual ~FileHandle() {}
    virtual bool available() const = 0;
    virtual size_t bytesLeft() const;
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
    virtual void close(FileHandleRef file) = 0;
    virtual FileHandleRef openRead(const char *path) = 0;

    virtual bool ls(Visitor &visitor) {
      // todo: implement.
      return false;
    }
};



FASTLED_NAMESPACE_END

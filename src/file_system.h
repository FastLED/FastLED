#pragma once

// Note, fs.h breaks ESPAsyncWebServer so we use file_system.h instead.

#include <stdint.h>
#include <stddef.h>

#include "namespace.h"
#include "ref.h"
#include "fx/video.h"

FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(FsImpl);

// Platforms need to implement this to create an instance of the filesystem.
FsImplRef make_sdcard_filesystem(int cs_pin);

FASTLED_SMART_REF(FileHandle);
class Video;

// Instantiate this with a pin number to create a filesystem.
class FileSystem {
  public:
    FileSystem();
    bool beginSd(int cs_pin); // Signal to begin using the filesystem resource.
    bool begin(FsImplRef platform_filesystem); // Signal to begin using the filesystem resource.
    void end(); // Signal to end use of the file system.

    
    FileHandleRef openRead(const char *path);  // Null if file could not be opened.
    Video openVideo(const char *path, size_t pixelsPerFrame, float fps = 30.0f, size_t nFrameHistory = 0);  // Null if video could not be opened.
    void close(FileHandleRef file);
    
  private:
    FsImplRef mFs;  // System dependent filesystem.
};



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

// Platforms will subclass this to implement the filesystem.
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
      (void)visitor;
      return false;
    }
};

FASTLED_NAMESPACE_END

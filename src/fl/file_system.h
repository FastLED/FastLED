#pragma once

// Note, fs.h breaks ESPAsyncWebServer so we use file_system.h instead.

#include <stdint.h>
#include <stddef.h>

#include "namespace.h"
#include "fl/ptr.h"
#include "fx/video.h"
#include "fl/str.h"

FASTLED_NAMESPACE_BEGIN
struct CRGB;
class Video;
FASTLED_NAMESPACE_END

namespace fl {

FASTLED_SMART_PTR(FsImpl);
FASTLED_SMART_PTR(FileSystem);
FASTLED_SMART_PTR(FileHandle);

// Platforms need to implement this to create an instance of the filesystem.
FsImplPtr make_sdcard_filesystem(int cs_pin);



// Instantiate this with a pin number to create a filesystem.
class FileSystem {
  public:
    FileSystem();
    bool beginSd(int cs_pin); // Signal to begin using the filesystem resource.
    bool begin(FsImplPtr platform_filesystem); // Signal to begin using the filesystem resource.
    void end(); // Signal to end use of the file system.

    
    FileHandlePtr openRead(const char *path);  // Null if file could not be opened.
    Video openVideo(const char *path, size_t pixelsPerFrame, float fps = 30.0f, size_t nFrameHistory = 0);  // Null if video could not be opened.
    bool readText(const char *path, fl::Str* out);
    void close(FileHandlePtr file);
    
  private:
    FsImplPtr mFs;  // System dependent filesystem.
};



// An abstract class that represents a file handle.
// Devices like the SD card will return one of these.
class FileHandle: public fl::Referent {
  public:
    virtual ~FileHandle() {}
    virtual bool available() const = 0;
    virtual size_t bytesLeft() const;
    virtual size_t size() const = 0;
    virtual size_t read(uint8_t *dst, size_t bytesToRead) = 0;
    virtual size_t pos() const = 0;
    virtual const char* path() const = 0;
    virtual bool seek(size_t pos) = 0;
    virtual void close() = 0;

    // convenience functions
    size_t readCRGB(CRGB* dst, size_t n) {
      return read((uint8_t*)dst, n * 3) / 3;
    }
};

// Platforms will subclass this to implement the filesystem.
class FsImpl : public fl::Referent {
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
      (void)visitor;
      return false;
    }
};

}  // namespace

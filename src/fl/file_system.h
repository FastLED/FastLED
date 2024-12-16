#pragma once

// Note, fs.h breaks ESPAsyncWebServer so we use file_system.h instead.

#include <stdint.h>
#include <stddef.h>

#include "fl/namespace.h"
#include "fl/ptr.h"
#include "fx/video.h"
#include "fl/str.h"


namespace fl {

FASTLED_SMART_PTR(FsImpl);
// PLATFORM INTERFACE
// You need to define this for your platform.
// Otherwise a null filesystem will be used that will do nothing but spew warnings, but otherwise
// won't crash the system.
FsImplPtr make_sdcard_filesystem(int cs_pin);
}

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class ScreenMap;
FASTLED_SMART_PTR(FileSystem);
FASTLED_SMART_PTR(FileHandle);
class Video;
template<typename Key, typename Value, size_t N> class FixedMap;




class JsonDocument;

class FileSystem {
  public:
    FileSystem();
    bool beginSd(int cs_pin); // Signal to begin using the filesystem resource.
    bool begin(FsImplPtr platform_filesystem); // Signal to begin using the filesystem resource.
    void end(); // Signal to end use of the file system.

    
    FileHandlePtr openRead(const char *path);  // Null if file could not be opened.
    Video openVideo(const char *path, size_t pixelsPerFrame, float fps = 30.0f, size_t nFrameHistory = 0);  // Null if video could not be opened.
    bool readText(const char *path, Str* out);
    bool readJson(const char *path, JsonDocument* doc);
    bool readScreenMaps(const char *path, FixedMap<Str, ScreenMap, 16>* out, Str* error = nullptr);
    bool readScreenMap(const char *path, const char* name, ScreenMap* out, Str* error = nullptr);
    void close(FileHandlePtr file);
    
  private:
    FsImplPtr mFs;  // System dependent filesystem.
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
    virtual bool seek(size_t pos) = 0;
    virtual void close() = 0;
    virtual bool valid() const = 0;

    // convenience functions
    size_t readCRGB(CRGB* dst, size_t n) {
      return read((uint8_t*)dst, n * 3) / 3;
    }
};

// Platforms will subclass this to implement the filesystem.
class FsImpl : public Referent {
  public:
    struct Visitor {
      virtual ~Visitor() {}
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

}  // namespace fl

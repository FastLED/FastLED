#pragma once

// Note, fs.h breaks ESPAsyncWebServer so we use file_system.h instead.

#include "fl/stdint.h"
#include "fl/int.h"

#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/str.h"
#include "fx/video.h"
#include "fl/screenmap.h"

namespace fl {

FASTLED_SMART_PTR(FsImpl);
// PLATFORM INTERFACE
// You need to define this for your platform.
// Otherwise a null filesystem will be used that will do nothing but spew
// warnings, but otherwise won't crash the system.
FsImplPtr make_sdcard_filesystem(int cs_pin);
} // namespace fl

FASTLED_NAMESPACE_BEGIN
struct CRGB;
FASTLED_NAMESPACE_END

namespace fl {

class ScreenMap;
FASTLED_SMART_PTR(FileSystem);
FASTLED_SMART_PTR(FileHandle);
class Video;
template <typename Key, typename Value, fl::size N> class FixedMap;

namespace json2 {
class Json;
}

class FileSystem {
  public:
    FileSystem();
    bool beginSd(int cs_pin); // Signal to begin using the filesystem resource.
    bool begin(FsImplPtr platform_filesystem); // Signal to begin using the
                                               // filesystem resource.
    void end(); // Signal to end use of the file system.

    FileHandlePtr
    openRead(const char *path); // Null if file could not be opened.
    Video
    openVideo(const char *path, fl::size pixelsPerFrame, float fps = 30.0f,
              fl::size nFrameHistory = 0); // Null if video could not be opened.
    bool readText(const char *path, string *out);
    bool readJson(const char *path, Json *doc);
    bool readScreenMaps(const char *path, fl::fl_map<string, ScreenMap> *out,
                        string *error = nullptr);
    bool readScreenMap(const char *path, const char *name, ScreenMap *out,
                       string *error = nullptr);
    void close(FileHandlePtr file);

  private:
    FsImplPtr mFs; // System dependent filesystem.
};

// An abstract class that represents a file handle.
// Devices like the SD card will return one of these.
class FileHandle {
  public:
    virtual ~FileHandle() {}
    virtual bool available() const = 0;
    virtual fl::size bytesLeft() const;
    virtual fl::size size() const = 0;
    virtual fl::size read(fl::u8 *dst, fl::size bytesToRead) = 0;
    virtual fl::size pos() const = 0;
    virtual const char *path() const = 0;
    virtual bool seek(fl::size pos) = 0;
    virtual void close() = 0;
    virtual bool valid() const = 0;

    // convenience functions
    fl::size readCRGB(CRGB *dst, fl::size n) {
        return read((fl::u8 *)dst, n * 3) / 3;
    }
};

// Platforms will subclass this to implement the filesystem.
class FsImpl {
  public:
    struct Visitor {
        virtual ~Visitor() {}
        virtual void accept(const char *path) = 0;
    };
    FsImpl() = default;
    virtual ~FsImpl() {} // Use default pins for spi.
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

} // namespace fl

#pragma once

// Note, fs.h breaks ESPAsyncWebServer so we use file_system.h instead.

#include "fl/stl/stdint.h"
#include "fl/int.h"

#include "fl/stl/shared_ptr.h"         // For FASTLED_SHARED_PTR macros
#include "fl/str_fwd.h"     // Lightweight forward declarations for string
#include "fl/fx/video.h"
#include "fl/screenmap.h"
#include "fl/codec/jpeg.h"
#include "fl/codec/mp3.h"
#include "fl/deprecated.h"
#include "crgb.h"

namespace fl {

FASTLED_SHARED_PTR(FsImpl);
// PLATFORM INTERFACE
// You need to define this for your platform.
// Otherwise a null filesystem will be used that will do nothing but spew
// warnings, but otherwise won't crash the system.
FsImplPtr make_sdcard_filesystem(int cs_pin);

#ifdef FASTLED_TESTING
// Test-specific functions for setting up filesystem root path
// These are implemented in platforms/stub/fs_stub.hpp
void setTestFileSystemRoot(const char* root_path);
const char* getTestFileSystemRoot();
#endif

} // namespace fl

namespace fl {

class ScreenMap;
FASTLED_SHARED_PTR(FileSystem);
FASTLED_SHARED_PTR(FileHandle);
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
    Video
    openMpeg1Video(const char *path, fl::size pixelsPerFrame, float fps = 30.0f,
                   fl::size nFrameHistory = 0); // Open MPEG1 video file
    bool readText(const char *path, string *out);
    bool readJson(const char *path, Json *doc);
    bool readScreenMaps(const char *path, fl::fl_map<string, ScreenMap> *out,
                        string *error = nullptr);
    bool readScreenMap(const char *path, const char *name, ScreenMap *out,
                       string *error = nullptr);
    void close(FileHandlePtr file);

    // Load JPEG image from file path directly to Frame
    FramePtr loadJpeg(const char *path, const JpegConfig &config = JpegConfig(),
                      fl::string *error_message = nullptr);

    // Open MP3 audio file and return streaming decoder
    fl::Mp3DecoderPtr openMp3(const char *path,
                              fl::string *error_message = nullptr);

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
    // New preferred method using CRGB
    fl::size readRGB8(CRGB *dst, fl::size n) {
        return read((fl::u8 *)dst, n * 3) / 3;
    }

    // Deprecated: Use readRGB8 instead
    FL_DEPRECATED("Use readRGB8(CRGB* dst, ...) instead")
    fl::size readCRGB(CRGB *dst, fl::size n) {
        return readRGB8(dst, n);
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

// Standalone helper function to load JPEG from SD card
// Combines SD card initialization and JPEG loading in one convenient function
inline FramePtr loadJpegFromSD(int cs_pin, const char *filepath,
                                 const JpegConfig &config = JpegConfig(),
                                 fl::string *error_message = nullptr) {
    FileSystem fs;
    if (!fs.beginSd(cs_pin)) {
        if (error_message) {
            *error_message = "Failed to initialize SD card on CS pin ";
            error_message->append(static_cast<fl::u32>(cs_pin));
        }
        return FramePtr();
    }
    return fs.loadJpeg(filepath, config, error_message);
}

} // namespace fl

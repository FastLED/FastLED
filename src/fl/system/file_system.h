#pragma once

// Note, fs.h breaks ESPAsyncWebServer so we use file_system.h instead.

#include "fl/stl/int.h"

#include "fl/stl/shared_ptr.h"         // IWYU pragma: export
#include "fl/stl/fstream.h"            // IWYU pragma: export
#include "fl/stl/flat_map.h"           // IWYU pragma: export
#include "fl/stl/string.h"             // IWYU pragma: export
#include "fl/fx/video.h"               // IWYU pragma: export
#include "fl/codec/jpeg.h"             // IWYU pragma: export
#include "fl/stl/compiler_control.h"   // IWYU pragma: export
#include "fl/stl/noexcept.h"

// Forward declaration — concrete Mp3Decoder lives in fl/codec/mp3.h
namespace fl {
class Mp3Decoder;
using Mp3DecoderPtr = fl::shared_ptr<Mp3Decoder>;
}

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
class Video;
template <typename Key, typename Value, fl::size N> class unsorted_map_fixed;

namespace json2 {
class json;
}

class FileSystem {
  public:
    FileSystem() FL_NOEXCEPT;
    bool beginSd(int cs_pin); // Signal to begin using the filesystem resource.
    bool begin(FsImplPtr platform_filesystem); // Signal to begin using the
                                               // filesystem resource.
    void end(); // Signal to end use of the file system.

    fl::ifstream
    openRead(const char *path); // Returns closed ifstream if file could not be opened.
    Video
    openVideo(const char *path, fl::size pixelsPerFrame, float fps = 30.0f,
              fl::size nFrameHistory = 0); // Null if video could not be opened.
    Video
    openMpeg1Video(const char *path, fl::size pixelsPerFrame, float fps = 30.0f,
                   fl::size nFrameHistory = 0); // Open MPEG1 video file
    bool readText(const char *path, string *out);
    bool readJson(const char *path, json *doc);
    bool readScreenMaps(const char *path, fl::flat_map<string, ScreenMap> *out,
                        string *error = nullptr);
    bool readScreenMap(const char *path, const char *name, ScreenMap *out,
                       string *error = nullptr);
    // Load JPEG image from file path directly to Frame
    FramePtr loadJpeg(const char *path, const JpegConfig &config = JpegConfig(),
                      fl::string *error_message = nullptr);

    // Open MP3 audio file and return streaming decoder
    fl::Mp3DecoderPtr openMp3(const char *path,
                              fl::string *error_message = nullptr);

  private:
    FsImplPtr mFs; // System dependent filesystem.
};

// Platforms will subclass this to implement the filesystem.
class FsImpl {
  public:
    struct Visitor {
        virtual ~Visitor() FL_NOEXCEPT {}
        virtual void accept(const char *path) = 0;
    };
    FsImpl() FL_NOEXCEPT = default;
    virtual ~FsImpl() FL_NOEXCEPT {} // Use default pins for spi.
    virtual bool begin() = 0;
    //  End use of card
    virtual void end() = 0;
    virtual filebuf_ptr openRead(const char *path) = 0;

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

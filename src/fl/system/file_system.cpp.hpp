#include "fl/system/file_system.h"
#include "fl/stl/has_include.h"
#include "fl/system/log.h"
#include "fl/stl/vector.h"
#include "fl/math/math.h"

#ifdef FASTLED_TESTING
// Test filesystem implementation that maps to real hard drive
// IWYU pragma: begin_keep
#include "platforms/stub/fs_stub.hpp" // ok platform headers
// IWYU pragma: end_keep
#define FASTLED_HAS_SDCARD 1
#elif defined(FL_IS_WASM)
// IWYU pragma: begin_keep
#include "platforms/wasm/fs_wasm.h" // ok platform headers
// IWYU pragma: end_keep
#define FASTLED_HAS_SDCARD 1
#elif FL_HAS_INCLUDE(<SD.h>) && FL_HAS_INCLUDE(<fs.h>)
// Include Arduino SD card implementation when SD library is available
#include "platforms/fs_sdcard_arduino.hpp"
#define FASTLED_HAS_SDCARD 1
#else
#define FASTLED_HAS_SDCARD 0
#endif

#include "fl/stl/json.h"
#include "fl/math/screenmap.h"
#include "fl/math/math.h" // for min
#include "fl/stl/cstring.h"
#include "fl/stl/noexcept.h"

// Codec-dependent methods (openMp3, loadJpeg, openMpeg1Video, Mpeg1FileHandle)
// moved to fl/codec/file_system_codecs.cpp.hpp to break fl.system+ -> fl.codec+ chain.

namespace fl {

class NullFileHandle : public filebuf {
  public:
    NullFileHandle() FL_NOEXCEPT = default;
    ~NullFileHandle() FL_NOEXCEPT override {}

    bool is_open() const override { return false; }
    fl::size_t size() const override { return 0; }
    fl::size_t read(char *dst, fl::size_t bytesToRead) override {
        FASTLED_UNUSED(dst);
        FASTLED_UNUSED(bytesToRead);
        return 0;
    }
    fl::size_t write(const char *data, fl::size_t count) override {
        FASTLED_UNUSED(data);
        FASTLED_UNUSED(count);
        return 0;
    }
    fl::size_t tell() override { return 0; }
    const char *path() const override { return "nullptr filebuf"; }
    bool seek(fl::size_t pos, seek_dir dir) override {
        FASTLED_UNUSED(pos);
        FASTLED_UNUSED(dir);
        return false;
    }
    using filebuf::seek; // single-arg overload
    void close() override {}
    bool is_eof() const override { return true; }
    bool has_error() const override { return false; }
    void clear_error() override {}
    int error_code() const override { return 0; }
    const char *error_message() const override { return "NullFileHandle"; }
};

class NullFileSystem : public FsImpl {
  public:
    NullFileSystem() FL_NOEXCEPT {
        FASTLED_WARN("NullFileSystem instantiated as a placeholder, please "
                     "implement a file system for your platform.");
    }
    ~NullFileSystem() FL_NOEXCEPT override {}

    bool begin() override { return true; }
    void end() override {}

    filebuf_ptr openRead(const char *_path) override {
        FASTLED_UNUSED(_path);
        fl::shared_ptr<NullFileHandle> ptr = fl::make_shared<NullFileHandle>();
        filebuf_ptr out = ptr;
        return out;
    }
};


bool FileSystem::beginSd(int cs_pin) {
    mFs = make_sdcard_filesystem(cs_pin);
    if (!mFs) {
        return false;
    }
    mFs->begin();
    return true;
}

bool FileSystem::begin(FsImplPtr platform_filesystem) {
    mFs = platform_filesystem;
    if (!mFs) {
        return false;
    }
    mFs->begin();
    return true;
}

FileSystem::FileSystem() : mFs() {}

void FileSystem::end() {
    if (mFs) {
        mFs->end();
    }
}

bool FileSystem::readJson(const char *path, json *doc) {
    string text;
    if (!readText(path, &text)) {
        return false;
    }
    
    // Parse using the new json class
    *doc = fl::json::parse(text);
    return !doc->is_null();
}

bool FileSystem::readScreenMaps(const char *path,
                                fl::flat_map<string, ScreenMap> *out, string *error) {
    string text;
    if (!readText(path, &text)) {
        FASTLED_WARN("Failed to read file: " << path);
        if (error) {
            *error = "Failed to read file: ";
            error->append(path);
        }
        return false;
    }
    string err;
    bool ok = ScreenMap::ParseJson(text.c_str(), out, &err);
    if (!ok) {
        FASTLED_WARN("Failed to parse screen map: " << err.c_str());
        *error = err;
        return false;
    }
    return true;
}

bool FileSystem::readScreenMap(const char *path, const char *name,
                               ScreenMap *out, string *error) {
    string text;
    if (!readText(path, &text)) {
        FASTLED_WARN("Failed to read file: " << path);
        if (error) {
            *error = "Failed to read file: ";
            error->append(path);
        }
        return false;
    }
    string err;
    bool ok = ScreenMap::ParseJson(text.c_str(), name, out, &err);
    if (!ok) {
        FASTLED_WARN("Failed to parse screen map: " << err.c_str());
        *error = err;
        return false;
    }
    return true;
}

fl::ifstream FileSystem::openRead(const char *path) {
    return fl::ifstream(mFs->openRead(path));
}
Video FileSystem::openVideo(const char *path, fl::size pixelsPerFrame, float fps,
                            fl::size nFrameHistory) {
    Video video(pixelsPerFrame, fps, nFrameHistory);
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        video.setError(fl::string("Could not open file: ").append(path));
        return video;
    }
    video.begin(file.rdbuf());
    return video;
}

bool FileSystem::readText(const char *path, fl::string *out) {
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        FASTLED_WARN("Failed to open file: " << path);
        return false;
    }
    fl::size size = file.size();
    out->reserve(size + out->size());
    bool wrote = false;
    while (file.available()) {
        u8 buf[64];
        fl::size n = file.read(buf, sizeof(buf));
        out->append((const char *)buf, n);
        wrote = true;
    }
    file.close();
    FASTLED_DBG_IF(!wrote, "Failed to write any data to the output string.");
    return wrote;
}

} // namespace fl

namespace fl {

#if !FASTLED_HAS_SDCARD
// Weak fallback implementation when SD library is not available
FL_LINK_WEAK FsImplPtr make_sdcard_filesystem(int cs_pin) {
    FASTLED_UNUSED(cs_pin);
    fl::shared_ptr<NullFileSystem> ptr = fl::make_shared<NullFileSystem>();
    FsImplPtr out = ptr;
    return out;
}
#endif

} // namespace fl

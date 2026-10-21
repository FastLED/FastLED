#include "fl/fs/fs.h"
#include "fl/fled/fled.h"
#include "fl/stl/has_include.h"
#include "fl/log/log.h"
#include "fl/stl/vector.h"
#include "fl/math/math.h"

// NOTE: SD card support (FileSystem::beginSd and make_sdcard_filesystem)
// lives in a SEPARATE translation unit at
// `src/fl/fs/sd/file_system_sd.cpp.hpp`, compiled into
// `fl.system.sd+.cpp.o` via `src/fl/build/fl.system.sd+.cpp`.
//
// This split lets the linker tree-shake the entire SD chain (libSD.a,
// libFS.a, Arduino's VFSImpl, the printf engine VFSFileImpl drags in
// via snprintf) AUTOMATICALLY when the user never calls
// `FileSystem::beginSd()` -- no FASTLED_USE_SDCARD opt-in required, and
// the macro that the earlier macro-gate PR (#2778 v1) shipped is
// removed by this PR. See FastLED #2773 item 1.2 and the SD TU header
// for the mechanism.
//
// All other FileSystem methods (begin, openRead, readText, ...) and the
// NullFileSystem stub stay here, so existing sketches that use only
// `FileSystem::begin(platform_filesystem)` are unaffected.

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
    NullFileHandle() FL_NO_EXCEPT = default;
    ~NullFileHandle() FL_NO_EXCEPT override {}

    bool is_open() const FL_NO_EXCEPT override { return false; }
    fl::size_t size() const FL_NO_EXCEPT override { return 0; }
    fl::size_t read(char *dst, fl::size_t bytesToRead) FL_NO_EXCEPT override {
        FASTLED_UNUSED(dst);
        FASTLED_UNUSED(bytesToRead);
        return 0;
    }
    fl::size_t write(const char *data, fl::size_t count) FL_NO_EXCEPT override {
        FASTLED_UNUSED(data);
        FASTLED_UNUSED(count);
        return 0;
    }
    fl::size_t tell() FL_NO_EXCEPT override { return 0; }
    const char *path() const FL_NO_EXCEPT override { return "nullptr filebuf"; }
    bool seek(fl::size_t pos, seek_dir dir) FL_NO_EXCEPT override {
        FASTLED_UNUSED(pos);
        FASTLED_UNUSED(dir);
        return false;
    }
    using filebuf::seek; // single-arg overload
    void close() FL_NO_EXCEPT override {}
    bool is_eof() const FL_NO_EXCEPT override { return true; }
    bool has_error() const FL_NO_EXCEPT override { return false; }
    void clear_error() FL_NO_EXCEPT override {}
    int error_code() const FL_NO_EXCEPT override { return 0; }
    const char *error_message() const FL_NO_EXCEPT override { return "NullFileHandle"; }
};

class NullFileSystem : public FsImpl {
  public:
    NullFileSystem() FL_NO_EXCEPT {
        FL_WARN("NullFileSystem instantiated as a placeholder, please "
                     "implement a file system for your platform.");
    }
    ~NullFileSystem() FL_NO_EXCEPT override {}

    bool begin() FL_NO_EXCEPT override { return true; }
    void end() FL_NO_EXCEPT override {}

    filebuf_ptr openRead(const char *_path) FL_NO_EXCEPT override {
        FASTLED_UNUSED(_path);
        fl::shared_ptr<NullFileHandle> ptr = fl::make_shared<NullFileHandle>();
        filebuf_ptr out = ptr;
        return out;
    }
};


// FileSystem::beginSd() is intentionally NOT defined in this TU. The
// definition lives in `fl/fs/sd/file_system_sd.cpp.hpp` which is
// compiled into its own `.o` (`fl.system.sd+.cpp.o`). The linker only
// pulls that `.o` when the user actually calls `fs.beginSd(...)`,
// keeping all SD library code (~15 KB on ESP32-S3) out of sketches that
// don't use it. See FastLED #2773 item 1.2.

bool FileSystem::begin(FsImplPtr platform_filesystem) FL_NO_EXCEPT {
    mFs = platform_filesystem;
    if (!mFs) {
        return false;
    }
    mFs->begin();
    return true;
}

Fled FileSystem::loadFled(const char *path) FL_NO_EXCEPT {
    return Fled::load(*this, path);
}

FileSystem::FileSystem() FL_NO_EXCEPT : mFs() {}

void FileSystem::end() FL_NO_EXCEPT {
    if (mFs) {
        mFs->end();
    }
}

bool FileSystem::readJson(const char *path, json *doc) FL_NO_EXCEPT {
    string text;
    if (!readText(path, &text)) {
        return false;
    }
    
    // Parse using the new json class
    *doc = fl::json::parse(text);
    return !doc->is_null();
}

bool FileSystem::readScreenMaps(const char *path,
                                fl::flat_map<string, ScreenMap> *out, string *error) FL_NO_EXCEPT {
    string text;
    if (!readText(path, &text)) {
        FL_WARN("Failed to read file: " << path);
        if (error) {
            *error = "Failed to read file: ";
            error->append(path);
        }
        return false;
    }
    string err;
    bool ok = ScreenMap::ParseJson(text.c_str(), out, &err);
    if (!ok) {
        FL_WARN("Failed to parse screen map: " << err.c_str());
        *error = err;
        return false;
    }
    return true;
}

bool FileSystem::readScreenMap(const char *path, const char *name,
                               ScreenMap *out, string *error) FL_NO_EXCEPT {
    string text;
    if (!readText(path, &text)) {
        FL_WARN("Failed to read file: " << path);
        if (error) {
            *error = "Failed to read file: ";
            error->append(path);
        }
        return false;
    }
    string err;
    bool ok = ScreenMap::ParseJson(text.c_str(), name, out, &err);
    if (!ok) {
        FL_WARN("Failed to parse screen map: " << err.c_str());
        *error = err;
        return false;
    }
    return true;
}

fl::ifstream FileSystem::openRead(const char *path) FL_NO_EXCEPT {
    if (!mFs) {
        // Defensive: default-constructed FileSystem or one whose begin*()
        // call failed has a null backend. Returning a closed ifstream
        // lets downstream code branch on is_open() rather than crash.
        return fl::ifstream();
    }
    return fl::ifstream(mFs->openRead(path));
}
Video FileSystem::openVideo(const char *path, fl::size pixelsPerFrame, float fps,
                            fl::size nFrameHistory) FL_NO_EXCEPT {
    Video video(pixelsPerFrame, fps, nFrameHistory);
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        video.setError(fl::string("Could not open file: ").append(path));
        return video;
    }
    video.begin(file.rdbuf());
    return video;
}

bool FileSystem::readText(const char *path, fl::string *out) FL_NO_EXCEPT {
    fl::ifstream file = openRead(path);
    if (!file.is_open()) {
        FL_WARN("Failed to open file: " << path);
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
    FL_DBG_IF(!wrote, "Failed to write any data to the output string.");
    return wrote;
}


// Convenience wrapper: mount an SD card and decode a JPEG in one call.
// Out of line because the API header wraps, it does not implement
// (agents/docs/cpp-standards.md -> API Object Pattern, rule 2; FastLED #4003).
FramePtr loadJpegFromSD(int cs_pin, const char *filepath,
                        const JpegConfig &config,
                        fl::string *error_message) FL_NO_EXCEPT {
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

// `make_sdcard_filesystem(int cs_pin)` is defined in the separate SD TU
// (`fl/fs/sd/file_system_sd.cpp.hpp`). When the SD TU is not linked
// (the user never calls `fs.beginSd()`), the symbol is also dead-stripped
// alongside `FileSystem::beginSd` itself.

#include "fl/file_system.h"
#include "fl/warn.h"


#ifdef __EMSCRIPTEN__
#include "platforms/wasm/fs_wasm.h"
#elif __has_include(<SD.h>)
// work in progress.
//#include "platforms/fs_sdcard_arduino.hpp"
#endif

#include "namespace.h"
#include "fl/json.h"


namespace fl {

class NullFileHandle : public FileHandle {
  public:
    NullFileHandle() = default;
    ~NullFileHandle() override {}

    bool available() const override { return false; }
    size_t size() const override { return 0; }
    size_t read(uint8_t *dst, size_t bytesToRead) override { return 0; }
    size_t pos() const override { return 0; }
    const char *path() const override { return "NULL FILE HANDLE"; }
    bool seek(size_t pos) override { return false; }
    void close() override {}
    bool valid() const override {
        FASTLED_WARN("NullFileHandle is not valid");
        return false;
    }
};

class NullFileSystem : public FsImpl {
  public:
    NullFileSystem() {
        FASTLED_WARN("NullFileSystem instantiated as a placeholder, please implement a file system for your platform.");
    }
    ~NullFileSystem() override {}

    bool begin() override { return true; }
    void end() override {}

    void close(FileHandlePtr file) override {
        // No need to do anything for in-memory files
        FASTLED_WARN("NullFileSystem::close");
    }

    FileHandlePtr openRead(const char *_path) override {
        FileHandlePtr out = FileHandlePtr::TakeOwnership(new NullFileHandle());
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

size_t FileHandle::bytesLeft() const { return size() - pos(); }

FileSystem::FileSystem() : mFs() {}


void FileSystem::end() {
    if (mFs) {
        mFs->end();
    }
}

bool FileSystem::readJson(const char *path, JsonDocument* doc, fl::Str* error) {
    return false;
}

void FileSystem::close(FileHandlePtr file) { mFs->close(file); }

FileHandlePtr FileSystem::openRead(const char *path) { return mFs->openRead(path); }
Video FileSystem::openVideo(const char *path, size_t pixelsPerFrame, float fps, size_t nFrameHistory) {
    Video video;
    FileHandlePtr file = openRead(path);
    if (!file) {
        video.setError(fl::Str("Could not open file: ") << path);
        return video;
    }
    video.begin(file, pixelsPerFrame, fps, nFrameHistory);
    return video;
}

bool FileSystem::readText(const char *path, fl::Str* out) {
    FileHandlePtr file = openRead(path);
    if (!file) {
        return false;
    }
    size_t size = file->size();
    out->reserve(size + out->size());
    bool wrote = false;
    while (file->available()) {
        uint8_t buf[64];
        size_t n = file->read(buf, sizeof(buf));
        // out->append(buf, n);
        out->append((const char*)buf, n);
        wrote = true;
    }
    file->close();
    return wrote;
}
}  // namespace fl

namespace fl {
__attribute__((weak)) FsImplPtr make_sdcard_filesystem(int cs_pin) {
    FsImplPtr out = FsImplPtr::TakeOwnership(new NullFileSystem());
    return out;
}

}  // namespace fl


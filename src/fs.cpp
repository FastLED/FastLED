#include "fs.h"


#ifdef __EMSCRIPTEN__
#include "platforms/wasm/fs_wasm.h"
#elif __has_include(<SD.h>)
// work in progress.
//#include "platforms/fs_sdcard_arduino.hpp"
#endif

#include "namespace.h"


FASTLED_NAMESPACE_BEGIN

// Override this if you want to supply a file system for your platform.
__attribute__((weak)) FsImplPtr make_filesystem(int cs_pin) {
    return FsImplPtr::Null();
}

bool Fs::begin() {
    if (!mFs) {
        return false;
    }
    mFs->begin();
    return true;
}

size_t FileHandle::bytesLeft() const { return size() - pos(); }

Fs::Fs(int cs_pin) : mFs(make_filesystem(cs_pin)) {}

Fs::Fs(FsImplPtr fs) : mFs(fs) {}

void Fs::end() {
    if (mFs) {
        mFs->end();
    }
}

void Fs::close(FileHandlePtr file) { mFs->close(file); }

FileHandlePtr Fs::openRead(const char *path) { return mFs->openRead(path); }

FASTLED_NAMESPACE_END

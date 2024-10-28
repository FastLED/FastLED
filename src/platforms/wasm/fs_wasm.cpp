
#ifdef __EMSCRIPTEN__


#include "ptr.h"
#include "fs.h"
#include <map>
#include <mutex>
#include <vector>
#include "str.h"
#include "math_macros.h"
#include "namespace.h"
#include "warn.h"
#include <mutex>

#include <emscripten.h>


FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(FsImplWasm);
DECLARE_SMART_PTR(WasmFileHandle);


namespace {
    // Map is great because it doesn't invalidate it's data members unless erase is called.
    typedef std::map<Str, std::vector<uint8_t>> FileMap;
    FileMap gFileMap;
    // At the time of creation, it's unclear whether this can be called by multiple threads.
    // With an std::map items remain valid while not erased. So we only need to protect the map itself
    // for thread safety. The values in the map are safe to access without a lock.
    std::mutex gFileMapMutex;
}  // namespace



class WasmFileHandle : public FileHandle {
private:
    const std::vector<uint8_t>* mData;
    size_t mPos;
    Str mPath;

public:
    // The std::vector is add only, it wil never be destroyed, however it could theoretically
    // be resized.
    WasmFileHandle(const Str& path, const std::vector<uint8_t>* data)
        : mPath(path), mData(data), mPos(0) {}

    virtual ~WasmFileHandle() override {}

    bool available() const override { return mPos < mData->size(); }
    size_t bytesLeft() const override { return mData->size() - mPos; }
    size_t size() const override { return mData->size(); }

    size_t read(uint8_t *dst, size_t bytesToRead) override {
        size_t bytesAvailable = bytesLeft();
        size_t bytesToActuallyRead = MIN(bytesToRead, bytesAvailable);
        std::copy(mData->begin() + mPos, mData->begin() + mPos + bytesToActuallyRead, dst);
        mPos += bytesToActuallyRead;
        return bytesToActuallyRead;
    }

    size_t pos() const override { return mPos; }
    const char* path() const override { return mPath.c_str(); }

    void seek(size_t pos) override {
        mPos = MIN(pos, mData->size());
    }

    void close() override {
        // No need to do anything for in-memory files
    }
};

class FsImplWasm : public FsImpl {
public:
    FsImplWasm() = default;
    ~FsImplWasm() override {}

    bool begin() override { return true; }
    void end() override {}

    void close(FileHandlePtr file) override {
        if (file) {
            file->close();
        }
    }

    FileHandlePtr openRead(const char *path) override {
        Str key(path);
        std::lock_guard<std::mutex> lock(gFileMapMutex);
        auto it = gFileMap.find(key);
        if (it != gFileMap.end()) {
            WasmFileHandlePtr out;
            out = WasmFileHandlePtr::TakeOwnership(new WasmFileHandle(key, &(it->second)));
            return out;
        }
        return FileHandlePtr::Null();
    }
};

// Platforms eed to implement this to create an instance of the filesystem.
FsImplPtr make_filesystem(int cs_pin) {
    return FsImplWasmPtr::New();
}

FASTLED_NAMESPACE_END

extern "C" {

EMSCRIPTEN_KEEPALIVE void jsInjectFile(const char* path, const uint8_t* data, size_t len) {
    //gFileMap[Str(path)] = std::vector<uint8_t>(data, data + len);
    // lock guard
    Str path_str(path);
    auto new_data = std::vector<uint8_t>(data, data + len);
    // Lock may not be necessary.
    std::lock_guard<std::mutex> lock(gFileMapMutex);
    auto it = gFileMap.find(path_str);
    if (it != gFileMap.end()) {
        FASTLED_WARN("File can only be injected once.");
    } else {
        gFileMap.insert(std::make_pair(path_str, new_data));
    }
}

}


#endif  // __EMSCRIPTEN__

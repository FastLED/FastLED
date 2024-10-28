
#ifdef __EMSCRIPTEN__

#include "fs.h"
#include "math_macros.h"
#include "namespace.h"
#include "ptr.h"
#include "str.h"
#include "warn.h"
#include <map>
#include <mutex>
#include <vector>

#include <emscripten.h>

FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(FsImplWasm);
DECLARE_SMART_PTR(WasmFileHandle);

namespace {
// Map is great because it doesn't invalidate it's data members unless erase is
// called.
struct FileData {
    std::vector<uint8_t> data;
    size_t len = 0;
    FileData(size_t len) : len(len) {}
    FileData(const std::vector<uint8_t> &data, size_t len)
        : data(data), len(len) {}
    FileData() = default;
};

typedef std::map<Str, FileData> FileMap;
FileMap gFileMap;
// At the time of creation, it's unclear whether this can be called by multiple
// threads. With an std::map items remain valid while not erased. So we only
// need to protect the map itself for thread safety. The values in the map are
// safe to access without a lock.
std::mutex gFileMapMutex;
} // namespace

class WasmFileHandle : public FileHandle {
  private:
    const std::vector<uint8_t> *mData;
    size_t mPos;
    Str mPath;

  public:
    // The std::vector is add only, it wil never be destroyed, however it could
    // theoretically be resized.
    WasmFileHandle(const Str &path, const std::vector<uint8_t> *data)
        : mPath(path), mData(data), mPos(0) {}

    virtual ~WasmFileHandle() override {}

    bool available() const override { return mPos < mData->size(); }
    size_t bytesLeft() const override { return mData->size() - mPos; }
    size_t size() const override { return mData->size(); }

    size_t read(uint8_t *dst, size_t bytesToRead) override {
        size_t bytesAvailable = bytesLeft();
        size_t bytesToActuallyRead = MIN(bytesToRead, bytesAvailable);
        std::copy(mData->begin() + mPos,
                  mData->begin() + mPos + bytesToActuallyRead, dst);
        mPos += bytesToActuallyRead;
        return bytesToActuallyRead;
    }

    size_t pos() const override { return mPos; }
    const char *path() const override { return mPath.c_str(); }

    void seek(size_t pos) override { mPos = MIN(pos, mData->size()); }

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
            auto& data = it->second;
            auto& vec = data.data;
            WasmFileHandlePtr out;
            out = WasmFileHandlePtr::TakeOwnership(new WasmFileHandle(key, &vec));
            return out;
        }
        return FileHandlePtr::Null();
    }
};

// Platforms eed to implement this to create an instance of the filesystem.
FsImplPtr make_filesystem(int cs_pin) { return FsImplWasmPtr::New(); }

FASTLED_NAMESPACE_END

extern "C" {

EMSCRIPTEN_KEEPALIVE bool jsInjectFile(const char *path, const uint8_t *data,
                                       size_t len) {
    Str path_str(path);
    auto entry = FileData(len);
    entry.data.insert(entry.data.end(), data, data + len);
    // Lock may not be necessary. We don't know how this is going to be called exactly yet.
    {
        std::lock_guard<std::mutex> lock(gFileMapMutex);
        auto it = gFileMap.find(path_str);
        if (it != gFileMap.end()) {
            FASTLED_WARN("File can only be injected once.");
            return false;
        } else {
            auto it = gFileMap.insert(std::make_pair(path_str, FileData(len)));
            std::swap(it.first->second.data, entry.data);
            return true;
        }
    }
}

EMSCRIPTEN_KEEPALIVE bool jsAppendFile(const char *path, const uint8_t *data,
                                       size_t len) {
    Str path_str(path);
    {
        std::lock_guard<std::mutex> lock(gFileMapMutex);
        auto it = gFileMap.find(path_str);
        if (it != gFileMap.end()) {
            auto* file_data = &(it->second.data);
            file_data->insert(file_data->end(), data, data + len);
            return true;
        } else {
            FASTLED_WARN("File must be declared before appending.");
            return false;
        }
    }    
}

EMSCRIPTEN_KEEPALIVE bool jsDeclareFile(const char *path, size_t len) {
    // declare a file and it's length. But don't fill it in yet
    std::lock_guard<std::mutex> lock(gFileMapMutex);
    auto it = gFileMap.find(path);
    if (it != gFileMap.end()) {
        FASTLED_WARN("File can only be declared once.");
        return false;
    } else {
        gFileMap.insert(std::make_pair(path, FileData(len)));
        return true;
    }
}


}  // extern "C"

#endif // __EMSCRIPTEN__

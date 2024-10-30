
#ifdef __EMSCRIPTEN__


#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>


#include <map>
#include <mutex>
#include <vector>
#include <stdio.h>


#include "file_system.h"
#include "math_macros.h"
#include "namespace.h"
#include "ref.h"
#include "str.h"
#include "json.h"
#include "warn.h"


FASTLED_NAMESPACE_BEGIN

FASTLED_SMART_REF(FsImplWasm);
FASTLED_SMART_REF(WasmFileHandle);

namespace {
// Map is great because it doesn't invalidate it's data members unless erase is
// called.
FASTLED_SMART_REF(FileData);

class FileData : public Referent {
  public:

    FileData(size_t capacity) : mCapacity(capacity) {}
    FileData(const std::vector<uint8_t> &data, size_t len)
        : mData(data), mCapacity(len) {}
    FileData() = default;

    void append(const uint8_t *data, size_t len) {
        std::lock_guard<std::mutex> lock(mMutex);
        mData.insert(mData.end(), data, data + len);
        mCapacity = MAX(mCapacity, mData.size());
    }

    size_t read(size_t pos, uint8_t *dst, size_t len) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (pos >= mData.size()) {
            return 0;
        }
        size_t bytesAvailable = mData.size() - pos;
        size_t bytesToActuallyRead = MIN(len, bytesAvailable);
        std::copy(mData.begin() + pos, mData.begin() + pos + bytesToActuallyRead,
                  dst);
        return bytesToActuallyRead;
    }

    size_t capacity() const {
        std::lock_guard<std::mutex> lock(mMutex);
        return mCapacity;
    }
private:
    std::vector<uint8_t> mData;
    size_t mCapacity = 0;
    mutable std::mutex mMutex;
};

typedef std::map<Str, FileDataRef> FileMap;
FileMap gFileMap;
// At the time of creation, it's unclear whether this can be called by multiple
// threads. With an std::map items remain valid while not erased. So we only
// need to protect the map itself for thread safety. The values in the map are
// safe to access without a lock.
std::mutex gFileMapMutex;
} // namespace

class WasmFileHandle : public FileHandle {
  private:
    FileDataRef mData;
    size_t mPos;
    Str mPath;

  public:
    WasmFileHandle(const Str &path, const FileDataRef data)
        : mPath(path), mData(data), mPos(0) {}

    virtual ~WasmFileHandle() override {}

    bool available() const override { return mPos < mData->capacity(); }
    size_t bytesLeft() const override { return mData->capacity() - mPos; }
    size_t size() const override { return mData->capacity(); }

    size_t read(uint8_t *dst, size_t bytesToRead) override {
        return mData->read(mPos, dst, bytesToRead);
    }

    size_t pos() const override { return mPos; }
    const char *path() const override { return mPath.c_str(); }

    void seek(size_t pos) override { mPos = MIN(pos, mData->capacity()); }

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

    void close(FileHandleRef file) override {
        printf("Closing file %s\n", file->path());
        if (file) {
            file->close();
        }
    }

    FileHandleRef openRead(const char *_path) override {
        printf("Opening file %s\n", _path);
        Str path(_path);
        std::lock_guard<std::mutex> lock(gFileMapMutex);
        auto it = gFileMap.find(path);
        if (it != gFileMap.end()) {
            auto &data = it->second;
            WasmFileHandleRef out =
                WasmFileHandleRef::TakeOwnership(new WasmFileHandle(path, data));
            return out;
        }
        return FileHandleRef::Null();
    }
};

// Platforms eed to implement this to create an instance of the filesystem.
FsImplRef make_filesystem(int cs_pin) { return FsImplWasmRef::New(); }


FileDataRef _findIfExists(const Str& path) {
    std::lock_guard<std::mutex> lock(gFileMapMutex);
    auto it = gFileMap.find(path);
    if (it != gFileMap.end()) {
        return it->second;
    }
    return FileDataRef::Null();
}

FileDataRef _findOrCreate(const Str& path, size_t len) {
    std::lock_guard<std::mutex> lock(gFileMapMutex);
    auto it = gFileMap.find(path);
    if (it != gFileMap.end()) {
        return it->second;
    }
    auto entry = FileDataRef::New(len);
    gFileMap.insert(std::make_pair(path, entry));
    return entry;
}

FileDataRef _createIfNotExists(const Str& path, size_t len) {
    std::lock_guard<std::mutex> lock(gFileMapMutex);
    auto it = gFileMap.find(path);
    if (it != gFileMap.end()) {
        return FileDataRef::Null();
    }
    auto entry = FileDataRef::New(len);
    gFileMap.insert(std::make_pair(path, entry));
    return entry;
}

FASTLED_NAMESPACE_END


FASTLED_USING_NAMESPACE



extern "C" {


EMSCRIPTEN_KEEPALIVE bool jsInjectFile(const char *path, const uint8_t *data,
                                       size_t len) {

    auto inserted = _createIfNotExists(Str(path), len);
    if (!inserted) {
        FASTLED_WARN("File can only be injected once.");
        return false;
    }
    inserted->append(data, len);
    return true;
}

EMSCRIPTEN_KEEPALIVE bool jsAppendFile(const char *path, const uint8_t *data,
                                       size_t len) {
    printf("Appending file %s with %lu bytes\n", path, len);
    auto entry = _findIfExists(Str(path));
    if (!entry) {
        FASTLED_WARN("File must be declared before it can be appended.");
        return false;
    }
    entry->append(data, len);
    return true;
}

EMSCRIPTEN_KEEPALIVE bool jsDeclareFile(const char *path, size_t len) {
    // declare a file and it's length. But don't fill it in yet
    auto inserted = _createIfNotExists(Str(path), len);
    if (!inserted) {
        FASTLED_WARN("File can only be declared once.");
        return false;
    }
    return true;
}


EMSCRIPTEN_KEEPALIVE void fastled_declare_files(std::string jsonStr) {
    FLArduinoJson::JsonDocument doc;
    FLArduinoJson::deserializeJson(doc, jsonStr);
    auto files = doc["files"];
    if (files.isNull()) {
        return;
    }
    auto files_array = files.as<FLArduinoJson::JsonArray>();
    if (files_array.isNull()) {
        return;
    }

    for (auto file : files_array) {
        auto size_obj = file["size"];
        if (size_obj.isNull()) {
            continue;
        }
        auto size = size_obj.as<int>();
        auto path_obj = file["path"];
        if (path_obj.isNull()) {
            continue;
        }
        printf("Declaring file %s with size %d. These will become available as File system paths within the app.\n", path_obj.as<const char*>(), size);
        jsDeclareFile(path_obj.as<const char*>(), size);
    }
}



} // extern "C"




EMSCRIPTEN_BINDINGS(_fastled_declare_files) {
    emscripten::function("_fastled_declare_files", &fastled_declare_files);
    //emscripten::function("jsAppendFile", emscripten::select_overload<bool(const char*, const uint8_t*, size_t)>(&jsAppendFile), emscripten::allow_raw_pointer<arg<0>, arg<1>>());
};

#endif // __EMSCRIPTEN__

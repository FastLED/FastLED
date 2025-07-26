
#ifdef __EMSCRIPTEN__

// ⚠️⚠️⚠️ CRITICAL WARNING: C++ ↔ JavaScript FILE SYSTEM BRIDGE - HANDLE WITH EXTREME CARE! ⚠️⚠️⚠️
//
// 🚨 THIS FILE CONTAINS C++ TO JAVASCRIPT FILE SYSTEM BINDINGS 🚨
//
// DO NOT MODIFY FUNCTION SIGNATURES WITHOUT UPDATING CORRESPONDING JAVASCRIPT CODE!
//
// This file manages file system operations between C++ and JavaScript for WASM builds.
// Any changes to:
// - EMSCRIPTEN_BINDINGS macro contents
// - extern "C" EMSCRIPTEN_KEEPALIVE function signatures
// - fastled_declare_files() parameter types
// - File operation function names or parameters
//
// Will BREAK JavaScript file loading and cause SILENT RUNTIME FAILURES!
//
// Key integration points that MUST remain synchronized:
// - EMSCRIPTEN_BINDINGS(_fastled_declare_files)
// - fastled_declare_files(std::string jsonStr) 
// - extern "C" jsInjectFile(), jsAppendFile(), jsDeclareFile()
// - JavaScript Module._fastled_declare_files() calls
// - JSON file declaration format parsing
//
// Before making ANY changes:
// 1. Understand this affects file loading for animations and data
// 2. Test with real WASM builds that load external files
// 3. Verify JSON parsing for file declarations works correctly
// 4. Check that file operations remain accessible from JavaScript
//
// ⚠️⚠️⚠️ REMEMBER: File system errors prevent resource loading! ⚠️⚠️⚠️

#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/val.h>

#include <map>
#include <mutex>
#include <stdio.h>
#include <vector>

#include "fl/dbg.h"
#include "fl/file_system.h"
#include "fl/json.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/memory.h"
#include "fl/str.h"
#include "fl/warn.h"
#include "fl/mutex.h"
#include "platforms/wasm/js.h"


namespace fl {

FASTLED_SMART_PTR(FsImplWasm);
FASTLED_SMART_PTR(WasmFileHandle);

// Map is great because it doesn't invalidate it's data members unless erase is
// called.
FASTLED_SMART_PTR(FileData);

class FileData {
  public:
    FileData(size_t capacity) : mCapacity(capacity) { mData.reserve(capacity); }
    FileData(const std::vector<uint8_t> &data, size_t len)
        : mData(data), mCapacity(len) {}
    FileData() = default;

    void append(const uint8_t *data, size_t len) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        mData.insert(mData.end(), data, data + len);
        mCapacity = MAX(mCapacity, mData.size());
    }

    size_t read(size_t pos, uint8_t *dst, size_t len) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        if (pos >= mData.size()) {
            return 0;
        }
        size_t bytesAvailable = mData.size() - pos;
        size_t bytesToActuallyRead = MIN(len, bytesAvailable);
        auto begin_it = mData.begin() + pos;
        auto end_it = begin_it + bytesToActuallyRead;
        std::copy(begin_it, end_it, dst);
        return bytesToActuallyRead;
    }

    bool ready(size_t pos) {
        fl::lock_guard<fl::mutex> lock(mMutex);
        return mData.size() == mCapacity || pos < mData.size();
    }

    size_t bytesRead() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        return mData.size();
    }

    size_t capacity() const {
        fl::lock_guard<fl::mutex> lock(mMutex);
        return mCapacity;
    }

  private:
    std::vector<uint8_t> mData;
    size_t mCapacity = 0;
    mutable fl::mutex mMutex;
};

typedef std::map<Str, FileDataPtr> FileMap;
static FileMap gFileMap;
// At the time of creation, it's unclear whether this can be called by multiple
// threads. With an std::map items remain valid while not erased. So we only
// need to protect the map itself for thread safety. The values in the map are
// safe to access without a lock.
static fl::mutex gFileMapMutex;

class WasmFileHandle : public fl::FileHandle {
  private:
    FileDataPtr mData;
    size_t mPos;
    Str mPath;

  public:
    WasmFileHandle(const Str &path, const FileDataPtr data)
        : mData(data), mPos(0), mPath(path) {}

    virtual ~WasmFileHandle() override {}

    bool available() const override {
        if (mPos >= mData->capacity()) {
            return false;
        }
        if (!mData->ready(mPos)) {
            FASTLED_WARN("File is not ready yet. This is a major error because "
                         "FastLED-wasm does not support async yet, the file "
                         "will fail to read.");
            return false;
        }
        return true;
    }
    size_t bytesLeft() const override {
        if (!available()) {
            return 0;
        }
        return mData->capacity() - mPos;
    }
    size_t size() const override { return mData->capacity(); }

    size_t read(uint8_t *dst, size_t bytesToRead) override {
        if (mPos >= mData->capacity()) {
            return 0;
        }
        if (mPos + bytesToRead > mData->capacity()) {
            bytesToRead = mData->capacity() - mPos;
        }
        if (!mData->ready(mPos)) {
            FASTLED_WARN("File is not ready yet. This is a major error because "
                         "FastLED-wasmdoes not support async yet, the file "
                         "will fail to read.");
            return 0;
        }
        // We do not have async so a delay will actually block the entire wasm
        // main thread. while (!mData->ready(mPos)) {
        //     delay(1);
        // }
        size_t bytesRead = mData->read(mPos, dst, bytesToRead);
        mPos += bytesRead;
        return bytesRead;
    }

    size_t pos() const override { return mPos; }
    const char *path() const override { return mPath.c_str(); }

    bool seek(size_t pos) override {
        if (pos > mData->capacity()) {
            return false;
        }
        mPos = pos;
        return true;
    }

    void close() override {
        // No need to do anything for in-memory files
    }

    bool valid() const override {
        return true;
    } // always valid if we can open a file.
};

class FsImplWasm : public fl::FsImpl {
  public:
    FsImplWasm() = default;
    ~FsImplWasm() override {}

    bool begin() override { return true; }
    void end() override {}

    void close(FileHandlePtr file) override {
        printf("Closing file %s\n", file->path());
        if (file) {
            file->close();
        }
    }

    fl::FileHandlePtr openRead(const char *_path) override {
        // FASTLED_DBG("Opening file: " << _path);
        Str path(_path);
        FileHandlePtr out;
        {
            fl::lock_guard<fl::mutex> lock(gFileMapMutex);
            auto it = gFileMap.find(path);
            if (it != gFileMap.end()) {
                auto &data = it->second;
                out = fl::make_shared<WasmFileHandle>(path, data);
                // FASTLED_DBG("Opened file: " << _path);
            } else {
                out = fl::FileHandlePtr();
                FASTLED_DBG("File not found: " << _path);
            }
        }
        return out;
    }
};

FileDataPtr _findIfExists(const Str &path) {
    fl::lock_guard<fl::mutex> lock(gFileMapMutex);
    auto it = gFileMap.find(path);
    if (it != gFileMap.end()) {
        return it->second;
    }
    return FileDataPtr();
}

FileDataPtr _findOrCreate(const Str &path, size_t len) {
    fl::lock_guard<fl::mutex> lock(gFileMapMutex);
    auto it = gFileMap.find(path);
    if (it != gFileMap.end()) {
        return it->second;
    }
    auto entry = fl::make_shared<FileData>(len);
    gFileMap.insert(std::make_pair(path, entry));
    return entry;
}

FileDataPtr _createIfNotExists(const Str &path, size_t len) {
    fl::lock_guard<fl::mutex> lock(gFileMapMutex);
    auto it = gFileMap.find(path);
    if (it != gFileMap.end()) {
        return FileDataPtr();
    }
    auto entry = fl::make_shared<FileData>(len);
    gFileMap.insert(std::make_pair(path, entry));
    return entry;
}

} // namespace fl

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

EMSCRIPTEN_KEEPALIVE void fastled_declare_files(const char* jsonStr) {
    fl::Json doc = fl::Json::parse(fl::string(jsonStr));
    if (!doc.is_object() || !doc.contains("files")) {
        return;
    }
    
    auto files = doc["files"];
    if (!files.is_array()) {
        return;
    }
    
    size_t fileCount = files.size();
    for (size_t i = 0; i < fileCount; i++) {
        auto file = files[i];
        if (!file.is_object()) {
            continue;
        }
        
        if (!file.contains("size") || !file.contains("path")) {
            continue;
        }
        
        int size = file["size"] | 0;
        fl::string path = file["path"] | fl::string("");
        
        if (size > 0 && !path.empty()) {
            printf("Declaring file %s with size %d. These will become available as "
                   "File system paths within the app.\n",
                   path.c_str(), size);
            jsDeclareFile(path.c_str(), size);
        }
    }
}

} // extern "C"



namespace fl {
// Platforms eed to implement this to create an instance of the filesystem.
FsImplPtr make_sdcard_filesystem(int cs_pin) { return fl::make_shared<FsImplWasm>(); }
} // namespace fl

#endif // __EMSCRIPTEN__

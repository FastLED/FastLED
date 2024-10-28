
#ifdef __EMSCRIPTEN__


#include "ptr.h"
#include "fs.h"
#include <map>
#include <mutex>
#include <vector>
#include "str.h"
#include "namespace.h"
#include "warn.h"
#include <mutex>


FASTLED_NAMESPACE_BEGIN

DECLARE_SMART_PTR(FsImplWasm);


namespace {
    // Map is great because it doesn't invalidate it's data members unless erase is called.
    typedef std::map<Str, std::vector<uint8_t>> FileMap;
    FileMap gFileMap;
    // At the time of creation, it's unclear whether this can be called by multiple threads.
    // With an std::map items remain valid while not erased. So we only need to protect the map itself
    // for thread safety. The values in the map are safe to access without a lock.
    std::mutex gFileMapMutex;
}  // namespace

void jsInjectFile(const char* path, const uint8_t* data, size_t len) {
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

class FsImplWasm : public FsImpl {
  public:
    FsImplWasm() = default;
    ~FsImplWasm() override {}

    bool begin() override { return true; }
    void end() override {}
    void close(FileHandlePtr file) override {}
    FileHandlePtr openRead(const char *path) override { return FileHandlePtr::Null(); }
};

// Platforms eed to implement this to create an instance of the filesystem.
FsImplPtr make_filesystem(int cs_pin) {
    return FsImplWasmPtr::New();
}


FASTLED_NAMESPACE_END

#endif  // __EMSCRIPTEN__

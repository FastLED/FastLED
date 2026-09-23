// IWYU pragma: private


#include "platforms/wasm/is_wasm.h"
#ifdef FL_IS_WASM

// âš ï¸âš ï¸âš ï¸ CRITICAL WARNING: C++ â†” JavaScript FILE SYSTEM BRIDGE - HANDLE WITH EXTREME CARE! âš ï¸âš ï¸âš ï¸
//
// ðŸš¨ THIS FILE CONTAINS C++ TO JAVASCRIPT FILE SYSTEM BINDINGS ðŸš¨
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
// âš ï¸âš ï¸âš ï¸ REMEMBER: File system errors prevent resource loading! âš ï¸âš ï¸âš ï¸

// IWYU pragma: begin_keep
#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/val.h>
// IWYU pragma: end_keep

// IWYU pragma: begin_keep
#include "fl/stl/flat_map.h"  // ok include
#include "fl/stl/mutex.h"  // ok include
#include "fl/stl/cstdio.h"  // ok include
#include "fl/stl/vector.h"  // ok include
// IWYU pragma: end_keep

#include "fl/log/log.h"
#include "fl/fled/detail/parser.h"
#include "fl/fs/fs.h"
#include "fl/stl/json.h"
#include "fl/math/math.h"
#include "fl/stl/memory.h"
#include "fl/stl/string.h"
#include "fl/stl/stdio.h"
#include "fl/log/log.h"
#include "fl/stl/mutex.h"
#include "platforms/wasm/js.h"
#include "platforms/wasm/fs_wasm_file_handle.h"


namespace fl {

FASTLED_SHARED_PTR(FsImplWasm);
typedef fl::flat_map<fl::string, FileDataPtr, fl::StringFastLess> FileMap;  // okay fl namespace

struct FileRegistry {
    FileMap files;
    fl::mutex mutex;
    static FileRegistry &instance() {
        static FileRegistry registry;
        return registry;
    }
private:
    FileRegistry() = default;
};

class FsImplWasm : public fl::FsImpl {
  public:
    FsImplWasm() = default;
    ~FsImplWasm() override {}

    bool begin() override { return true; }
    void end() override {}

    fl::filebuf_ptr openRead(const char *_path) override {
        // FL_DBG("Opening file: " << _path);
        string path(_path);
        filebuf_ptr out;
        {
            auto &reg = FileRegistry::instance();
            fl::unique_lock<fl::mutex> lock(reg.mutex);
            auto it = reg.files.find(path);
            if (it != reg.files.end()) {
                auto &data = it->second;
                out = fl::make_shared<WasmFileHandle>(path, data);
                // FL_DBG("Opened file: " << _path);
            } else {
                out = fl::filebuf_ptr();
                FL_DBG_F("File not found: %s", _path);
            }
        }
        return out;
    }
};

FileDataPtr _findIfExists(const fl::string &path) {
    auto &reg = FileRegistry::instance();
    fl::unique_lock<fl::mutex> lock(reg.mutex);
    auto it = reg.files.find(path);
    if (it != reg.files.end()) {
        return it->second;
    }
    return FileDataPtr();
}

FileDataPtr _findOrCreate(const fl::string &path, size_t len) {
    auto &reg = FileRegistry::instance();
    fl::unique_lock<fl::mutex> lock(reg.mutex);
    auto it = reg.files.find(path);
    if (it != reg.files.end()) {
        return it->second;
    }
    auto entry = fl::make_shared<FileData>(len);
    reg.files.insert(fl::make_pair(path, entry));  // okay fl namespace
    return entry;
}

FileDataPtr _createIfNotExists(const fl::string &path, size_t len) {
    auto &reg = FileRegistry::instance();
    fl::unique_lock<fl::mutex> lock(reg.mutex);
    auto it = reg.files.find(path);
    if (it != reg.files.end()) {
        return FileDataPtr();
    }
    auto entry = fl::make_shared<FileData>(len);
    reg.files.insert(fl::make_pair(path, entry));  // okay fl namespace
    return entry;
}

// Dump a .fled file's JSON metadata envelope to the browser console.
//
// Files arrive here either whole (jsInjectFile) or in chunks (jsDeclareFile +
// jsAppendFile), so this runs after every append and reports as soon as the
// header plus the declared json_length have landed - not when the whole
// payload has, which for a video is orders of magnitude later.
//
// printf is routed to console.log on this platform (see platforms/wasm/readme),
// so println() is the console. Anything that is not a .fled stays silent: this
// is a diagnostic for FLED bundles, not a log line for every asset.
void _logFledEnvelopeOnce(const fl::string &path, const FileDataPtr &entry) {
    if (!entry) {
        return;
    }

    static fl::vector<fl::string> sReported;
    static fl::mutex sReportedMutex;

    const size_t have = entry->bytesRead();
    if (have < 12) {
        return;  // header not complete yet
    }

    // json_length lives at offset 8 as u32 little-endian.
    fl::u8 header[12] = {0};
    if (entry->read(0, header, 12) != 12) {
        return;
    }
    if (header[0] != 'F' || header[1] != 'L' || header[2] != 'E' ||
        header[3] != 'D') {
        return;  // not a FLED container
    }
    const fl::u32 jsonLen =
        static_cast<fl::u32>(header[8]) |
        (static_cast<fl::u32>(header[9]) << 8) |
        (static_cast<fl::u32>(header[10]) << 16) |
        (static_cast<fl::u32>(header[11]) << 24);

    const size_t envelopeEnd = 12u + static_cast<size_t>(jsonLen);
    if (have < envelopeEnd) {
        return;  // envelope still streaming
    }

    {
        fl::unique_lock<fl::mutex> lock(sReportedMutex);
        for (size_t i = 0; i < sReported.size(); ++i) {
            if (sReported[i] == path) {
                return;  // already reported this file
            }
        }
        sReported.push_back(path);
    }

    fl::vector<fl::u8> buf;
    buf.resize(envelopeEnd);
    if (entry->read(0, buf.data(), envelopeEnd) != envelopeEnd) {
        return;
    }

    fl::fled::ParsedHeader hdr{};
    fl::json envelope;
    if (!fl::fled::parseHeaderAndEnvelope(buf.data(), buf.size(), &hdr,
                                          &envelope)) {
        fl::println("[FLED] malformed container, cannot read metadata");
        return;
    }

    fl::string out = "[FLED] ";
    out += path;
    out += " (v";
    out += fl::to_string(static_cast<int>(hdr.version));
    out += ", pixel_format 0x";
    const char kHex[] = "0123456789abcdef";
    char pf[3] = {kHex[(hdr.pixelFormat >> 4) & 0x0f],
                  kHex[hdr.pixelFormat & 0x0f], '\0'};
    out += pf;
    out += ") metadata: ";
    out += envelope.to_string();
    fl::println(out.c_str());
}

} // namespace fl

extern "C" {

EMSCRIPTEN_KEEPALIVE bool jsInjectFile(const char *path, const fl::u8 *data,
                                       size_t len) {

    auto inserted = fl::_createIfNotExists(fl::string(path), len);
    if (!inserted) {
        FL_WARN_F("File can only be injected once.");
        return false;
    }
    inserted->append(data, len);
    fl::_logFledEnvelopeOnce(fl::string(path), inserted);
    return true;
}

EMSCRIPTEN_KEEPALIVE bool jsAppendFile(const char *path, const fl::u8 *data,
                                       size_t len) {
    auto entry = fl::_findIfExists(fl::string(path));
    if (!entry) {
        FL_WARN_F("File must be declared before it can be appended.");
        return false;
    }
    entry->append(data, len);
    fl::_logFledEnvelopeOnce(fl::string(path), entry);
    return true;
}

EMSCRIPTEN_KEEPALIVE bool jsDeclareFile(const char *path, size_t len) {
    // declare a file and it's length. But don't fill it in yet
    auto inserted = fl::_createIfNotExists(fl::string(path), len);
    if (!inserted) {
        FL_WARN_F("File can only be declared once.");
        return false;
    }
    return true;
}

EMSCRIPTEN_KEEPALIVE void fastled_declare_files(const char* jsonStr) {
    fl::json doc = fl::json::parse(fl::string(jsonStr));
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
            fl::printf("Declaring file %s with size %d. These will become available as "
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

#endif // FL_IS_WASM

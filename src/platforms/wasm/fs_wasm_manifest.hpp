#ifndef FL_PLATFORMS_WASM_FS_WASM_MANIFEST_HPP
#define FL_PLATFORMS_WASM_FS_WASM_MANIFEST_HPP

// IWYU pragma: private

#include "fl/stl/json.h"
#include "fl/stl/string.h"

namespace fl {
namespace wasm {

// Kept separate from the Emscripten bridge so the manifest contract can be
// exercised by native tests. The callback receives only valid file entries.
template <typename DeclareFile>
void declareManifestFiles(const char *jsonStr, DeclareFile declareFile) {
    fl::json doc = fl::json::parse(fl::string(jsonStr));
    if (!doc.is_object() || !doc.contains("files")) {
        return;
    }

    auto files = doc["files"];
    if (!files.is_array()) {
        return;
    }

    for (size_t i = 0; i < files.size(); ++i) {
        auto file = files[i];
        if (!file.is_object() || !file.contains("size") ||
            !file.contains("path")) {
            continue;
        }

        int size = file["size"] | -1;
        fl::string path = file["path"] | fl::string("");
        if (size >= 0 && !path.empty()) {
            declareFile(path, static_cast<size_t>(size));
        }
    }
}

} // namespace wasm
} // namespace fl

#endif // FL_PLATFORMS_WASM_FS_WASM_MANIFEST_HPP

#include "fl/fs/fstream.h"
#include "platforms/wasm/fs_wasm_file_handle.h"
#include "platforms/wasm/fs_wasm_manifest.hpp"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("WASM manifest declares zero-byte files and rejects negative sizes") {
    int calls = 0;
    fl::string path;
    size_t size = 1;
    fl::FileDataPtr data;
    fl::wasm::declareManifestFiles(
        R"({"files":[{"path":"empty.bin","size":0},{"path":"bad.bin","size":-1}]})",
        [&](const fl::string &declaredPath, size_t declaredSize) {
            ++calls;
            path = declaredPath;
            size = declaredSize;
            data = fl::make_shared<fl::FileData>(declaredSize);
        });

    FL_REQUIRE_EQ(calls, 1);
    FL_CHECK_EQ(path, fl::string("empty.bin"));
    FL_CHECK_EQ(size, size_t(0));

    fl::ifstream file(fl::make_shared<fl::WasmFileHandle>(path, data));
    FL_CHECK_TRUE(file.is_open());
    FL_CHECK_EQ(file.size(), size_t(0));
    FL_CHECK_TRUE(file.eof());
    char byte = 0;
    file.read(&byte, 1);
    FL_CHECK_EQ(file.gcount(), size_t(0));
}

} // FL_TEST_FILE

// IWYU pragma: private

#include "platforms/stub/fs_stub.hpp"
#include "fl/stl/noexcept.h"

#ifdef FASTLED_TESTING

namespace fl {

// Global variable to store test root path for stub platform
fl::string g_stub_fs_root_path;

// Function to set test root path for stub platform
void setTestFileSystemRoot(const char* root_path) FL_NOEXCEPT {
    if (root_path) {
        g_stub_fs_root_path = root_path;
    } else {
        g_stub_fs_root_path.clear();
    }
}

// Getter for test root path for stub platform
const char* getTestFileSystemRoot() FL_NOEXCEPT {
    return g_stub_fs_root_path.c_str();
}

// Stub platform implementation that maps to real hard drive
FsImplPtr make_sdcard_filesystem(int cs_pin) FL_NOEXCEPT {
    FASTLED_UNUSED(cs_pin);
    fl::shared_ptr<StubFileSystem> ptr = fl::make_shared<StubFileSystem>();
    if (!g_stub_fs_root_path.empty()) {
        ptr->setRootPath(g_stub_fs_root_path);
    }
    FsImplPtr out = ptr;
    return out;
}

} // namespace fl

#endif // FASTLED_TESTING
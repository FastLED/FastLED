#pragma once

// IWYU pragma: private

/// @file platforms/stub/fs/embedded_fs_stub.hpp
/// @brief Host fragment backing `fl::getEmbeddedFs()` under FASTLED_TESTING.
///
/// FastLED #4007 asked, as its third open question:
///
///     Should the stub implement it? Mapping it to a scratch directory would
///     let host tests exercise real read paths instead of only the
///     null-fallback contract.
///
/// It would, and this is that. Without it the only host coverage of embedded
/// storage is the no-op fragment -- a genuine contract, and one the tests do
/// cover, but it exercises none of the read path a sketch actually runs.
///
/// **Opt-in, and null until asked.** `fl::getEmbeddedFs()` returns null here
/// exactly as the no-op fragment does until a test calls
/// `fl::setTestEmbeddedFileSystemRoot()`. Nothing that does not ask changes
/// behaviour, and the null-fallback tests keep testing the null fallback.
///
/// **A separate root from the SD stub, deliberately.** On a device these are
/// two media -- on-chip flash and a card -- and a sketch that writes to one
/// and reads from the other must not find the same bytes. Sharing
/// `setTestFileSystemRoot`'s directory would make a test pass on the host
/// that fails on hardware, so the two roots are set independently.

#include "fl/stl/compiler_control.h"
#include "fl/fs/fs.h"

#ifdef FASTLED_TESTING

#include "fl/stl/memory.h"
#include "fl/stl/string.h"
#include "platforms/stub/fs_stub.hpp"

namespace fl {
namespace platforms {

inline FsImplPtr makeEmbeddedFs(bool format_on_fail) FL_NO_EXCEPT {
    // Nothing to format: the backing store is a directory that either
    // exists or does not, and creating one behind a sketch's back is not
    // what the flag means on a device either.
    FASTLED_UNUSED(format_on_fail);

    const char *root = getTestEmbeddedFileSystemRoot();
    if (root == nullptr || *root == '\0') {
        return FsImplPtr();
    }
    fl::shared_ptr<StubFileSystem> ptr = fl::make_shared<StubFileSystem>();
    ptr->setRootPath(fl::string(root));
    return ptr;
}

} // namespace platforms
} // namespace fl

#else

namespace fl {
namespace platforms {

inline FsImplPtr makeEmbeddedFs(bool format_on_fail) FL_NO_EXCEPT {
    FASTLED_UNUSED(format_on_fail);
    return FsImplPtr();
}

} // namespace platforms
} // namespace fl

#endif // FASTLED_TESTING

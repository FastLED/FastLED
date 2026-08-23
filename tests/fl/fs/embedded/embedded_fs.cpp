// Tests for fl::getEmbeddedFs() — storage built into the MCU itself.
//
// The host test build has no on-chip flash, so it resolves to the no-op
// fragment. That is worth testing rather than skipping: the null path is
// what every platform without embedded storage runs, and the contract it
// has to honor is that a sketch can call getEmbeddedFs() unconditionally
// and find out at begin() instead of at link time.
//
// The ESP backend is covered by compiling for a board that has it —
// mounting a flash partition cannot be exercised on the host.

#include "test.h"

#include "fl/fs/embedded/embedded_fs.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("getEmbeddedFs links on a platform with no embedded storage") {
    // The call resolves at link time even though no backend exists in
    // this build — that is what lets sketches call it unguarded.
    fl::FsImplPtr impl = fl::getEmbeddedFs();

    FL_CHECK(!impl);
}

FL_TEST_CASE("format_on_fail does not change the no-op result") {
    FL_CHECK(!fl::getEmbeddedFs(true));
    FL_CHECK(!fl::getEmbeddedFs(false));
}

FL_TEST_CASE("FileSystem::begin reports failure rather than crashing") {
    // A sketch written for ESP32 should still build and run on a board
    // with no embedded storage, and simply learn that at runtime.
    fl::FileSystem fs;
    FL_CHECK(!fs.begin(fl::getEmbeddedFs()));
}

FL_TEST_CASE("a failed mount leaves the FileSystem safe to use") {
    fl::FileSystem fs;
    fs.begin(fl::getEmbeddedFs());

    // No backend was installed, so reads fail closed rather than
    // dereferencing a null impl.
    fl::ifstream handle = fs.openRead("does_not_exist.bin");
    FL_CHECK(!handle.is_open());

    // And tearing down an unmounted filesystem is harmless.
    fs.end();
}

} // FL_TEST_FILE

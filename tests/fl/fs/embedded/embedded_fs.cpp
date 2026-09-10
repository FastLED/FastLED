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
#include "fl/fs/testing.h"

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

// ---------------------------------------------------------------------------
// The host backend (#4007 question 3)
//
// #4007 asked whether the stub should implement this, and answered its own
// question: "Mapping it to a scratch directory would let host tests exercise
// real read paths instead of only the null-fallback contract." Everything
// above tests the null fallback. These test the read path.
//
// The root is opt-in, so the cases above still see null and still mean what
// they meant.
// ---------------------------------------------------------------------------

namespace {

// A file that exists under tests/data and is not going anywhere.
const char *kEmbeddedRoot = "tests/data";
const char *kExistingFile = "audio/README.md";

struct ScopedEmbeddedRoot {
    explicit ScopedEmbeddedRoot(const char *root) {
        fl::setTestEmbeddedFileSystemRoot(root);
    }
    ~ScopedEmbeddedRoot() { fl::setTestEmbeddedFileSystemRoot(nullptr); }
};

} // namespace

FL_TEST_CASE("the host backend stays null until a test asks for it") {
    // Every case above depends on this. If pointing the host at a directory
    // became the default, they would stop testing the no-embedded-storage
    // contract and nothing would say so.
    fl::setTestEmbeddedFileSystemRoot(nullptr);
    FL_CHECK(!fl::getEmbeddedFs());
}

FL_TEST_CASE("the host backend reads a real file once a root is set") {
    ScopedEmbeddedRoot root(kEmbeddedRoot);

    fl::FsImplPtr impl = fl::getEmbeddedFs();
    FL_REQUIRE(impl);

    fl::FileSystem fs;
    FL_REQUIRE(fs.begin(impl));

    fl::ifstream handle = fs.openRead(kExistingFile);
    FL_REQUIRE(handle.is_open());

    // Reading, not just opening: the null fallback can fake an open, and it
    // is the bytes that the ESP backend has to match.
    char first = 0;
    handle.read(&first, 1);
    FL_CHECK_NE(first, 0);

    fs.end();
}

FL_TEST_CASE("a missing file still fails closed with a root set") {
    ScopedEmbeddedRoot root(kEmbeddedRoot);

    fl::FileSystem fs;
    FL_REQUIRE(fs.begin(fl::getEmbeddedFs()));
    FL_CHECK(!fs.openRead("no/such/file.bin").is_open());
    fs.end();
}

FL_TEST_CASE("embedded storage and the SD card are separate stores") {
    // On a device these are two media. A test that wrote to on-chip flash and
    // read it back from a card must not pass, so the two roots are set
    // independently and pointing one somewhere does not move the other.
    ScopedEmbeddedRoot root("tests/data/audio");
    fl::setTestFileSystemRoot("tests/data/codec");

    fl::FileSystem embedded;
    FL_REQUIRE(embedded.begin(fl::getEmbeddedFs()));

    fl::FileSystem card;
    FL_REQUIRE(card.begin(fl::make_sdcard_filesystem(0)));

    // README.md is under the embedded root only.
    FL_CHECK(embedded.openRead("README.md").is_open());
    FL_CHECK(!card.openRead("README.md").is_open());

    // file.gif is under the card root only.
    FL_CHECK(!embedded.openRead("file.gif").is_open());
    FL_CHECK(card.openRead("file.gif").is_open());

    embedded.end();
    card.end();
    fl::setTestFileSystemRoot(nullptr);
}

} // FL_TEST_FILE

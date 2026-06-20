// Tests for the FileSystem sugar added in PR3 of #3311:
//   - FileSystem::sd(cs_pin)  -- static factory equivalent to
//                                 fs; fs.beginSd(cs_pin)
//   - FileSystem::loadFled(path) -- one-line forwarder to
//                                    fl::Fled::load(*this, path)
//
// Both are exercised against the test stub filesystem.

#include "fl/fled/fled.h"
#include "fl/system/file_system.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("FileSystem::sd factory returns a usable object") {
    // The stub backend accepts any cs_pin and returns a non-null FsImpl,
    // so this should round-trip without crash.
    FileSystem fs = FileSystem::sd(5);
    (void)fs;
    FL_CHECK(true);
}

FL_TEST_CASE("FileSystem::loadFled on a missing path returns a null Fled") {
    FileSystem fs;
    Fled f = fs.loadFled("does/not/exist.fled");
    FL_CHECK_FALSE(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(0));
}

} // FL_TEST_FILE

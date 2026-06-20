// Tests for the FileSystem sugar added in PR3 of #3311:
//   - FileSystem::sd(cs_pin)     -- static factory equivalent to
//                                    fs; fs.beginSd(cs_pin)
//   - FileSystem::loadFled(path) -- one-line forwarder to
//                                   fl::Fled::load(*this, path)
//
// We use the stub filesystem (platforms/stub/fs_stub.hpp) to drop a real
// .fled byte buffer onto a temp directory, point the test FS root at it,
// then verify the loadFled sugar reads + parses the bundle end-to-end.

#include "test.h"

#include "fl/fled/fled.h"
#include "fl/stl/int.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/system/file_system.h"

#ifdef FASTLED_TESTING
#include "platforms/stub/fs_stub.hpp"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

// RAII guard mirroring tests/fl/system/file_system.cpp - wipes the temp
// dir on entry and exit so a previous failed run cannot poison this one.
class TestDirGuard {
    fl::string mDir;
  public:
    explicit TestDirGuard(const fl::string& dir) : mDir(dir) {
        fl::StubFileSystem::forceRemoveDirectory(mDir.c_str());
        fl::StubFileSystem::createDirectory(mDir.c_str());
    }
    ~TestDirGuard() {
        fl::StubFileSystem::forceRemoveDirectory(mDir.c_str());
        fl::setTestFileSystemRoot("");
    }
};

// Build a minimal valid v1 .fled byte buffer with the given JSON envelope.
fl::vector<fl::u8> buildBundle(const char* envelope, fl::size envelopeLen,
                               const fl::u8* payload, fl::size payloadLen) {
    const fl::u32 jsonLen = static_cast<fl::u32>(envelopeLen);
    fl::vector<fl::u8> out;
    out.resize(12 + envelopeLen + payloadLen);
    out[0] = 'F'; out[1] = 'L'; out[2] = 'E'; out[3] = 'D';
    out[4] = 1;        // version
    out[5] = 0x00;     // pixel_format = rgb8
    out[6] = 0; out[7] = 0;
    out[8]  = static_cast<fl::u8>(jsonLen & 0xff);
    out[9]  = static_cast<fl::u8>((jsonLen >> 8) & 0xff);
    out[10] = static_cast<fl::u8>((jsonLen >> 16) & 0xff);
    out[11] = static_cast<fl::u8>((jsonLen >> 24) & 0xff);
    for (fl::size i = 0; i < envelopeLen; ++i) {
        out[12 + i] = static_cast<fl::u8>(envelope[i]);
    }
    for (fl::size i = 0; i < payloadLen; ++i) {
        out[12 + envelopeLen + i] = payload[i];
    }
    return out;
}

// Drop a byte buffer onto disk under the stub-fs root via createTextFile -
// it writes binary-clean (fl::ofstream is opened with ios::binary | trunc
// and ofs.write is used, not operator<<).
bool dropBundle(const fl::string& path, const fl::vector<fl::u8>& bundle) {
    fl::string content;
    content.assign(reinterpret_cast<const char*>(bundle.data()),
                   bundle.size());
    return fl::StubFileSystem::createTextFile(path, content);
}

}  // namespace

FL_TEST_CASE("FileSystem::loadFled - end-to-end via stub fs") {
    const fl::string testDir = "test_fled_loadfled";
    TestDirGuard guard(testDir);

    // Drop a valid v1 bundle on the test FS at <testDir>/sketch.fled.
    const char env[] = "{\"map\":{},\"video\":{\"fps\":24}}";
    const fl::u8 payload[3] = {0xAA, 0xBB, 0xCC};
    fl::vector<fl::u8> bundle = buildBundle(env, sizeof(env) - 1,
                                            payload, sizeof(payload));
    fl::string fullPath = testDir;
    fullPath.append("/sketch.fled");
    FL_REQUIRE(dropBundle(fullPath, bundle));

    // Point the stub fs root at <testDir>, then loadFled via the sugar.
    fl::setTestFileSystemRoot(testDir.c_str());

    fl::FileSystem fs = fl::FileSystem::sd(5);   // cs_pin ignored by stub
    fl::Fled f = fs.loadFled("sketch.fled");

    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(1));
    FL_CHECK_EQ(f.sectionCount(), fl::size(2));
    int fps = f.json()["video"]["fps"] | 0;
    FL_CHECK_EQ(fps, 24);

    // Frame payload survived the round-trip.
    fl::size n = 0;
    auto blob = f.blob("frame_payload", &n);
    FL_CHECK(blob != nullptr);
    FL_CHECK_EQ(n, fl::size(3));
    if (blob && n == 3) {
        FL_CHECK_EQ(blob.get()[0], fl::u8(0xAA));
        FL_CHECK_EQ(blob.get()[2], fl::u8(0xCC));
    }
}

FL_TEST_CASE("FileSystem::loadFled - missing file returns null Fled") {
    const fl::string testDir = "test_fled_loadfled_missing";
    TestDirGuard guard(testDir);
    fl::setTestFileSystemRoot(testDir.c_str());

    fl::FileSystem fs = fl::FileSystem::sd(5);
    fl::Fled f = fs.loadFled("does_not_exist.fled");
    FL_CHECK_FALSE(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(0));
}

FL_TEST_CASE("FileSystem::loadFled - corrupt header returns null Fled") {
    const fl::string testDir = "test_fled_loadfled_corrupt";
    TestDirGuard guard(testDir);

    // Bundle with wrong magic ('XLED' instead of 'FLED').
    const char env[] = "{\"map\":{}}";
    fl::vector<fl::u8> bundle = buildBundle(env, sizeof(env) - 1, nullptr, 0);
    bundle[0] = 'X';
    fl::string fullPath = testDir;
    fullPath.append("/bad.fled");
    FL_REQUIRE(dropBundle(fullPath, bundle));

    fl::setTestFileSystemRoot(testDir.c_str());
    fl::FileSystem fs = fl::FileSystem::sd(5);
    fl::Fled f = fs.loadFled("bad.fled");
    FL_CHECK_FALSE(static_cast<bool>(f));
}

}  // FL_TEST_FILE

#endif  // FASTLED_TESTING

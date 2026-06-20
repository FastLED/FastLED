// Tests for the FileSystem sugar added in PR3 of #3311:
//   - FileSystem::sd(cs_pin)     -- static factory equivalent to
//                                    fs; fs.beginSd(cs_pin)
//   - FileSystem::loadFled(path) -- one-line forwarder to
//                                   fl::Fled::load(*this, path)
//
// Tests use a true in-memory mock FsImpl (no disk I/O, no temp dirs,
// platform-neutral). Bundles are byte arrays inserted into a fake
// filesystem by path; the FileSystem::loadFled sugar reads them back
// through the standard FsImpl::openRead path.

#include "test.h"

#include "fl/fled/fled.h"
#include "fl/stl/cstring.h"
#include "fl/stl/detail/file_handle.h"
#include "fl/stl/flat_map.h"
#include "fl/stl/int.h"
#include "fl/stl/make_shared.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/system/file_system.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

// In-memory seekable filebuf over an owned byte vector. Mirrors the
// posix_filebuf surface but reads from RAM rather than fl::FILE*.
class MemoryFilebuf : public fl::filebuf {
  public:
    MemoryFilebuf(fl::vector<fl::u8> bytes, fl::string path) FL_NO_EXCEPT
        : mBytes(fl::move(bytes)), mPath(fl::move(path)), mPos(0),
          mOpen(true) {}
    ~MemoryFilebuf() FL_NO_EXCEPT override = default;

    bool is_open() const override { return mOpen; }
    void close() override { mOpen = false; }

    fl::size_t read(char* buffer, fl::size_t count) override {
        if (!mOpen || !buffer || count == 0) return 0;
        const fl::size_t avail = (mPos < mBytes.size()) ? (mBytes.size() - mPos) : 0;
        const fl::size_t n = (count < avail) ? count : avail;
        for (fl::size_t i = 0; i < n; ++i) {
            buffer[i] = static_cast<char>(mBytes[mPos + i]);
        }
        mPos += n;
        return n;
    }
    using fl::filebuf::read;

    fl::size_t write(const char*, fl::size_t) override { return 0; }
    fl::size_t tell() override { return mPos; }

    bool seek(fl::size_t pos, fl::seek_dir dir) override {
        fl::size_t target;
        switch (dir) {
            case fl::seek_dir::beg: target = pos; break;
            case fl::seek_dir::cur: target = mPos + pos; break;
            case fl::seek_dir::end: target = mBytes.size() + pos; break;
            default: return false;
        }
        if (target > mBytes.size()) return false;
        mPos = target;
        return true;
    }
    using fl::filebuf::seek;

    fl::size_t size() const override { return mBytes.size(); }
    const char* path() const override { return mPath.c_str(); }
    bool is_eof() const override { return mPos >= mBytes.size(); }
    bool has_error() const override { return false; }
    void clear_error() override {}
    int error_code() const override { return 0; }
    const char* error_message() const override { return "ok"; }

  private:
    fl::vector<fl::u8> mBytes;
    fl::string mPath;
    fl::size_t mPos;
    bool mOpen;
};

// In-memory FsImpl: path -> byte vector. add() seeds it, openRead()
// returns a fresh MemoryFilebuf with an independent read cursor each
// time (so the same bundle can be re-read across tests).
class MemoryFs : public fl::FsImpl {
  public:
    MemoryFs() FL_NO_EXCEPT = default;
    ~MemoryFs() FL_NO_EXCEPT override = default;

    bool begin() override { return true; }
    void end() override {}

    fl::filebuf_ptr openRead(const char* path) override {
        if (!path) return fl::filebuf_ptr();
        fl::string key(path);
        auto it = mFiles.find(key);
        if (it == mFiles.end()) return fl::filebuf_ptr();
        return fl::make_shared<MemoryFilebuf>(it->second, key);
    }

    void add(const char* path, fl::vector<fl::u8> bytes) {
        mFiles[fl::string(path)] = fl::move(bytes);
    }

  private:
    fl::flat_map<fl::string, fl::vector<fl::u8>> mFiles;
};

// Build a minimal valid v1 .fled byte buffer with the given envelope
// + payload. Mirrors the helper in tests/fl/fled/fled.cpp.
fl::vector<fl::u8> buildBundle(const char* envelope, fl::size envelopeLen,
                               const fl::u8* payload, fl::size payloadLen) {
    const fl::u32 jsonLen = static_cast<fl::u32>(envelopeLen);
    fl::vector<fl::u8> out;
    out.resize(12 + envelopeLen + payloadLen);
    out[0] = 'F'; out[1] = 'L'; out[2] = 'E'; out[3] = 'D';
    out[4] = 1;
    out[5] = 0x00;
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

}  // namespace

FL_TEST_CASE("FileSystem::loadFled - end-to-end via in-memory FsImpl") {
    auto mem = fl::make_shared<MemoryFs>();
    const char env[] = "{\"map\":{},\"video\":{\"fps\":24}}";
    const fl::u8 payload[3] = {0xAA, 0xBB, 0xCC};
    mem->add("sketch.fled",
             buildBundle(env, sizeof(env) - 1, payload, sizeof(payload)));

    fl::FileSystem fs;
    FL_REQUIRE(fs.begin(mem));

    fl::Fled f = fs.loadFled("sketch.fled");
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(1));
    FL_CHECK_EQ(f.sectionCount(), fl::size(2));
    int fps = f.json()["video"]["fps"] | 0;
    FL_CHECK_EQ(fps, 24);

    fl::size n = 0;
    auto blob = f.blob("frame_payload", &n);
    FL_CHECK(blob != nullptr);
    FL_CHECK_EQ(n, fl::size(3));
    if (blob && n == 3) {
        FL_CHECK_EQ(blob.get()[0], fl::u8(0xAA));
        FL_CHECK_EQ(blob.get()[2], fl::u8(0xCC));
    }
}

FL_TEST_CASE("FileSystem::loadFled - missing path returns null Fled") {
    auto mem = fl::make_shared<MemoryFs>();
    fl::FileSystem fs;
    FL_REQUIRE(fs.begin(mem));
    fl::Fled f = fs.loadFled("does_not_exist.fled");
    FL_CHECK_FALSE(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(0));
}

FL_TEST_CASE("FileSystem::loadFled - corrupt magic returns null Fled") {
    auto mem = fl::make_shared<MemoryFs>();
    const char env[] = "{\"map\":{}}";
    fl::vector<fl::u8> bundle = buildBundle(env, sizeof(env) - 1, nullptr, 0);
    bundle[0] = 'X';
    mem->add("bad.fled", fl::move(bundle));

    fl::FileSystem fs;
    FL_REQUIRE(fs.begin(mem));
    fl::Fled f = fs.loadFled("bad.fled");
    FL_CHECK_FALSE(static_cast<bool>(f));
}

FL_TEST_CASE("FileSystem::sd factory constructs without crash") {
    // No SD backend on the host stub; sd(5) returns an FS whose mFs is
    // null (or NullFileSystem depending on build). Either way the
    // construction path must not crash and the resulting FS must not
    // segfault when used.
    fl::FileSystem fs = fl::FileSystem::sd(5);
    fl::Fled f = fs.loadFled("anything.fled");
    FL_CHECK_FALSE(static_cast<bool>(f));
}

}  // FL_TEST_FILE

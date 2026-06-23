// Unit tests for fl::Fled (v1 surface).
// What fl::Fled returns and nothing else:
//   - Header info: version(), pixelFormat()
//   - JSON envelope: json(), sectionCount()
//   - Raw byte ranges: blob("frame_payload", &outLen)
//   - Typed objects: screenMap(), channels() (channels() is a stub)
//
// No on-disk fixtures - each test hand-builds its byte buffer.

#include "fl/channels/config.h"
#include "fl/fled/fled.h"
#include "fl/math/screenmap.h"
#include "fl/stl/int.h"
#include "fl/stl/json.h"
#include "fl/stl/move.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

const char kEnvelope[] = "{\"map\":{},\"video\":{\"fps\":30}}";
const fl::u8 kPayload[5] = {0x01, 0x02, 0x03, 0x04, 0x05};

fl::vector<fl::u8> buildBundleFromEnvelope(const char *envelope,
                                            fl::size envelopeLen,
                                            fl::u8 pixelFormat = 0x00) {
    const fl::u32 jsonLen = static_cast<fl::u32>(envelopeLen);
    fl::vector<fl::u8> out;
    out.resize(12 + envelopeLen);
    out[0] = 'F'; out[1] = 'L'; out[2] = 'E'; out[3] = 'D';
    out[4] = 1;
    out[5] = pixelFormat;
    out[6] = 0; out[7] = 0;
    out[8]  = static_cast<fl::u8>(jsonLen & 0xff);
    out[9]  = static_cast<fl::u8>((jsonLen >> 8) & 0xff);
    out[10] = static_cast<fl::u8>((jsonLen >> 16) & 0xff);
    out[11] = static_cast<fl::u8>((jsonLen >> 24) & 0xff);
    for (fl::size i = 0; i < envelopeLen; ++i) {
        out[12 + i] = static_cast<fl::u8>(envelope[i]);
    }
    return out;
}

fl::vector<fl::u8> buildValidBundle() {
    const fl::u32 jsonLen = static_cast<fl::u32>(sizeof(kEnvelope) - 1);
    fl::vector<fl::u8> out;
    out.resize(12 + jsonLen + sizeof(kPayload));
    out[0] = 'F'; out[1] = 'L'; out[2] = 'E'; out[3] = 'D';
    out[4] = 1;
    out[5] = 0x00;
    out[6] = 0; out[7] = 0;
    out[8]  = static_cast<fl::u8>(jsonLen & 0xff);
    out[9]  = static_cast<fl::u8>((jsonLen >> 8) & 0xff);
    out[10] = static_cast<fl::u8>((jsonLen >> 16) & 0xff);
    out[11] = static_cast<fl::u8>((jsonLen >> 24) & 0xff);
    for (fl::size i = 0; i < jsonLen; ++i) {
        out[12 + i] = static_cast<fl::u8>(kEnvelope[i]);
    }
    for (fl::size i = 0; i < sizeof(kPayload); ++i) {
        out[12 + jsonLen + i] = kPayload[i];
    }
    return out;
}

} // namespace

FL_TEST_CASE("fl::Fled - default-constructed is null") {
    Fled f;
    FL_CHECK_FALSE(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(0));
    FL_CHECK_EQ(f.pixelFormat(), fl::u8(0));
    FL_CHECK_EQ(f.sectionCount(), fl::size(0));
    fl::size n = 999;
    auto p = f.blob("anything", &n);
    FL_CHECK(p == nullptr);
    FL_CHECK_EQ(n, fl::size(0));
    FL_CHECK(f.screenMap() == nullptr);
    FL_CHECK(f.channels() == nullptr);
}

FL_TEST_CASE("fl::Fled - loadFromStatic happy path") {
    static fl::vector<fl::u8> sBuf = buildValidBundle();
    fl::span<const fl::u8> spanBuf(sBuf.data(), sBuf.size());
    Fled f = Fled::loadFromStatic(spanBuf);
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(1));
    FL_CHECK_EQ(f.pixelFormat(), fl::u8(0));
    FL_CHECK_EQ(f.sectionCount(), fl::size(2));
    int fps = f.json()["video"]["fps"] | 0;
    FL_CHECK_EQ(fps, 30);
    fl::size n = 0;
    auto blob = f.blob("frame_payload", &n);
    FL_CHECK(blob != nullptr);
    FL_CHECK_EQ(n, fl::size(5));
    FL_CHECK_EQ(blob.get()[0], fl::u8(0x01));
    FL_CHECK_EQ(blob.get()[4], fl::u8(0x05));
    fl::size n2 = 0;
    auto blob2 = f.blob("payload", &n2);
    FL_CHECK(blob2 != nullptr);
    FL_CHECK_EQ(n2, fl::size(5));
    fl::size n3 = 999;
    auto blob3 = f.blob("nonesuch", &n3);
    FL_CHECK(blob3 == nullptr);
    FL_CHECK_EQ(n3, fl::size(0));
}

FL_TEST_CASE("fl::Fled - loadFromVector happy path") {
    fl::vector<fl::u8> buf = buildValidBundle();
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(1));
    FL_CHECK_EQ(f.pixelFormat(), fl::u8(0));
    FL_CHECK_EQ(f.sectionCount(), fl::size(2));
    int fps = f.json()["video"]["fps"] | 0;
    FL_CHECK_EQ(fps, 30);
    fl::size n = 0;
    auto blob = f.blob("frame_payload", &n);
    FL_CHECK(blob != nullptr);
    FL_CHECK_EQ(n, fl::size(5));
}

FL_TEST_CASE("fl::Fled - copy is cheap (shares impl)") {
    fl::vector<fl::u8> buf = buildValidBundle();
    Fled a = Fled::loadFromVector(fl::move(buf));
    Fled b = a;
    FL_CHECK(static_cast<bool>(a));
    FL_CHECK(static_cast<bool>(b));
    FL_CHECK_EQ(b.version(), fl::u8(1));
    FL_CHECK_EQ(b.sectionCount(), fl::size(2));
}

FL_TEST_CASE("fl::Fled - buffer too short rejected") {
    fl::vector<fl::u8> tiny;
    tiny.resize(8);
    for (fl::size i = 0; i < tiny.size(); ++i) tiny[i] = 0;
    Fled f = Fled::loadFromVector(fl::move(tiny));
    FL_CHECK_FALSE(static_cast<bool>(f));
}

FL_TEST_CASE("fl::Fled - wrong magic rejected") {
    fl::vector<fl::u8> buf = buildValidBundle();
    buf[0] = 'X';
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK_FALSE(static_cast<bool>(f));
}

FL_TEST_CASE("fl::Fled - wrong version rejected") {
    fl::vector<fl::u8> buf = buildValidBundle();
    buf[4] = 2;
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK_FALSE(static_cast<bool>(f));
}

FL_TEST_CASE("fl::Fled - version=0 rejected") {
    fl::vector<fl::u8> buf = buildValidBundle();
    buf[4] = 0;
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK_FALSE(static_cast<bool>(f));
}

FL_TEST_CASE("fl::Fled - empty static span rejected") {
    Fled f = Fled::loadFromStatic(fl::span<const fl::u8>());
    FL_CHECK_FALSE(static_cast<bool>(f));
}

FL_TEST_CASE("fl::Fled - oversize json_length rejected") {
    fl::vector<fl::u8> buf = buildValidBundle();
    const fl::u32 huge = 0x00100000u;
    buf[8]  = static_cast<fl::u8>(huge & 0xff);
    buf[9]  = static_cast<fl::u8>((huge >> 8) & 0xff);
    buf[10] = static_cast<fl::u8>((huge >> 16) & 0xff);
    buf[11] = static_cast<fl::u8>((huge >> 24) & 0xff);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK_FALSE(static_cast<bool>(f));
}

FL_TEST_CASE("fl::Fled - malformed JSON rejected") {
    const char garbage[] = "not valid json{{{";
    const fl::u32 jsonLen = static_cast<fl::u32>(sizeof(garbage) - 1);
    fl::vector<fl::u8> buf;
    buf.resize(12 + jsonLen + 3);
    buf[0] = 'F'; buf[1] = 'L'; buf[2] = 'E'; buf[3] = 'D';
    buf[4] = 1;
    buf[5] = 0x00;
    buf[6] = 0; buf[7] = 0;
    buf[8]  = static_cast<fl::u8>(jsonLen & 0xff);
    buf[9]  = static_cast<fl::u8>((jsonLen >> 8) & 0xff);
    buf[10] = static_cast<fl::u8>((jsonLen >> 16) & 0xff);
    buf[11] = static_cast<fl::u8>((jsonLen >> 24) & 0xff);
    for (fl::size i = 0; i < jsonLen; ++i) {
        buf[12 + i] = static_cast<fl::u8>(garbage[i]);
    }
    buf[12 + jsonLen + 0] = 0xAA;
    buf[12 + jsonLen + 1] = 0xBB;
    buf[12 + jsonLen + 2] = 0xCC;
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK_FALSE(static_cast<bool>(f));
}

FL_TEST_CASE("fl::Fled - version + pixelFormat readable after load") {
    const char env[] = "{}";
    fl::vector<fl::u8> buf = buildBundleFromEnvelope(env, sizeof(env) - 1, 0x00);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(1));
    FL_CHECK_EQ(f.pixelFormat(), fl::u8(0x00));
}

FL_TEST_CASE("fl::Fled - pixelFormat non-zero round-trips") {
    const char env[] = "{}";
    fl::vector<fl::u8> buf = buildBundleFromEnvelope(env, sizeof(env) - 1, 0x05);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.pixelFormat(), fl::u8(0x05));
}

FL_TEST_CASE("fl::Fled - video metadata accessible via json()") {
    fl::vector<fl::u8> buf = buildValidBundle();
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    int fps = f.json()["video"]["fps"] | 0;
    FL_CHECK_EQ(fps, 30);
}

FL_TEST_CASE("fl::Fled - video blob via frame_payload returns the right bytes + length") {
    fl::vector<fl::u8> buf = buildValidBundle();
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    fl::size n = 0;
    auto blob = f.blob("frame_payload", &n);
    FL_REQUIRE(blob != nullptr);
    FL_CHECK_EQ(n, fl::size(5));
    FL_CHECK_EQ(blob.get()[0], fl::u8(0x01));
    FL_CHECK_EQ(blob.get()[1], fl::u8(0x02));
    FL_CHECK_EQ(blob.get()[2], fl::u8(0x03));
    FL_CHECK_EQ(blob.get()[3], fl::u8(0x04));
    FL_CHECK_EQ(blob.get()[4], fl::u8(0x05));
}

FL_TEST_CASE("fl::Fled - screenMap() returns null on default-constructed Fled") {
    Fled f;
    fl::shared_ptr<ScreenMap> sm = f.screenMap();
    FL_CHECK(sm == nullptr);
}

FL_TEST_CASE("fl::Fled - screenMap() returns null when envelope has no map key") {
    const char env[] = "{\"video\":{\"fps\":30}}";
    fl::vector<fl::u8> buf = buildBundleFromEnvelope(env, sizeof(env) - 1);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    fl::shared_ptr<ScreenMap> sm = f.screenMap();
    FL_CHECK(sm == nullptr);
}

FL_TEST_CASE("fl::Fled - screenMap() returns null when map is an empty object") {
    fl::vector<fl::u8> buf = buildValidBundle();
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    fl::shared_ptr<ScreenMap> sm = f.screenMap();
    FL_CHECK(sm == nullptr);
}

FL_TEST_CASE("fl::Fled - screenMap() parses a real v1 map") {
    const char env[] =
        "{\"map\":{\"strip1\":{\"x\":[10.5,30.5,50.5],"
        "\"y\":[20.5,40.5,60.5],\"diameter\":2.5}}}";
    fl::vector<fl::u8> buf = buildBundleFromEnvelope(env, sizeof(env) - 1);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    fl::shared_ptr<ScreenMap> sm = f.screenMap();
    FL_REQUIRE(sm != nullptr);
    FL_CHECK_EQ(sm->getLength(), fl::u32(3));
    FL_CHECK(sm->getDiameter() == 2.5f);
    FL_CHECK((*sm)[0].x == 10.5f);
    FL_CHECK((*sm)[2].y == 60.5f);
}

FL_TEST_CASE("fl::Fled - channels() is a stub returning nullptr") {
    fl::vector<fl::u8> buf = buildValidBundle();
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK(f.channels() == nullptr);
}

} // FL_TEST_FILE

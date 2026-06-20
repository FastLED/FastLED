// Unit tests for fl::fled::FledBuilder. The builder assembles a v1 .fled
// byte buffer and routes through the canonical Fled::loadFromVector()
// parse path, so a successful build() returns a Fled indistinguishable
// from one loaded from disk.

#include "fl/channels/config.h"
#include "fl/fled/builder.h"
#include "fl/fled/fled.h"
#include "fl/math/screenmap.h"
#include "fl/stl/int.h"
#include "fl/stl/json.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/span.h"
#include "fl/stl/vector.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("FledBuilder - header only yields a Fled with an empty envelope") {
    fl::fled::FledBuilder b;
    fl::Fled f = b.build();
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(1));
    FL_CHECK_EQ(f.pixelFormat(), fl::u8(0));
    FL_CHECK_EQ(f.sectionCount(), fl::size(0));
    FL_CHECK(f.screenMap() == nullptr);
    FL_CHECK(f.channels() == nullptr);
    fl::size n = 999;
    auto p = f.blob("frame_payload", &n);
    FL_CHECK(p == nullptr);
    FL_CHECK_EQ(n, fl::size(0));
}

FL_TEST_CASE("FledBuilder - setVersion + setPixelFormat round-trip into Fled") {
    fl::fled::FledBuilder b;
    b.setVersion(1).setPixelFormat(0x05);
    fl::Fled f = b.build();
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(1));
    FL_CHECK_EQ(f.pixelFormat(), fl::u8(0x05));
}

FL_TEST_CASE("FledBuilder - header + screenmap JSON yields a real ScreenMap") {
    const char* mapJson =
        "{\"strip1\":{\"x\":[10.5,30.5,50.5],"
        "\"y\":[20.5,40.5,60.5],\"diameter\":2.5}}";
    fl::fled::FledBuilder b;
    b.setScreenMapJson(mapJson);
    fl::Fled f = b.build();
    FL_CHECK(static_cast<bool>(f));
    fl::shared_ptr<ScreenMap> sm = f.screenMap();
    FL_REQUIRE(sm != nullptr);
    FL_CHECK_EQ(sm->getLength(), fl::u32(3));
    FL_CHECK(sm->getDiameter() == 2.5f);
    FL_CHECK((*sm)[0].x == 10.5f);
    FL_CHECK((*sm)[2].y == 60.5f);
}

FL_TEST_CASE("FledBuilder - header + payload yields a readable frame_payload blob") {
    const fl::u8 payload[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    fl::fled::FledBuilder b;
    b.setPayload(fl::span<const fl::u8>(payload, sizeof(payload)));
    fl::Fled f = b.build();
    FL_CHECK(static_cast<bool>(f));
    fl::size n = 0;
    auto blob = f.blob("frame_payload", &n);
    FL_REQUIRE(blob != nullptr);
    FL_CHECK_EQ(n, fl::size(4));
    FL_CHECK_EQ(blob.get()[0], fl::u8(0xDE));
    FL_CHECK_EQ(blob.get()[1], fl::u8(0xAD));
    FL_CHECK_EQ(blob.get()[2], fl::u8(0xBE));
    FL_CHECK_EQ(blob.get()[3], fl::u8(0xEF));
}

FL_TEST_CASE("FledBuilder - end-to-end header + screenmap + payload all read back") {
    const char* mapJson =
        "{\"strip1\":{\"x\":[1.0,2.0],\"y\":[3.0,4.0]}}";
    const fl::u8 payload[3] = {0x11, 0x22, 0x33};
    fl::fled::FledBuilder b;
    b.setPixelFormat(0x02)
     .setScreenMapJson(mapJson)
     .setPayload(fl::span<const fl::u8>(payload, sizeof(payload)));
    fl::Fled f = b.build();
    FL_CHECK(static_cast<bool>(f));

    // Header info round-trips.
    FL_CHECK_EQ(f.version(), fl::u8(1));
    FL_CHECK_EQ(f.pixelFormat(), fl::u8(0x02));

    // ScreenMap parses.
    fl::shared_ptr<ScreenMap> sm = f.screenMap();
    FL_REQUIRE(sm != nullptr);
    FL_CHECK_EQ(sm->getLength(), fl::u32(2));

    // Payload reads.
    fl::size n = 0;
    auto blob = f.blob("frame_payload", &n);
    FL_REQUIRE(blob != nullptr);
    FL_CHECK_EQ(n, fl::size(3));
    FL_CHECK_EQ(blob.get()[0], fl::u8(0x11));
    FL_CHECK_EQ(blob.get()[1], fl::u8(0x22));
    FL_CHECK_EQ(blob.get()[2], fl::u8(0x33));
}

} // FL_TEST_FILE

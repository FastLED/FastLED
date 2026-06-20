// Comprehensive tests for the FLED v1 container spec coverage in
// src/fl/fled/. The spec itself lives at src/fl/fled/FLED_FORMAT.md;
// this file walks through every observable behavior that spec requires:
//
//   - 12-byte binary header layout (magic, version, pixel_format,
//     reserved, json_length).
//   - All five v1 pixel-format enum values map to the correct
//     bytes-per-LED.
//   - frame_count = payload_bytes / (led_count * bytes_per_led).
//   - video.fps reader with default fallback.
//   - Envelope walked via json() reflects the on-disk shape.
//   - Frame payload bytes are accessible verbatim through blob().
//
// All bundles are hand-assembled in memory; no on-disk fixtures.

#include "fl/fled/detail/pixel_format.h"
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

// Hand-assemble a v1 .fled bundle with the given header bytes + JSON
// envelope + payload. Mirrors the layout table in FLED_FORMAT.md.
fl::vector<fl::u8> buildBundle(fl::u8 version, fl::u8 pixelFormat,
                                const char* envelope, fl::size envelopeLen,
                                const fl::u8* payload, fl::size payloadLen) {
    const fl::u32 jsonLen = static_cast<fl::u32>(envelopeLen);
    fl::vector<fl::u8> out;
    out.resize(12 + envelopeLen + payloadLen);
    // Offset 0..3: magic "FLED"
    out[0] = 'F'; out[1] = 'L'; out[2] = 'E'; out[3] = 'D';
    // Offset 4: version
    out[4] = version;
    // Offset 5: pixel_format
    out[5] = pixelFormat;
    // Offset 6..7: reserved (must be zero)
    out[6] = 0; out[7] = 0;
    // Offset 8..11: json_length u32 little-endian
    out[8]  = static_cast<fl::u8>(jsonLen & 0xff);
    out[9]  = static_cast<fl::u8>((jsonLen >> 8) & 0xff);
    out[10] = static_cast<fl::u8>((jsonLen >> 16) & 0xff);
    out[11] = static_cast<fl::u8>((jsonLen >> 24) & 0xff);
    // Offset 12..12+jsonLen: UTF-8 JSON envelope
    for (fl::size i = 0; i < envelopeLen; ++i) {
        out[12 + i] = static_cast<fl::u8>(envelope[i]);
    }
    // Offset 12+jsonLen..end: frame_payload bytes
    for (fl::size i = 0; i < payloadLen; ++i) {
        out[12 + envelopeLen + i] = payload[i];
    }
    return out;
}

}  // namespace

// ============================================================================
// Pixel format enum (FLED_FORMAT.md "Pixel Format Enum" table)
// ============================================================================

FL_TEST_CASE("FLED_FORMAT - bytesPerLed for all defined v1 pixel formats") {
    // FLED_FORMAT.md table:
    //   0x00 rgb8     -> 3 bpp
    //   0x01 gray8    -> 1 bpp
    //   0x02 rgba8    -> 4 bpp
    //   0x03 rgbw8    -> 4 bpp
    //   0x04 rgb565le -> 2 bpp
    FL_CHECK_EQ(fl::fled::bytesPerLed(fl::u8(0x00)), fl::u8(3));
    FL_CHECK_EQ(fl::fled::bytesPerLed(fl::u8(0x01)), fl::u8(1));
    FL_CHECK_EQ(fl::fled::bytesPerLed(fl::u8(0x02)), fl::u8(4));
    FL_CHECK_EQ(fl::fled::bytesPerLed(fl::u8(0x03)), fl::u8(4));
    FL_CHECK_EQ(fl::fled::bytesPerLed(fl::u8(0x04)), fl::u8(2));
}

FL_TEST_CASE("FLED_FORMAT - bytesPerLed for reserved values returns 0") {
    // Per spec: 0x05..0xff are reserved for future formats. Returning 0
    // signals "consumer does not support" so callers reject before
    // reading frame bytes.
    FL_CHECK_EQ(fl::fled::bytesPerLed(fl::u8(0x05)), fl::u8(0));
    FL_CHECK_EQ(fl::fled::bytesPerLed(fl::u8(0x10)), fl::u8(0));
    FL_CHECK_EQ(fl::fled::bytesPerLed(fl::u8(0xff)), fl::u8(0));
}

FL_TEST_CASE("FLED_FORMAT - PixelFormat enum overload agrees with raw u8") {
    using fl::fled::PixelFormat;
    FL_CHECK_EQ(fl::fled::bytesPerLed(PixelFormat::Rgb8),     fl::u8(3));
    FL_CHECK_EQ(fl::fled::bytesPerLed(PixelFormat::Gray8),    fl::u8(1));
    FL_CHECK_EQ(fl::fled::bytesPerLed(PixelFormat::Rgba8),    fl::u8(4));
    FL_CHECK_EQ(fl::fled::bytesPerLed(PixelFormat::Rgbw8),    fl::u8(4));
    FL_CHECK_EQ(fl::fled::bytesPerLed(PixelFormat::Rgb565Le), fl::u8(2));
}

// ============================================================================
// Header parse (FLED_FORMAT.md "File Layout" table)
// ============================================================================

FL_TEST_CASE("FLED_FORMAT - header round-trips version + pixelFormat") {
    const char env[] = "{}";
    // Pick a non-default pixel_format to confirm the byte is preserved.
    fl::vector<fl::u8> buf = buildBundle(/*version=*/1, /*pixel_format=*/0x03,
                                          env, sizeof(env) - 1, nullptr, 0);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.version(), fl::u8(1));
    FL_CHECK_EQ(f.pixelFormat(), fl::u8(0x03));
    FL_CHECK_EQ(f.bytesPerLed(), fl::u8(4));   // rgbw8
}

// ============================================================================
// frame_count derivation (FLED_FORMAT.md "File Layout" paragraph)
// ============================================================================

FL_TEST_CASE("FLED_FORMAT - frameCount derives from payload + led_count + bpp") {
    // rgb8: 3 bpp. led_count = 4. 6 frames * 4 leds * 3 bpp = 72 bytes.
    const char env[] = "{}";
    fl::vector<fl::u8> payload(72, 0xAB);
    fl::vector<fl::u8> buf = buildBundle(1, 0x00, env, sizeof(env) - 1,
                                          payload.data(), payload.size());
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.payloadBytes(), fl::size(72));
    FL_CHECK_EQ(f.frameCount(fl::size(4)), fl::size(6));
}

FL_TEST_CASE("FLED_FORMAT - frameCount truncates on partial frame") {
    // rgba8: 4 bpp. led_count = 2. payload = 17 bytes -> 2 complete
    // frames (16 bytes), 1 byte remainder. Spec says readers should
    // "treat a remainder as malformed"; frameCount returns the floor.
    const char env[] = "{}";
    fl::vector<fl::u8> payload(17, 0xFF);
    fl::vector<fl::u8> buf = buildBundle(1, 0x02, env, sizeof(env) - 1,
                                          payload.data(), payload.size());
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.frameCount(fl::size(2)), fl::size(2));
}

FL_TEST_CASE("FLED_FORMAT - frameCount is 0 for reserved pixel formats") {
    // 0x42 is reserved -> bytesPerLed == 0 -> frameCount must be 0
    // regardless of led_count, to avoid divide-by-zero.
    const char env[] = "{}";
    fl::vector<fl::u8> payload(100, 0x00);
    fl::vector<fl::u8> buf = buildBundle(1, 0x42, env, sizeof(env) - 1,
                                          payload.data(), payload.size());
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.bytesPerLed(), fl::u8(0));
    FL_CHECK_EQ(f.frameCount(fl::size(10)), fl::size(0));
}

FL_TEST_CASE("FLED_FORMAT - frameCount is 0 for led_count == 0") {
    const char env[] = "{}";
    fl::vector<fl::u8> payload(30, 0x00);
    fl::vector<fl::u8> buf = buildBundle(1, 0x00, env, sizeof(env) - 1,
                                          payload.data(), payload.size());
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK_EQ(f.frameCount(fl::size(0)), fl::size(0));
}

FL_TEST_CASE("FLED_FORMAT - frameCount per pixel format - all v1 formats") {
    // 12 bytes of payload, 1 LED, run the formula across all five formats.
    const char env[] = "{}";
    fl::vector<fl::u8> payload(12, 0x00);

    struct Case { fl::u8 fmt; fl::u8 expectedBpp; fl::size expectedFrames; };
    Case cases[5] = {
        {0x00, 3, 4},   // rgb8:     12 / (1 * 3) = 4
        {0x01, 1, 12},  // gray8:    12 / (1 * 1) = 12
        {0x02, 4, 3},   // rgba8:    12 / (1 * 4) = 3
        {0x03, 4, 3},   // rgbw8:    12 / (1 * 4) = 3
        {0x04, 2, 6},   // rgb565le: 12 / (1 * 2) = 6
    };

    for (int i = 0; i < 5; ++i) {
        fl::vector<fl::u8> buf = buildBundle(1, cases[i].fmt, env,
                                              sizeof(env) - 1,
                                              payload.data(), payload.size());
        Fled f = Fled::loadFromVector(fl::move(buf));
        FL_CHECK(static_cast<bool>(f));
        FL_CHECK_EQ(f.bytesPerLed(), cases[i].expectedBpp);
        FL_CHECK_EQ(f.frameCount(fl::size(1)), cases[i].expectedFrames);
    }
}

// ============================================================================
// Video metadata (FLED_FORMAT.md "JSON Envelope - Video metadata")
// ============================================================================

FL_TEST_CASE("FLED_FORMAT - videoFps reads from envelope's video.fps") {
    const char env[] = "{\"video\":{\"fps\":24}}";
    fl::vector<fl::u8> buf = buildBundle(1, 0x00, env, sizeof(env) - 1,
                                          nullptr, 0);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK(f.videoFps() == 24.0f);
}

FL_TEST_CASE("FLED_FORMAT - videoFps falls back to default when absent") {
    // Spec: "If video.fps is absent, consumers may use an application
    // default ...". The accessor returns the caller-supplied default.
    const char env[] = "{}";
    fl::vector<fl::u8> buf = buildBundle(1, 0x00, env, sizeof(env) - 1,
                                          nullptr, 0);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK(f.videoFps(60.0f) == 60.0f);
    FL_CHECK(f.videoFps() == 30.0f);  // default default is 30
}

FL_TEST_CASE("FLED_FORMAT - videoFps falls back when video is non-object") {
    const char env[] = "{\"video\":42}";
    fl::vector<fl::u8> buf = buildBundle(1, 0x00, env, sizeof(env) - 1,
                                          nullptr, 0);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK(f.videoFps(7.5f) == 7.5f);
}

FL_TEST_CASE("FLED_FORMAT - videoFps on null Fled returns the default") {
    Fled null;
    FL_CHECK(null.videoFps() == 30.0f);
    FL_CHECK(null.videoFps(120.0f) == 120.0f);
}

// ============================================================================
// JSON envelope passthrough (FLED_FORMAT.md "JSON Envelope")
// ============================================================================

FL_TEST_CASE("FLED_FORMAT - json() returns the envelope verbatim") {
    const char env[] =
        "{\"map\":{},\"video\":{\"fps\":15},\"custom\":{\"k\":\"v\"}}";
    fl::vector<fl::u8> buf = buildBundle(1, 0x00, env, sizeof(env) - 1,
                                          nullptr, 0);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.sectionCount(), fl::size(3));
    int fps = f.json()["video"]["fps"] | 0;
    FL_CHECK_EQ(fps, 15);
    // Unknown sections survive in the envelope per the spec
    // ("readers should consume the sections they understand and
    // preserve or ignore unknown sections").
    FL_CHECK(f.json()["custom"].is_object());
}

// ============================================================================
// ScreenMap parse (FLED_FORMAT.md "JSON Envelope - Required screenmap")
// ============================================================================

FL_TEST_CASE("FLED_FORMAT - screenMap parses the v1 envelope's map key") {
    // Example envelope mirrors the one in FLED_FORMAT.md.
    const char env[] =
        "{\"map\":{\"strip0\":{\"x\":[0.0,1.0,2.0],"
        "\"y\":[0.0,0.0,0.0],\"diameter\":0.25}},"
        "\"video\":{\"fps\":30}}";
    fl::vector<fl::u8> buf = buildBundle(1, 0x00, env, sizeof(env) - 1,
                                          nullptr, 0);
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    fl::shared_ptr<ScreenMap> sm = f.screenMap();
    FL_REQUIRE(sm != nullptr);
    FL_CHECK_EQ(sm->getLength(), fl::u32(3));
    FL_CHECK(sm->getDiameter() == 0.25f);
    FL_CHECK((*sm)[0].x == 0.0f);
    FL_CHECK((*sm)[2].x == 2.0f);
}

FL_TEST_CASE("FLED_FORMAT - led_count from screenMap drives frameCount") {
    // End-to-end demonstration of FLED_FORMAT.md's frame-count formula:
    // payload_bytes / (led_count * bytes_per_led).
    //   led_count = 3 (from screenmap segment lengths)
    //   bytes_per_led = 3 (rgb8)
    //   payload_bytes = 45 -> 5 frames
    const char env[] =
        "{\"map\":{\"strip0\":{\"x\":[0,1,2],\"y\":[0,0,0]}}}";
    fl::vector<fl::u8> payload(45, 0x42);
    fl::vector<fl::u8> buf = buildBundle(1, 0x00, env, sizeof(env) - 1,
                                          payload.data(), payload.size());
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    fl::shared_ptr<ScreenMap> sm = f.screenMap();
    FL_REQUIRE(sm != nullptr);
    const fl::size ledCount = static_cast<fl::size>(sm->getLength());
    FL_CHECK_EQ(ledCount, fl::size(3));
    FL_CHECK_EQ(f.frameCount(ledCount), fl::size(5));
}

// ============================================================================
// Frame payload bytes (FLED_FORMAT.md "Frame Payload" section)
// ============================================================================

FL_TEST_CASE("FLED_FORMAT - blob('frame_payload') returns the post-JSON bytes verbatim") {
    const char env[] = "{}";
    const fl::u8 payload[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    fl::vector<fl::u8> buf = buildBundle(1, 0x00, env, sizeof(env) - 1,
                                          payload, sizeof(payload));
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    fl::size n = 0;
    auto blob = f.blob("frame_payload", &n);
    FL_REQUIRE(blob != nullptr);
    FL_CHECK_EQ(n, fl::size(6));
    for (fl::size i = 0; i < sizeof(payload); ++i) {
        FL_CHECK_EQ(blob.get()[i], payload[i]);
    }
}

FL_TEST_CASE("FLED_FORMAT - payloadBytes matches blob length") {
    const char env[] = "{}";
    fl::vector<fl::u8> payload(123, 0x55);
    fl::vector<fl::u8> buf = buildBundle(1, 0x00, env, sizeof(env) - 1,
                                          payload.data(), payload.size());
    Fled f = Fled::loadFromVector(fl::move(buf));
    FL_CHECK(static_cast<bool>(f));
    FL_CHECK_EQ(f.payloadBytes(), fl::size(123));
    fl::size n = 0;
    auto blob = f.blob("frame_payload", &n);
    FL_CHECK(blob != nullptr);
    FL_CHECK_EQ(n, fl::size(123));
}

}  // FL_TEST_FILE

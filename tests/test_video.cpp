
// g++ --std=c++11 test.cpp

#include "test.h"

#include <vector>

#include "crgb.h"
#include "fl/bytestreammemory.h"
#include "fl/ptr.h"
#include "fx/video.h"
#include "fx/video/pixel_stream.h"
#include "lib8tion/intmap.h"
#include "test.h"

#include "fl/namespace.h"

#define FPS 30
#define FRAME_TIME 1000 / FPS
#define VIDEO_WIDTH 10
#define VIDEO_HEIGHT 10
#define LEDS_PER_FRAME VIDEO_WIDTH *VIDEO_HEIGHT

FASTLED_SMART_PTR(FakeFileHandle);

using namespace fl;

class FakeFileHandle : public FileHandle {
  public:
    virtual ~FakeFileHandle() {}
    bool available() const override { return mPos < data.size(); }
    size_t bytesLeft() const override { return data.size() - mPos; }
    size_t size() const override { return data.size(); }
    bool valid() const override { return true; }

    size_t write(const uint8_t *src, size_t len) {
        data.insert(data.end(), src, src + len);
        return len;
    }
    size_t writeCRGB(const CRGB *src, size_t len) {
        size_t bytes_written = write((const uint8_t *)src, len * 3);
        return bytes_written / 3;
    }
    size_t read(uint8_t *dst, size_t bytesToRead) override {
        size_t bytesRead = 0;
        while (bytesRead < bytesToRead && mPos < data.size()) {
            dst[bytesRead] = data[mPos];
            bytesRead++;
            mPos++;
        }
        return bytesRead;
    }
    size_t pos() const override { return mPos; }
    const char *path() const override { return "fake"; }
    bool seek(size_t pos) override {
        this->mPos = pos;
        return true;
    }
    void close() override {}
    std::vector<uint8_t> data;
    size_t mPos = 0;
};

TEST_CASE("video with memory stream") {
    // Video video(LEDS_PER_FRAME, FPS);
    Video video(LEDS_PER_FRAME, FPS, 1);
    video.setFade(0, 0);
    ByteStreamMemoryPtr memoryStream =
        fl::make_shared<ByteStreamMemory>(LEDS_PER_FRAME * 3);
    CRGB testData[LEDS_PER_FRAME] = {};
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        testData[i] = i % 2 == 0 ? CRGB::Red : CRGB::Black;
    }
    size_t pixels_written = memoryStream->writeCRGB(testData, LEDS_PER_FRAME);
    REQUIRE_EQ(pixels_written, LEDS_PER_FRAME);
    video.beginStream(memoryStream);
    CRGB leds[LEDS_PER_FRAME];
    bool ok = video.draw(FRAME_TIME + 1, leds);
    REQUIRE(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        CHECK_EQ(leds[i], testData[i]);
    }
    ok = video.draw(2 * FRAME_TIME + 1, leds);
    REQUIRE(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        // CHECK_EQ(leds[i], testData[i]);
        REQUIRE_EQ(leds[i].r, testData[i].r);
        REQUIRE_EQ(leds[i].g, testData[i].g);
        REQUIRE_EQ(leds[i].b, testData[i].b);
    }
}

TEST_CASE("video with memory stream, interpolated") {
    // Video video(LEDS_PER_FRAME, FPS);
    Video video(LEDS_PER_FRAME, 1);
    video.setFade(0, 0);
    ByteStreamMemoryPtr memoryStream =
        fl::make_shared<ByteStreamMemory>(LEDS_PER_FRAME * sizeof(CRGB) * 2);
    CRGB testData[LEDS_PER_FRAME] = {};
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        testData[i] = CRGB::Red;
    }
    size_t pixels_written = memoryStream->writeCRGB(testData, LEDS_PER_FRAME);
    CHECK_EQ(pixels_written, LEDS_PER_FRAME);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        testData[i] = CRGB::Black;
    }
    pixels_written = memoryStream->writeCRGB(testData, LEDS_PER_FRAME);
    CHECK_EQ(pixels_written, LEDS_PER_FRAME);
    video.beginStream(memoryStream); // One frame per second.
    CRGB leds[LEDS_PER_FRAME];
    bool ok = video.draw(0, leds); // First frame starts time 0.
    ok = video.draw(500, leds);    // Half a frame.
    CHECK(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        int r = leds[i].r;
        int g = leds[i].g;
        int b = leds[i].b;
        REQUIRE_EQ(128, r); // We expect the color to be interpolated to 128.
        REQUIRE_EQ(0, g);
        REQUIRE_EQ(0, b);
    }
}

TEST_CASE("video with file handle") {
    // Video video(LEDS_PER_FRAME, FPS);
    Video video(LEDS_PER_FRAME, FPS);
    video.setFade(0, 0);
    FakeFileHandlePtr fileHandle = fl::make_shared<FakeFileHandle>();
    CRGB led_frame[LEDS_PER_FRAME];
    // alternate between red and black
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        led_frame[i] = i % 2 == 0 ? CRGB::Red : CRGB::Black;
    }
    // now write the data
    size_t leds_written = fileHandle->writeCRGB(led_frame, LEDS_PER_FRAME);
    CHECK_EQ(leds_written, LEDS_PER_FRAME);
    video.begin(fileHandle);
    CRGB leds[LEDS_PER_FRAME];
    bool ok = video.draw(FRAME_TIME + 1, leds);
    REQUIRE(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        CHECK_EQ(leds[i], led_frame[i]);
    }
    ok = video.draw(2 * FRAME_TIME + 1, leds);
    CHECK(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        CHECK_EQ(leds[i], led_frame[i]);
    }
}

TEST_CASE("Video duration") {
    Video video(LEDS_PER_FRAME, FPS);
    FakeFileHandlePtr fileHandle = fl::make_shared<FakeFileHandle>();
    CRGB led_frame[LEDS_PER_FRAME];
    // just set all the leds to white

    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        led_frame[i] = CRGB::White;
    }
    // fill frames for all of one second
    for (uint32_t i = 0; i < FPS; i++) {
        size_t leds_written = fileHandle->writeCRGB(led_frame, LEDS_PER_FRAME);
        CHECK_EQ(leds_written, LEDS_PER_FRAME);
    }

    video.begin(fileHandle);
    int32_t duration = video.durationMicros();
    float duration_f = duration / 1000.0;
    CHECK_EQ(1000, uint32_t(duration_f + 0.5));
}

TEST_CASE("video with end frame fadeout") {
    Video video(LEDS_PER_FRAME, FPS);
    video.setFade(0, 1000);
    FakeFileHandlePtr fileHandle = fl::make_shared<FakeFileHandle>();
    CRGB led_frame[LEDS_PER_FRAME];
    // just set all the leds to white

    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        led_frame[i] = CRGB::White;
    }
    // fill frames for all of one second
    for (uint32_t i = 0; i < FPS; i++) {
        size_t leds_written = fileHandle->writeCRGB(led_frame, LEDS_PER_FRAME);
        CHECK_EQ(leds_written, LEDS_PER_FRAME);
    }

    video.begin(fileHandle);
    CRGB leds[LEDS_PER_FRAME];
    bool ok = video.draw(0, leds);
    REQUIRE(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        CHECK_EQ(leds[i], led_frame[i]);
    }
    ok = video.draw(500, leds);
    // test that the leds are about half as bright
    REQUIRE(ok);



    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        // This is what the values should be but we don't do inter-frame
        // interpolation yet. CHECK_EQ(leds[i].r, 127); CHECK_EQ(leds[i].g,
        // 127); CHECK_EQ(leds[i].b, 127);
        CHECK_EQ(leds[i].r, 110);
        CHECK_EQ(leds[i].g, 110);
        CHECK_EQ(leds[i].b, 110);
    }

    ok = video.draw(900, leds); // close to last frame
    REQUIRE(ok);
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        CHECK_EQ(leds[i].r, 8);
        CHECK_EQ(leds[i].g, 8);
        CHECK_EQ(leds[i].b, 8);
    }

    ok = video.draw(965, leds); // Last frame
    REQUIRE(ok);
    // test that the leds are almost black
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        REQUIRE_EQ(leds[i], CRGB(0, 0, 0));
    }
    #if 0  // Bug - we do not handle wrapping around
    ok = video.draw(1000, leds); // Bug - we have to let the buffer drain with one frame.
    ok = video.draw(1000, leds); // After last frame we flip around
    for (uint32_t i = 0; i < LEDS_PER_FRAME; i++) {
        REQUIRE_EQ(leds[i], CRGB(4, 4, 4));
    }
    #endif  //
}

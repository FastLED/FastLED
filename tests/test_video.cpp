
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <vector>

#include "doctest.h"
#include "lib8tion/intmap.h"
#include "fx/detail/data_stream.h"
#include "fx/storage/bytestreammemory.h"
#include "fx/video.h"
#include "ref.h"
#include "crgb.h"


#include "namespace.h"

FASTLED_USING_NAMESPACE

#define FPS 30
#define FRAME_TIME 1000 / FPS
#define VIDEO_WIDTH 10
#define VIDEO_HEIGHT 10
#define LEDS_PER_FRAME VIDEO_WIDTH * VIDEO_HEIGHT

FASTLED_SMART_REF(FakeFileHandle);

class FakeFileHandle: public FileHandle {
  public:
    virtual ~FakeFileHandle() {}
    bool available() const override {
        return mPos < data.size();
    }
    size_t bytesLeft() const override {
        return data.size() - mPos;
    }
    size_t size() const override {
        return data.size();
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
    size_t pos() const override {
        return mPos;
    }
    const char* path() const override { return "fake"; }
    void seek(size_t pos) override {
        this->mPos = pos;
    }
    void close() override {}
    std::vector<uint8_t> data;
    size_t mPos = 0;
};

TEST_CASE("video with memory stream") {
    Video video(LEDS_PER_FRAME, FPS);
    ByteStreamMemoryRef memoryStream = ByteStreamMemoryRef::New(LEDS_PER_FRAME);
    uint8_t testData[LEDS_PER_FRAME];
    memoryStream->write(testData, LEDS_PER_FRAME);
    video.beginStream(memoryStream);
    CRGB leds[LEDS_PER_FRAME];
    video.draw(FRAME_TIME+1, leds);
    video.draw(2*FRAME_TIME + 1, leds);
}

TEST_CASE("video with file handle") {
    Video video(LEDS_PER_FRAME, FPS);
    FileHandleRef fileHandle = FakeFileHandleRef::New();
    video.begin(fileHandle);
    CRGB leds[LEDS_PER_FRAME];
    video.draw(FRAME_TIME+1, leds);
    video.draw(2*FRAME_TIME + 1, leds);
}

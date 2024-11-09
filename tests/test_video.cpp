
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "lib8tion/intmap.h"
#include "fx/detail/data_stream.h"
#include "fx/storage/bytestreammemory.h"
#include "fx/video.h"
#include "ref.h"

#include "namespace.h"

FASTLED_USING_NAMESPACE

#define FPS 30
#define FRAME_TIME 1000 / FPS
#define VIDEO_WIDTH 10
#define VIDEO_HEIGHT 10
#define LEDS_PER_FRAME VIDEO_WIDTH * VIDEO_HEIGHT

TEST_CASE("video") {
    Video video(LEDS_PER_FRAME, FPS);
    ByteStreamMemoryRef memoryStream = ByteStreamMemoryRef::New(LEDS_PER_FRAME);
    uint8_t testData[LEDS_PER_FRAME];
    memoryStream->write(testData, LEDS_PER_FRAME);
    video.beginStream(memoryStream);
    CRGB leds[LEDS_PER_FRAME];
    video.draw(FRAME_TIME+1, leds);
    video.draw(2*FRAME_TIME + 1, leds);
}

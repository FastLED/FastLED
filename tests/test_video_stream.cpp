
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "lib8tion/intmap.h"
#include "fx/util/video_stream.h"
#include "fx/storage/bytestreammemory.h"
#include "ptr.h"

TEST_CASE("video stream simple test") {
    const int BYTES_PER_FRAME = 3 * 10 * 10; // Assuming a 10x10 RGB video
    
    // Create a ByteStreamMemory
    const uint32_t BUFFER_SIZE = BYTES_PER_FRAME * 10; // Enough for 10 frames
    ByteStreamMemoryPtr memoryStream = Ptr::New<ByteStreamMemory>(BUFFER_SIZE);

    // Fill the ByteStreamMemory with test data
    uint8_t testData[BUFFER_SIZE];
    for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
        testData[i] = static_cast<uint8_t>(i % 256);
    }
    memoryStream->write(testData, BUFFER_SIZE);

    // Create and initialize VideoStream
    VideoStreamPtr videoStream = Ptr::New<VideoStream>(BYTES_PER_FRAME);
    bool initSuccess = videoStream->beginStream(memoryStream);
    REQUIRE(initSuccess);

    // Test basic properties
    CHECK(videoStream->getType() == VideoStream::kStreaming);
    CHECK(videoStream->BytesPerFrame() == BYTES_PER_FRAME);

    // Read a pixel
    CRGB pixel;
    bool readSuccess = videoStream->ReadPixel(&pixel);
    REQUIRE(readSuccess);
    CHECK(pixel.r == 0);
    CHECK(pixel.g == 1);
    CHECK(pixel.b == 2);

    // Read some bytes
    uint8_t buffer[10];
    size_t bytesRead = videoStream->ReadBytes(buffer, 10);
    CHECK(bytesRead == 10);
    for (int i = 0; i < 10; ++i) {
        CHECK(buffer[i] == static_cast<uint8_t>((i + 3) % 256));
    }

    // Check frame counting - streaming mode doesn't support this.
    //CHECK(videoStream->FramesDisplayed() == 0);
    //CHECK(videoStream->FramesRemaining() == 10); // We have 10 frames of data

    // Close the stream
    videoStream->Close();
}



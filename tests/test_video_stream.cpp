
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "lib8tion/intmap.h"
#include "fx/detail/data_stream.h"
#include "fx/storage/bytestreammemory.h"
#include "ref.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("video stream simple test") {
    const int BYTES_PER_FRAME = 3 * 10 * 10; // Assuming a 10x10 RGB video
    
    // Create a ByteStreamMemory
    const uint32_t BUFFER_SIZE = BYTES_PER_FRAME * 10; // Enough for 10 frames
    ByteStreamMemoryRef memoryStream = ByteStreamMemoryRef::New(BUFFER_SIZE);

    // Fill the ByteStreamMemory with test data
    uint8_t testData[BUFFER_SIZE];
    for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
        testData[i] = static_cast<uint8_t>(i % 256);
    }
    memoryStream->write(testData, BUFFER_SIZE);

    // Create and initialize DataStream
    DataStreamRef DataStream = DataStreamRef::New(BYTES_PER_FRAME);
    bool initSuccess = DataStream->beginStream(memoryStream);
    REQUIRE(initSuccess);

    // Test basic properties
    CHECK(DataStream->getType() == DataStream::kStreaming);
    CHECK(DataStream->BytesPerFrame() == BYTES_PER_FRAME);

    // Read a pixel
    CRGB pixel;
    bool readSuccess = DataStream->ReadPixel(&pixel);
    REQUIRE(readSuccess);
    CHECK(pixel.r == 0);
    CHECK(pixel.g == 1);
    CHECK(pixel.b == 2);

    // Read some bytes
    uint8_t buffer[10];
    size_t bytesRead = DataStream->ReadBytes(buffer, 10);
    CHECK(bytesRead == 10);
    for (int i = 0; i < 10; ++i) {
        CHECK(buffer[i] == static_cast<uint8_t>((i + 3) % 256));
    }

    // Check frame counting - streaming mode doesn't support this.
    //CHECK(DataStream->FramesDisplayed() == 0);
    //CHECK(DataStream->FramesRemaining() == 10); // We have 10 frames of data

    // Close the stream
    DataStream->Close();
}



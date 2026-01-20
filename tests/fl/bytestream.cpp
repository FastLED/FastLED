// Compile with: g++ --std=c++11 test.cpp


#include "fl/bytestreammemory.h"
#include "fl/fx/video/pixel_stream.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "doctest.h"
#include "fl/fx/video.h"
#include "fl/stl/shared_ptr.h"
#include "hsv2rgb.h"

TEST_CASE("ByteStreamMemory basic operations") {

    SUBCASE("Write and read single byte") {
        fl::ByteStreamMemory stream(10);  // Stream with 10 bytes capacity
        uint8_t testByte = 42;
        CHECK(stream.write(&testByte, 1) == 1);
        
        uint8_t readByte = 0;
        CHECK(stream.read(&readByte, 1) == 1);
        CHECK(readByte == testByte);

        // Next read should fail since the stream is empty
        CHECK(stream.read(&readByte, 1) == 0);
    }

    SUBCASE("Write and read multiple bytes") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3, 4, 5};
        CHECK(stream.write(testData, 5) == 5);
        
        uint8_t readData[5] = {0};
        CHECK(stream.read(readData, 5) == 5);
        for (int i = 0; i < 5; ++i) {
            CHECK(readData[i] == testData[i]);
        }
    }

    SUBCASE("Attempt to read from empty stream") {
        fl::ByteStreamMemory stream(10);
        uint8_t readByte = 0;
        CHECK(stream.read(&readByte, 1) == 0);
    }

    SUBCASE("Attempt to write beyond capacity") {
        fl::ByteStreamMemory stream(5);
        uint8_t testData[] = {1, 2, 3, 4, 5, 6};
        CHECK(stream.write(testData, 6) == 5);  // Should write up to capacity
    }

    SUBCASE("Attempt to read more than available data") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3};
        CHECK(stream.write(testData, 3) == 3);

        uint8_t readData[5] = {0};
        CHECK(stream.read(readData, 5) == 3);  // Should read only available data
        //CHECK(readData[0] == 1);
        //CHECK(readData[1] == 2);
        //CHECK(readData[2] == 3);
    }

    SUBCASE("Multiple write and read operations") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData1[] = {1, 2, 3};
        uint8_t testData2[] = {4, 5};
        CHECK(stream.write(testData1, 3) == 3);
        CHECK(stream.write(testData2, 2) == 2);

        uint8_t readData[5] = {0};
        CHECK(stream.read(readData, 5) == 5);
        CHECK(readData[0] == 1);
        CHECK(readData[1] == 2);
        CHECK(readData[2] == 3);
        CHECK(readData[3] == 4);
        CHECK(readData[4] == 5);
    }


    SUBCASE("Write after partial read") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3, 4, 5};
        CHECK(stream.write(testData, 5) == 5);

        uint8_t readData[2] = {0};
        CHECK(stream.read(readData, 2) == 2);
        CHECK(readData[0] == 1);
        CHECK(readData[1] == 2);

        uint8_t newTestData[] = {6, 7};
        CHECK(stream.write(newTestData, 2) == 2);

        uint8_t remainingData[5] = {0};
        CHECK(stream.read(remainingData, 5) == 5);
        CHECK(remainingData[0] == 3);
        CHECK(remainingData[1] == 4);
        CHECK(remainingData[2] == 5);
        CHECK(remainingData[3] == 6);
        CHECK(remainingData[4] == 7);
    }

    SUBCASE("Fill and empty stream multiple times") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[10];
        for (uint8_t i = 0; i < 10; ++i) {
            testData[i] = i;
        }

        // First cycle
        CHECK(stream.write(testData, 10) == 10);
        uint8_t readData[10] = {0};
        CHECK(stream.read(readData, 10) == 10);
        for (uint8_t i = 0; i < 10; ++i) {
            CHECK(readData[i] == i);
        }

        // Second cycle
        CHECK(stream.write(testData, 10) == 10);
        CHECK(stream.read(readData, 10) == 10);
        for (uint8_t i = 0; i < 10; ++i) {
            CHECK(readData[i] == i);
        }
    }

    SUBCASE("Zero-length write and read") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3};
        CHECK(stream.write(testData, 0) == 0);

        uint8_t readData[3] = {0};
        CHECK(stream.read(readData, 0) == 0);
    }

    SUBCASE("Write and read with null pointers") {
        fl::ByteStreamMemory stream(10);
        CHECK(stream.write(static_cast<uint8_t*>(nullptr), 5) == 0);
        CHECK(stream.read(nullptr, 5) == 0);
    }

    SUBCASE("Boundary conditions") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[10];
        for (uint8_t i = 0; i < 10; ++i) {
            testData[i] = i;
        }
        CHECK(stream.write(testData, 10) == 10);

        uint8_t readData[10] = {0};
        CHECK(stream.read(readData, 10) == 10);
        for (uint8_t i = 0; i < 10; ++i) {
            CHECK(readData[i] == i);
        }

        // Try to write again to full capacity
        CHECK(stream.write(testData, 10) == 10);
    }

    SUBCASE("Write with partial capacity") {
        fl::ByteStreamMemory stream(5);
        uint8_t testData[] = {1, 2, 3, 4, 5};
        CHECK(stream.write(testData, 5) == 5);

        uint8_t moreData[] = {6, 7};
        CHECK(stream.write(moreData, 2) == 0);  // Should not write since capacity is full

        uint8_t readData[5] = {0};
        CHECK(stream.read(readData, 5) == 5);
        for (int i = 0; i < 5; ++i) {
            CHECK(readData[i] == testData[i]);
        }

        // Now buffer is empty, try writing again
        CHECK(stream.write(moreData, 2) == 2);
        CHECK(stream.read(readData, 2) == 2);
        CHECK(readData[0] == 6);
        CHECK(readData[1] == 7);
    }

    SUBCASE("Read after buffer is reset") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3};
        CHECK(stream.write(testData, 3) == 3);

        stream.clear();  // Assuming reset clears the buffer

        uint8_t readData[3] = {0};
        CHECK(stream.read(readData, 3) == 0);  // Should read nothing
    }

    SUBCASE("Write zero bytes when buffer is full") {
        fl::ByteStreamMemory stream(0);  // Zero capacity
        uint8_t testByte = 42;
        CHECK(stream.write(&testByte, 1) == 0);  // Cannot write to zero-capacity buffer
    }

    SUBCASE("Sequential writes and reads") {
        fl::ByteStreamMemory stream(10);
        for (uint8_t i = 0; i < 10; ++i) {
            CHECK(stream.write(&i, 1) == 1);
        }

        uint8_t readByte = 0;
        for (uint8_t i = 0; i < 10; ++i) {
            CHECK(stream.read(&readByte, 1) == 1);
            CHECK(readByte == i);
        }

        // Stream should now be empty
        CHECK(stream.read(&readByte, 1) == 0);
    }

}


TEST_CASE("byte stream memory basic operations") {
    const int BYTES_PER_FRAME = 3 * 10 * 10; // Assuming a 10x10 RGB video
    
    // Create a ByteStreamMemory
    const uint32_t BUFFER_SIZE = BYTES_PER_FRAME * 10; // Enough for 10 frames
    fl::ByteStreamMemoryPtr memoryStream = fl::make_shared<fl::ByteStreamMemory>(BUFFER_SIZE);

    // Fill the ByteStreamMemory with test data
    uint8_t testData[BUFFER_SIZE];
    for (uint32_t i = 0; i < BUFFER_SIZE; ++i) {
        testData[i] = static_cast<uint8_t>(i % 256);
    }
    memoryStream->write(testData, BUFFER_SIZE);

    // Create and initialize PixelStream
    fl::PixelStreamPtr stream = fl::make_shared<fl::PixelStream>(BYTES_PER_FRAME);
    bool initSuccess = stream->beginStream(memoryStream);
    REQUIRE(initSuccess);

    // Test basic properties
    CHECK(stream->getType() == fl::PixelStream::kStreaming);
    CHECK(stream->bytesPerFrame() == BYTES_PER_FRAME);

    // Read a pixel
    CRGB pixel;
    bool readSuccess = stream->readPixel(&pixel);
    REQUIRE(readSuccess);
    CHECK(pixel.r == 0);
    CHECK(pixel.g == 1);
    CHECK(pixel.b == 2);

    // Read some bytes
    uint8_t buffer[10];
    size_t bytesRead = stream->readBytes(buffer, 10);
    CHECK(bytesRead == 10);
    for (int i = 0; i < 10; ++i) {
        CHECK(buffer[i] == static_cast<uint8_t>((i + 3) % 256));
    }

    // Check frame counting - streaming mode doesn't support this.
    //CHECK(PixelStream->framesDisplayed() == 0);
    //CHECK(PixelStream->framesRemaining() == 10); // We have 10 frames of data

    // Close the stream
    stream->close();
}

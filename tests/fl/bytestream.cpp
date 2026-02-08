// Compile with: g++ --std=c++11 test.cpp


#include "fl/bytestreammemory.h"
#include "fl/fx/video/pixel_stream.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "test.h"
#include "fl/fx/video.h"
#include "fl/stl/shared_ptr.h"
#include "hsv2rgb.h"

FL_TEST_CASE("ByteStreamMemory basic operations") {

    FL_SUBCASE("Write and read single byte") {
        fl::ByteStreamMemory stream(10);  // Stream with 10 bytes capacity
        uint8_t testByte = 42;
        FL_CHECK(stream.write(&testByte, 1) == 1);
        
        uint8_t readByte = 0;
        FL_CHECK(stream.read(&readByte, 1) == 1);
        FL_CHECK(readByte == testByte);

        // Next read should fail since the stream is empty
        FL_CHECK(stream.read(&readByte, 1) == 0);
    }

    FL_SUBCASE("Write and read multiple bytes") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3, 4, 5};
        FL_CHECK(stream.write(testData, 5) == 5);
        
        uint8_t readData[5] = {0};
        FL_CHECK(stream.read(readData, 5) == 5);
        for (int i = 0; i < 5; ++i) {
            FL_CHECK(readData[i] == testData[i]);
        }
    }

    FL_SUBCASE("Attempt to read from empty stream") {
        fl::ByteStreamMemory stream(10);
        uint8_t readByte = 0;
        FL_CHECK(stream.read(&readByte, 1) == 0);
    }

    FL_SUBCASE("Attempt to write beyond capacity") {
        fl::ByteStreamMemory stream(5);
        uint8_t testData[] = {1, 2, 3, 4, 5, 6};
        FL_CHECK(stream.write(testData, 6) == 5);  // Should write up to capacity
    }

    FL_SUBCASE("Attempt to read more than available data") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3};
        FL_CHECK(stream.write(testData, 3) == 3);

        uint8_t readData[5] = {0};
        FL_CHECK(stream.read(readData, 5) == 3);  // Should read only available data
        //FL_CHECK(readData[0] == 1);
        //FL_CHECK(readData[1] == 2);
        //FL_CHECK(readData[2] == 3);
    }

    FL_SUBCASE("Multiple write and read operations") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData1[] = {1, 2, 3};
        uint8_t testData2[] = {4, 5};
        FL_CHECK(stream.write(testData1, 3) == 3);
        FL_CHECK(stream.write(testData2, 2) == 2);

        uint8_t readData[5] = {0};
        FL_CHECK(stream.read(readData, 5) == 5);
        FL_CHECK(readData[0] == 1);
        FL_CHECK(readData[1] == 2);
        FL_CHECK(readData[2] == 3);
        FL_CHECK(readData[3] == 4);
        FL_CHECK(readData[4] == 5);
    }


    FL_SUBCASE("Write after partial read") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3, 4, 5};
        FL_CHECK(stream.write(testData, 5) == 5);

        uint8_t readData[2] = {0};
        FL_CHECK(stream.read(readData, 2) == 2);
        FL_CHECK(readData[0] == 1);
        FL_CHECK(readData[1] == 2);

        uint8_t newTestData[] = {6, 7};
        FL_CHECK(stream.write(newTestData, 2) == 2);

        uint8_t remainingData[5] = {0};
        FL_CHECK(stream.read(remainingData, 5) == 5);
        FL_CHECK(remainingData[0] == 3);
        FL_CHECK(remainingData[1] == 4);
        FL_CHECK(remainingData[2] == 5);
        FL_CHECK(remainingData[3] == 6);
        FL_CHECK(remainingData[4] == 7);
    }

    FL_SUBCASE("Fill and empty stream multiple times") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[10];
        for (uint8_t i = 0; i < 10; ++i) {
            testData[i] = i;
        }

        // First cycle
        FL_CHECK(stream.write(testData, 10) == 10);
        uint8_t readData[10] = {0};
        FL_CHECK(stream.read(readData, 10) == 10);
        for (uint8_t i = 0; i < 10; ++i) {
            FL_CHECK(readData[i] == i);
        }

        // Second cycle
        FL_CHECK(stream.write(testData, 10) == 10);
        FL_CHECK(stream.read(readData, 10) == 10);
        for (uint8_t i = 0; i < 10; ++i) {
            FL_CHECK(readData[i] == i);
        }
    }

    FL_SUBCASE("Zero-length write and read") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3};
        FL_CHECK(stream.write(testData, 0) == 0);

        uint8_t readData[3] = {0};
        FL_CHECK(stream.read(readData, 0) == 0);
    }

    FL_SUBCASE("Write and read with null pointers") {
        fl::ByteStreamMemory stream(10);
        FL_CHECK(stream.write(static_cast<uint8_t*>(nullptr), 5) == 0);
        FL_CHECK(stream.read(nullptr, 5) == 0);
    }

    FL_SUBCASE("Boundary conditions") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[10];
        for (uint8_t i = 0; i < 10; ++i) {
            testData[i] = i;
        }
        FL_CHECK(stream.write(testData, 10) == 10);

        uint8_t readData[10] = {0};
        FL_CHECK(stream.read(readData, 10) == 10);
        for (uint8_t i = 0; i < 10; ++i) {
            FL_CHECK(readData[i] == i);
        }

        // Try to write again to full capacity
        FL_CHECK(stream.write(testData, 10) == 10);
    }

    FL_SUBCASE("Write with partial capacity") {
        fl::ByteStreamMemory stream(5);
        uint8_t testData[] = {1, 2, 3, 4, 5};
        FL_CHECK(stream.write(testData, 5) == 5);

        uint8_t moreData[] = {6, 7};
        FL_CHECK(stream.write(moreData, 2) == 0);  // Should not write since capacity is full

        uint8_t readData[5] = {0};
        FL_CHECK(stream.read(readData, 5) == 5);
        for (int i = 0; i < 5; ++i) {
            FL_CHECK(readData[i] == testData[i]);
        }

        // Now buffer is empty, try writing again
        FL_CHECK(stream.write(moreData, 2) == 2);
        FL_CHECK(stream.read(readData, 2) == 2);
        FL_CHECK(readData[0] == 6);
        FL_CHECK(readData[1] == 7);
    }

    FL_SUBCASE("Read after buffer is reset") {
        fl::ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3};
        FL_CHECK(stream.write(testData, 3) == 3);

        stream.clear();  // Assuming reset clears the buffer

        uint8_t readData[3] = {0};
        FL_CHECK(stream.read(readData, 3) == 0);  // Should read nothing
    }

    FL_SUBCASE("Write zero bytes when buffer is full") {
        fl::ByteStreamMemory stream(0);  // Zero capacity
        uint8_t testByte = 42;
        FL_CHECK(stream.write(&testByte, 1) == 0);  // Cannot write to zero-capacity buffer
    }

    FL_SUBCASE("Sequential writes and reads") {
        fl::ByteStreamMemory stream(10);
        for (uint8_t i = 0; i < 10; ++i) {
            FL_CHECK(stream.write(&i, 1) == 1);
        }

        uint8_t readByte = 0;
        for (uint8_t i = 0; i < 10; ++i) {
            FL_CHECK(stream.read(&readByte, 1) == 1);
            FL_CHECK(readByte == i);
        }

        // Stream should now be empty
        FL_CHECK(stream.read(&readByte, 1) == 0);
    }

}


FL_TEST_CASE("byte stream memory basic operations") {
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
    FL_REQUIRE(initSuccess);

    // Test basic properties
    FL_CHECK(stream->getType() == fl::PixelStream::kStreaming);
    FL_CHECK(stream->bytesPerFrame() == BYTES_PER_FRAME);

    // Read a pixel
    CRGB pixel;
    bool readSuccess = stream->readPixel(&pixel);
    FL_REQUIRE(readSuccess);
    FL_CHECK(pixel.r == 0);
    FL_CHECK(pixel.g == 1);
    FL_CHECK(pixel.b == 2);

    // Read some bytes
    uint8_t buffer[10];
    size_t bytesRead = stream->readBytes(buffer, 10);
    FL_CHECK(bytesRead == 10);
    for (int i = 0; i < 10; ++i) {
        FL_CHECK(buffer[i] == static_cast<uint8_t>((i + 3) % 256));
    }

    // Check frame counting - streaming mode doesn't support this.
    //FL_CHECK(PixelStream->framesDisplayed() == 0);
    //FL_CHECK(PixelStream->framesRemaining() == 10); // We have 10 frames of data

    // Close the stream
    stream->close();
}

// Compile with: g++ --std=c++11 test.cpp


#include "fl/stl/detail/memory_file_handle.h"
#include "fl/fx/video/pixel_stream.h"
#include "fl/stl/cstddef.h"
#include "fl/stl/stdint.h"
#include "fl/stl/span.h"
#include "test.h"
#include "fl/fx/video.h"
#include "fl/stl/shared_ptr.h"
#include "hsv2rgb.h"

FL_TEST_CASE("memorybuf basic operations") {

    FL_SUBCASE("Write and read single byte") {
        fl::memorybuf stream(10);  // Stream with 10 bytes capacity
        fl::u8 testByte = 42;
        FL_CHECK(stream.write(fl::span<const fl::u8>(&testByte, 1)) == 1);

        fl::u8 readByte = 0;
        FL_CHECK(stream.read(&readByte, 1) == 1);
        FL_CHECK(readByte == testByte);

        // Next read should fail since the stream is empty
        FL_CHECK(stream.read(&readByte, 1) == 0);
    }

    FL_SUBCASE("Write and read multiple bytes") {
        fl::memorybuf stream(10);
        fl::u8 testData[] = {1, 2, 3, 4, 5};
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData, 5)) == 5);

        fl::u8 readData[5] = {0};
        FL_CHECK(stream.read(readData, 5) == 5);
        for (int i = 0; i < 5; ++i) {
            FL_CHECK(readData[i] == testData[i]);
        }
    }

    FL_SUBCASE("Attempt to read from empty stream") {
        fl::memorybuf stream(10);
        fl::u8 readByte = 0;
        FL_CHECK(stream.read(&readByte, 1) == 0);
    }

    FL_SUBCASE("Attempt to write beyond capacity") {
        fl::memorybuf stream(5);
        fl::u8 testData[] = {1, 2, 3, 4, 5, 6};
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData, 6)) == 5);  // Should write up to capacity
    }

    FL_SUBCASE("Attempt to read more than available data") {
        fl::memorybuf stream(10);
        fl::u8 testData[] = {1, 2, 3};
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData, 3)) == 3);

        fl::u8 readData[5] = {0};
        FL_CHECK(stream.read(readData, 5) == 3);  // Should read only available data
    }

    FL_SUBCASE("Multiple write and read operations") {
        fl::memorybuf stream(10);
        fl::u8 testData1[] = {1, 2, 3};
        fl::u8 testData2[] = {4, 5};
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData1, 3)) == 3);
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData2, 2)) == 2);

        fl::u8 readData[5] = {0};
        FL_CHECK(stream.read(readData, 5) == 5);
        FL_CHECK(readData[0] == 1);
        FL_CHECK(readData[1] == 2);
        FL_CHECK(readData[2] == 3);
        FL_CHECK(readData[3] == 4);
        FL_CHECK(readData[4] == 5);
    }


    FL_SUBCASE("Write after partial read") {
        fl::memorybuf stream(10);
        fl::u8 testData[] = {1, 2, 3, 4, 5};
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData, 5)) == 5);

        fl::u8 readData[2] = {0};
        FL_CHECK(stream.read(readData, 2) == 2);
        FL_CHECK(readData[0] == 1);
        FL_CHECK(readData[1] == 2);

        fl::u8 newTestData[] = {6, 7};
        FL_CHECK(stream.write(fl::span<const fl::u8>(newTestData, 2)) == 2);

        fl::u8 remainingData[5] = {0};
        FL_CHECK(stream.read(remainingData, 5) == 5);
        FL_CHECK(remainingData[0] == 3);
        FL_CHECK(remainingData[1] == 4);
        FL_CHECK(remainingData[2] == 5);
        FL_CHECK(remainingData[3] == 6);
        FL_CHECK(remainingData[4] == 7);
    }

    FL_SUBCASE("Fill and empty stream multiple times") {
        fl::memorybuf stream(10);
        fl::u8 testData[10];
        for (fl::u8 i = 0; i < 10; ++i) {
            testData[i] = i;
        }

        // First cycle
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData, 10)) == 10);
        fl::u8 readData[10] = {0};
        FL_CHECK(stream.read(readData, 10) == 10);
        for (fl::u8 i = 0; i < 10; ++i) {
            FL_CHECK(readData[i] == i);
        }

        // Second cycle
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData, 10)) == 10);
        FL_CHECK(stream.read(readData, 10) == 10);
        for (fl::u8 i = 0; i < 10; ++i) {
            FL_CHECK(readData[i] == i);
        }
    }

    FL_SUBCASE("Zero-length write and read") {
        fl::memorybuf stream(10);
        FL_CHECK(stream.write(fl::span<const fl::u8>()) == 0);

        fl::u8 readData[3] = {0};
        FL_CHECK(stream.read(readData, 0) == 0);
    }

    FL_SUBCASE("Write and read with null pointers") {
        fl::memorybuf stream(10);
        FL_CHECK(stream.write(static_cast<const char*>(nullptr), 5) == 0);
        FL_CHECK(stream.read(static_cast<char*>(nullptr), 5) == 0);
    }

    FL_SUBCASE("Boundary conditions") {
        fl::memorybuf stream(10);
        fl::u8 testData[10];
        for (fl::u8 i = 0; i < 10; ++i) {
            testData[i] = i;
        }
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData, 10)) == 10);

        fl::u8 readData[10] = {0};
        FL_CHECK(stream.read(readData, 10) == 10);
        for (fl::u8 i = 0; i < 10; ++i) {
            FL_CHECK(readData[i] == i);
        }

        // Try to write again to full capacity
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData, 10)) == 10);
    }

    FL_SUBCASE("Write with partial capacity") {
        fl::memorybuf stream(5);
        fl::u8 testData[] = {1, 2, 3, 4, 5};
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData, 5)) == 5);

        fl::u8 moreData[] = {6, 7};
        FL_CHECK(stream.write(fl::span<const fl::u8>(moreData, 2)) == 0);  // Should not write since capacity is full

        fl::u8 readData[5] = {0};
        FL_CHECK(stream.read(readData, 5) == 5);
        for (int i = 0; i < 5; ++i) {
            FL_CHECK(readData[i] == testData[i]);
        }

        // Now buffer is empty, try writing again
        FL_CHECK(stream.write(fl::span<const fl::u8>(moreData, 2)) == 2);
        FL_CHECK(stream.read(readData, 2) == 2);
        FL_CHECK(readData[0] == 6);
        FL_CHECK(readData[1] == 7);
    }

    FL_SUBCASE("Read after buffer is reset") {
        fl::memorybuf stream(10);
        fl::u8 testData[] = {1, 2, 3};
        FL_CHECK(stream.write(fl::span<const fl::u8>(testData, 3)) == 3);

        stream.clear();  // Assuming reset clears the buffer

        fl::u8 readData[3] = {0};
        FL_CHECK(stream.read(readData, 3) == 0);  // Should read nothing
    }

    FL_SUBCASE("Write zero bytes when buffer is full") {
        fl::memorybuf stream(0);  // Zero capacity
        fl::u8 testByte = 42;
        FL_CHECK(stream.write(fl::span<const fl::u8>(&testByte, 1)) == 0);  // Cannot write to zero-capacity buffer
    }

    FL_SUBCASE("Sequential writes and reads") {
        fl::memorybuf stream(10);
        for (fl::u8 i = 0; i < 10; ++i) {
            FL_CHECK(stream.write(fl::span<const fl::u8>(&i, 1)) == 1);
        }

        fl::u8 readByte = 0;
        for (fl::u8 i = 0; i < 10; ++i) {
            FL_CHECK(stream.read(&readByte, 1) == 1);
            FL_CHECK(readByte == i);
        }

        // Stream should now be empty
        FL_CHECK(stream.read(&readByte, 1) == 0);
    }

}


FL_TEST_CASE("memory file handle with pixel stream") {
    const int BYTES_PER_FRAME = 3 * 10 * 10; // Assuming a 10x10 RGB video

    // Create a memorybuf
    const fl::u32 BUFFER_SIZE = BYTES_PER_FRAME * 10; // Enough for 10 frames
    fl::memorybufPtr memoryStream = fl::make_shared<fl::memorybuf>(BUFFER_SIZE);

    // Fill the memorybuf with test data
    fl::u8 testData[BUFFER_SIZE];
    for (fl::u32 i = 0; i < BUFFER_SIZE; ++i) {
        testData[i] = static_cast<fl::u8>(i % 256);
    }
    memoryStream->write(fl::span<const fl::u8>(testData, BUFFER_SIZE));

    // Create and initialize PixelStream
    fl::PixelStreamPtr stream = fl::make_shared<fl::PixelStream>(BYTES_PER_FRAME);
    bool initSuccess = stream->begin(memoryStream);
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
    fl::u8 buffer[10];
    size_t bytesRead = stream->readBytes(buffer, 10);
    FL_CHECK(bytesRead == 10);
    for (int i = 0; i < 10; ++i) {
        FL_CHECK(buffer[i] == static_cast<fl::u8>((i + 3) % 256));
    }

    // Close the stream
    stream->close();
}

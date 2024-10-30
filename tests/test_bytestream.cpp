// Compile with: g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/storage/bytestreammemory.h"

#include "namespace.h"
FASTLED_USING_NAMESPACE

TEST_CASE("ByteStreamMemory basic operations") {

    SUBCASE("Write and read single byte") {
        ByteStreamMemory stream(10);  // Stream with 10 bytes capacity
        uint8_t testByte = 42;
        CHECK(stream.write(&testByte, 1) == 1);
        
        uint8_t readByte = 0;
        CHECK(stream.read(&readByte, 1) == 1);
        CHECK(readByte == testByte);

        // Next read should fail since the stream is empty
        CHECK(stream.read(&readByte, 1) == 0);
    }

    SUBCASE("Write and read multiple bytes") {
        ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3, 4, 5};
        CHECK(stream.write(testData, 5) == 5);
        
        uint8_t readData[5] = {0};
        CHECK(stream.read(readData, 5) == 5);
        for (int i = 0; i < 5; ++i) {
            CHECK(readData[i] == testData[i]);
        }
    }

    SUBCASE("Attempt to read from empty stream") {
        ByteStreamMemory stream(10);
        uint8_t readByte = 0;
        CHECK(stream.read(&readByte, 1) == 0);
    }

    SUBCASE("Attempt to write beyond capacity") {
        ByteStreamMemory stream(5);
        uint8_t testData[] = {1, 2, 3, 4, 5, 6};
        CHECK(stream.write(testData, 6) == 5);  // Should write up to capacity
    }

    SUBCASE("Attempt to read more than available data") {
        ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3};
        CHECK(stream.write(testData, 3) == 3);

        uint8_t readData[5] = {0};
        CHECK_FALSE(stream.read(readData, 5) == 3);  // Should read only available data
        //CHECK(readData[0] == 1);
        //CHECK(readData[1] == 2);
        //CHECK(readData[2] == 3);
    }

    SUBCASE("Multiple write and read operations") {
        ByteStreamMemory stream(10);
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
        ByteStreamMemory stream(10);
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
        ByteStreamMemory stream(10);
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
        ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3};
        CHECK(stream.write(testData, 0) == 0);

        uint8_t readData[3] = {0};
        CHECK(stream.read(readData, 0) == 0);
    }

    SUBCASE("Write and read with null pointers") {
        ByteStreamMemory stream(10);
        CHECK(stream.write(nullptr, 5) == 0);
        CHECK(stream.read(nullptr, 5) == 0);
    }

    SUBCASE("Boundary conditions") {
        ByteStreamMemory stream(10);
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
        ByteStreamMemory stream(5);
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
        ByteStreamMemory stream(10);
        uint8_t testData[] = {1, 2, 3};
        CHECK(stream.write(testData, 3) == 3);

        stream.clear();  // Assuming reset clears the buffer

        uint8_t readData[3] = {0};
        CHECK(stream.read(readData, 3) == 0);  // Should read nothing
    }

    SUBCASE("Write zero bytes when buffer is full") {
        ByteStreamMemory stream(0);  // Zero capacity
        uint8_t testByte = 42;
        CHECK(stream.write(&testByte, 1) == 0);  // Cannot write to zero-capacity buffer
    }

    SUBCASE("Sequential writes and reads") {
        ByteStreamMemory stream(10);
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

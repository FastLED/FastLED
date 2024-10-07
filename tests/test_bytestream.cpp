
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/storage/bytestreammemory.h"


TEST_CASE("bytestreammemory basic operations") {


    SUBCASE("Write and read single byte") {
        ByteStreamMemory stream(10);  // Create a stream with 10 bytes capacity
        uint8_t testByte = 42;
        CHECK(stream.write(&testByte, 1) == 1);
        
        uint8_t readByte = 0;
        CHECK(stream.read(&readByte, 1) == 1);
        CHECK(readByte == testByte);
    }

    SUBCASE("Write and read multiple bytes") {
        ByteStreamMemory stream(10);  // Create a stream with 10 bytes capacity
        uint8_t testData[] = {1, 2, 3, 4, 5};
        CHECK(stream.write(testData, 5) == 5);
        
        uint8_t readData[5] = {0};
        CHECK(stream.read(readData, 5) == 5);
        for (int i = 0; i < 5; ++i) {
            CHECK(readData[i] == testData[i]);
        }
    }
}


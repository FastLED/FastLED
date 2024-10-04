
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/storage/sd.h"

TEST_CASE("Compile test") {
    #if !FASTLED_HAS_SD
    #warning "No SD card support"
    #endif
}



// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/framebuffer.h"

TEST_CASE("Test compilation") {
    FrameBuffer fb(10);
}


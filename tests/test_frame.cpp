
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "fx/frame.h"
#include <cstdlib>

namespace {
    int allocation_count = 0;

    void* custom_malloc(size_t size) {
        allocation_count++;
        return std::malloc(size);
    }

    void custom_free(void* ptr) {
        allocation_count--;
        std::free(ptr);
    }
}

TEST_CASE("test frame custom allocator") {
    // Set our custom allocator
    Frame::SetAllocator(custom_malloc, custom_free);

    SUBCASE("Custom allocator is used") {
        {
            FramePtr frame = FramePtr::New(100, true);  // 100 pixels with alpha channel
            CHECK(allocation_count == 2);  // One for RGB, one for alpha
        }
        // Frame should be destroyed here
        CHECK(allocation_count == 0);
    }
}


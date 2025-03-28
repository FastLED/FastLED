
// g++ --std=c++11 test.cpp

#include "test.h"

#include "test.h"
#include "fx/frame.h"
#include <cstdlib>
#include "fl/allocator.h"

#include "fl/namespace.h"
FASTLED_USING_NAMESPACE

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
    SetLargeBlockAllocator(custom_malloc, custom_free);
    
    FramePtr frame = FramePtr::New(100);  // 100 pixels.
    CHECK(allocation_count == 1);  // One for RGB.
    frame.reset();

    // Frame should be destroyed here
    CHECK(allocation_count == 0);
}


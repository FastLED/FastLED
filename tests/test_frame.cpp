
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
    SetPSRamAllocator(custom_malloc, custom_free);
    
    FramePtr frame = fl::make_shared<Frame>(100);  // 100 pixels.
    CHECK(allocation_count == 1);  // One for RGB.
    frame.reset();

    // Frame should be destroyed here
    CHECK(allocation_count == 0);
}


TEST_CASE("test blend by black") {
    SetPSRamAllocator(custom_malloc, custom_free);
    FramePtr frame = fl::make_shared<Frame>(1);  // 1 pixels.
    frame->rgb()[0] = CRGB(255, 0, 0);  // Red
    CRGB out;
    frame->draw(&out, DRAW_MODE_BLEND_BY_MAX_BRIGHTNESS);
    CHECK(out == CRGB(255, 0, 0));  // full red because max luma is 255
    out = CRGB(0, 0, 0);
    frame->rgb()[0] = CRGB(128, 0, 0);  // Red
    frame->draw(&out, DRAW_MODE_BLEND_BY_MAX_BRIGHTNESS);
    CHECK(out == CRGB(64, 0, 0));
}

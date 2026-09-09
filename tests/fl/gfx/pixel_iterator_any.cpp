/// @file tests/fl/gfx/pixel_iterator_any.cpp
/// @brief `PixelIteratorAny` must survive being copied or moved (#4201).

#include "fl/gfx/pixel_iterator_any.h"
#include "fl/stl/int.h"
#include "pixel_controller.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

/// Read three bytes per pixel through an adapter, in wire order.
void drain(PixelIteratorAny& adapter, int count, fl::vector<u8>* out) {
    PixelIterator& iterator = adapter.get();
    for (int i = 0; i < count && iterator.has(1); ++i) {
        u8 r = 0;
        u8 g = 0;
        u8 b = 0;
        iterator.loadAndScaleRGB(&r, &g, &b);
        out->push_back(r);
        out->push_back(g);
        out->push_back(b);
        iterator.advanceData();
    }
}

}  // namespace

FL_TEST_CASE("[#4201] A moved PixelIteratorAny reads its own controller") {
    // This class is self-referential: it holds the controller by value in
    // `mAnyController`, and the type-erased `PixelIterator` inside it holds a
    // raw pointer *into that member*. The compiler-generated move carried
    // that pointer across unchanged, so the destination kept reading the
    // source's controller -- and where the source was a temporary, as in
    // `Channel::showPixels`'s addressing branch, that was a read of dead
    // stack on every addressed frame. ASAN called it stack-use-after-scope.
    //
    // Assigning from a temporary inside a scope that then ends is the shape
    // that crashed; reading the right bytes afterwards is the claim.
    CRGB leds[2] = {CRGB(10, 20, 30), CRGB(40, 50, 60)};

    PixelController<RGB> source(leds, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    PixelIteratorAny adapter(source, RGB, Rgbw());
    {
        PixelController<RGB> temporary(leds, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
        adapter = PixelIteratorAny(temporary, RGB, Rgbw());
    }

    fl::vector<u8> bytes;
    drain(adapter, 2, &bytes);

    FL_REQUIRE(bytes.size() == 6u);
    FL_CHECK_EQ(static_cast<int>(bytes[0]), 10);
    FL_CHECK_EQ(static_cast<int>(bytes[1]), 20);
    FL_CHECK_EQ(static_cast<int>(bytes[2]), 30);
    FL_CHECK_EQ(static_cast<int>(bytes[3]), 40);
    FL_CHECK_EQ(static_cast<int>(bytes[4]), 50);
    FL_CHECK_EQ(static_cast<int>(bytes[5]), 60);
}

FL_TEST_CASE("[#4201] A copied PixelIteratorAny is independent of its source") {
    // The copy must own its iteration, not share the original's position --
    // which it would if the copied pointer still aimed at the original's
    // controller.
    CRGB leds[2] = {CRGB(1, 2, 3), CRGB(4, 5, 6)};
    PixelController<RGB> source(leds, 2, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    PixelIteratorAny original(source, RGB, Rgbw());

    // Consume the first pixel through the original.
    fl::vector<u8> first;
    drain(original, 1, &first);
    FL_REQUIRE(first.size() == 3u);

    PixelIteratorAny copy(original);

    // The copy resumes where the original was, and advancing it must not
    // move the original.
    fl::vector<u8> from_copy;
    drain(copy, 1, &from_copy);
    fl::vector<u8> from_original;
    drain(original, 1, &from_original);

    FL_REQUIRE(from_copy.size() == 3u);
    FL_REQUIRE(from_original.size() == 3u);
    FL_CHECK_EQ(static_cast<int>(from_copy[0]), 4);
    FL_CHECK_EQ(static_cast<int>(from_original[0]), 4);
}

FL_TEST_CASE("[#4201] Colour order survives the copy") {
    // The order lives in which alternative `mAnyController` holds, so a copy
    // that re-aims the iterator has to re-aim it at the *same* alternative.
    CRGB leds[1] = {CRGB(10, 20, 30)};
    PixelController<RGB> source(leds, 1, ColorAdjustment::noAdjustment(), DISABLE_DITHER);
    PixelIteratorAny original(source, BGR, Rgbw());
    PixelIteratorAny copy(original);

    fl::vector<u8> bytes;
    drain(copy, 1, &bytes);
    FL_REQUIRE(bytes.size() == 3u);
    FL_CHECK_EQ(static_cast<int>(bytes[0]), 30);
    FL_CHECK_EQ(static_cast<int>(bytes[1]), 20);
    FL_CHECK_EQ(static_cast<int>(bytes[2]), 10);
}

}  // FL_TEST_FILE

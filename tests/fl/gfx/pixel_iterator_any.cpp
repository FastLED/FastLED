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


// ===========================================================================
// One PixelController<RGB> for every order must equal six of them (#4402).
//
// The oracle is the legacy path itself: a PixelIterator over a real
// PixelController<ORDER>, which is what PixelIteratorAny used to build and
// what every templated driver still builds. Binary dither on, and per-channel
// scales that are all different, so a wrong permutation cannot hide behind a
// symmetric input.
// ===========================================================================

namespace {

const EOrder kAllOrders[6] = {RGB, RBG, GRB, GBR, BRG, BGR};

ColorAdjustment lopsidedAdjustment() {
    ColorAdjustment adj = ColorAdjustment::noAdjustment();
    adj.premixed = CRGB(230, 140, 60);
#if FASTLED_HD_COLOR_MIXING
    // `color` only exists in the HD build; premixed is the one every build has.
    adj.color = CRGB(230, 140, 60);
#endif
    return adj;
}

CRGB* fixture(CRGB (&leds)[6]) {
    leds[0] = CRGB(10, 200, 33);
    leds[1] = CRGB(0, 0, 0);
    leds[2] = CRGB(255, 255, 255);
    leds[3] = CRGB(7, 8, 9);
    leds[4] = CRGB(128, 64, 32);
    leds[5] = CRGB(1, 2, 254);
    return leds;
}

void drainRGB(PixelIterator& it, fl::vector<u8>* out) {
    while (it.has(1)) {
        u8 c[3];
        it.loadAndScaleRGB(&c[0], &c[1], &c[2]);
        for (int i = 0; i < 3; ++i) { out->push_back(c[i]); }
        it.stepDithering();
        it.advanceData();
    }
}
void drainRGBW(PixelIterator& it, fl::vector<u8>* out) {
    while (it.has(1)) {
        u8 c[4];
        it.loadAndScaleRGBW(&c[0], &c[1], &c[2], &c[3]);
        for (int i = 0; i < 4; ++i) { out->push_back(c[i]); }
        it.stepDithering();
        it.advanceData();
    }
}
void drainRGBWW(PixelIterator& it, fl::vector<u8>* out) {
    while (it.has(1)) {
        u8 c[5];
        it.loadAndScaleRGBWW(&c[0], &c[1], &c[2], &c[3], &c[4]);
        for (int i = 0; i < 5; ++i) { out->push_back(c[i]); }
        it.stepDithering();
        it.advanceData();
    }
}

template <EOrder ORDER, typename Drain>
fl::vector<u8> oracle(PixelController<RGB>& source, Rgbw rgbw, Rgbww rgbww, Drain drain) {
    PixelController<ORDER> ordered(source);   // the legacy route
    PixelIterator it(&ordered, rgbw, rgbww);  // identity permutation
    fl::vector<u8> out;
    drain(it, &out);
    return out;
}

template <typename Drain>
fl::vector<u8> oracleFor(EOrder order, PixelController<RGB>& source, Rgbw rgbw, Rgbww rgbww, Drain drain) {
    switch (order) {
        case RGB: return oracle<RGB>(source, rgbw, rgbww, drain);
        case RBG: return oracle<RBG>(source, rgbw, rgbww, drain);
        case GRB: return oracle<GRB>(source, rgbw, rgbww, drain);
        case GBR: return oracle<GBR>(source, rgbw, rgbww, drain);
        case BRG: return oracle<BRG>(source, rgbw, rgbww, drain);
        case BGR: return oracle<BGR>(source, rgbw, rgbww, drain);
    }
    return fl::vector<u8>();
}

template <typename Drain>
void checkAllOrders(Rgbw rgbw, Rgbww rgbww, Drain drain, const char* what) {
    CRGB leds[6];
    fixture(leds);
    for (int oi = 0; oi < 6; ++oi) {
        const EOrder order = kAllOrders[oi];
        PixelController<RGB> a(leds, 6, lopsidedAdjustment(), BINARY_DITHER);
        PixelController<RGB> b(leds, 6, lopsidedAdjustment(), BINARY_DITHER);

        PixelIteratorAny any(a, order, rgbw, rgbww);
        fl::vector<u8> got;
        drain(any.get(), &got);

        const fl::vector<u8> want = oracleFor(order, b, rgbw, rgbww, drain);

        FL_INFO(what << " order=" << static_cast<int>(order));
        FL_REQUIRE_EQ(got.size(), want.size());
        FL_REQUIRE(got.size() > 0u);
        for (fl::size i = 0; i < got.size(); ++i) {
            FL_CHECK_EQ(static_cast<int>(got[i]), static_cast<int>(want[i]));
        }
    }
}

}  // namespace

FL_TEST_CASE("[#4402] one PixelController<RGB> equals PixelController<ORDER> for RGB, all six orders") {
    checkAllOrders(Rgbw(), RgbwwInvalid::value(), drainRGB, "rgb");
}

FL_TEST_CASE("[#4402] ... and for RGBW, every W placement") {
    const EOrderW placements[4] = {EOrderW::W3, EOrderW::W2, EOrderW::W1, EOrderW::W0};
    for (int pi = 0; pi < 4; ++pi) {
        Rgbw rgbw(fl::kRGBWDefaultColorTemp, fl::RGBW_MODE::kRGBWExactColors, placements[pi]);
        checkAllOrders(rgbw, RgbwwInvalid::value(), drainRGBW, "rgbw");
    }
}

FL_TEST_CASE("[#4402] ... and for RGBWW") {
    checkAllOrders(Rgbw(), Rgbww(), drainRGBWW, "rgbww");
}

#if FASTLED_HD_COLOR_MIXING
FL_TEST_CASE("[#4402] ... and the HD per-slot scale follows the same permutation") {
    CRGB leds[6];
    fixture(leds);
    for (int oi = 0; oi < 6; ++oi) {
        const EOrder order = kAllOrders[oi];
        PixelController<RGB> a(leds, 6, lopsidedAdjustment(), BINARY_DITHER);
        PixelIteratorAny any(a, order, Rgbw());
        u8 got[4];
        any.get().loadRGBScaleAndBrightness(&got[0], &got[1], &got[2], &got[3]);

        PixelController<RGB> b(leds, 6, lopsidedAdjustment(), BINARY_DITHER);
        u8 want[4];
        switch (order) {
            case RGB: { PixelController<RGB> o(b); PixelIterator it(&o, Rgbw()); it.loadRGBScaleAndBrightness(&want[0], &want[1], &want[2], &want[3]); break; }
            case RBG: { PixelController<RBG> o(b); PixelIterator it(&o, Rgbw()); it.loadRGBScaleAndBrightness(&want[0], &want[1], &want[2], &want[3]); break; }
            case GRB: { PixelController<GRB> o(b); PixelIterator it(&o, Rgbw()); it.loadRGBScaleAndBrightness(&want[0], &want[1], &want[2], &want[3]); break; }
            case GBR: { PixelController<GBR> o(b); PixelIterator it(&o, Rgbw()); it.loadRGBScaleAndBrightness(&want[0], &want[1], &want[2], &want[3]); break; }
            case BRG: { PixelController<BRG> o(b); PixelIterator it(&o, Rgbw()); it.loadRGBScaleAndBrightness(&want[0], &want[1], &want[2], &want[3]); break; }
            case BGR: { PixelController<BGR> o(b); PixelIterator it(&o, Rgbw()); it.loadRGBScaleAndBrightness(&want[0], &want[1], &want[2], &want[3]); break; }
        }
        for (int i = 0; i < 4; ++i) {
            FL_CHECK_EQ(static_cast<int>(got[i]), static_cast<int>(want[i]));
        }
    }
}
#endif

}  // FL_TEST_FILE

// Unit tests for fl/gfx/binary_dither.h (#4672): the temporal binary dither
// algorithm, its disabled policy, and its binding into PixelController.

#include "test.h"
#include "fl/gfx/binary_dither.h"
#include "fl/math/math.h"
#include "pixel_controller.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

// The setup exactly as PixelController::init_binary_dithering() computed it
// before the extraction, kept verbatim as the golden reference.
void legacyInit(u8 (&d)[3], u8 (&e)[3], const u8 (&scale)[3], u8 R) {
    u8 ditherBits = VIRTUAL_BITS;
    R &= (0x01 << ditherBits) - 1;
    u8 Q = 0;
    if (R & 0x01) { Q |= 0x80; }
    if (R & 0x02) { Q |= 0x40; }
    if (R & 0x04) { Q |= 0x20; }
    if (R & 0x08) { Q |= 0x10; }
    if (R & 0x10) { Q |= 0x08; }
    if (R & 0x20) { Q |= 0x04; }
    if (R & 0x40) { Q |= 0x02; }
    if (R & 0x80) { Q |= 0x01; }
    if (ditherBits < 8) {
        Q += 0x01 << (7 - ditherBits);
    }
    for (int i = 0; i < 3; ++i) {
        u8 s = scale[i];
        e[i] = s ? (256 / s) + 1 : 0;
        d[i] = fl::scale8(Q, e[i]);
#if (FASTLED_SCALE8_FIXED == 1)
        if (d[i]) (--d[i]);
#endif
        if (e[i]) --e[i];
    }
}

void initUniform(u8 (&d)[3], u8 (&e)[3], u8 s, u8 frame) {
    const u8 scale[3] = {s, s, s};
    BinaryDither::init(d, e, scale, frame);
}

}  // namespace

FL_TEST_CASE("BinaryDither - phase is the bit-reversed, centred frame") {
    // VIRTUAL_BITS = 3: an 8-frame cycle, offsets spread across the byte.
    FL_CHECK_EQ(VIRTUAL_BITS, 3);
    const u8 expected[8] = {16, 144, 80, 208, 48, 176, 112, 240};
    for (int f = 0; f < 8; ++f) {
        FL_CHECK_EQ(BinaryDither::phase(static_cast<u8>(f)), expected[f]);
    }
    // Only the low VIRTUAL_BITS of the frame matter.
    for (int f = 0; f < 256; ++f) {
        FL_CHECK_EQ(BinaryDither::phase(static_cast<u8>(f)),
                    expected[f & 7]);
    }
}

FL_TEST_CASE("BinaryDither - init matches the pre-extraction algorithm byte for byte") {
    for (int s = 0; s < 256; ++s) {
        for (int f = 0; f < 256; ++f) {
            const u8 scale[3] = {static_cast<u8>(s), static_cast<u8>(255 - s),
                                 static_cast<u8>(s ^ 0x5a)};
            u8 d[3], e[3], ld[3], le[3];
            BinaryDither::init(d, e, scale, static_cast<u8>(f));
            legacyInit(ld, le, scale, static_cast<u8>(f));
            for (int c = 0; c < 3; ++c) {
                FL_REQUIRE_EQ(d[c], ld[c]);
                FL_REQUIRE_EQ(e[c], le[c]);
            }
        }
    }
}

FL_TEST_CASE("BinaryDither - ranges and offsets") {
    // e = 256/s: one output step's worth of input at that scale.
    u8 d[3], e[3];
    initUniform(d, e, 51, 0);
    FL_CHECK_EQ(e[0], 5);
    initUniform(d, e, 255, 0);
    FL_CHECK_EQ(e[0], 1);
    initUniform(d, e, 128, 0);
    FL_CHECK_EQ(e[0], 2);

    // An unlit channel gets no dither.
    initUniform(d, e, 0, 3);
    FL_CHECK_EQ(e[0], 0);
    FL_CHECK_EQ(d[0], 0);

    // s = 1 wants a range of 256, which does not fit a byte; the
    // arithmetic wraps to no dither rather than a wrong range.
    initUniform(d, e, 1, 3);
    FL_CHECK_EQ(e[0], 0);
    FL_CHECK_EQ(d[0], 0);

    // The offset always lies within [0, e].
    for (int s = 0; s < 256; ++s) {
        for (int f = 0; f < 8; ++f) {
            initUniform(d, e, static_cast<u8>(s), static_cast<u8>(f));
            FL_REQUIRE_LE(d[0], e[0]);
        }
    }

    // The worked example from the header: 20% brightness, one cycle.
    const u8 cycle[8] = {0, 2, 1, 4, 0, 3, 2, 5};
    for (int f = 0; f < 8; ++f) {
        initUniform(d, e, 51, static_cast<u8>(f));
        FL_CHECK_EQ(d[0], cycle[f]);
    }
}

FL_TEST_CASE("BinaryDither - reseed lands where init would") {
    for (int s = 0; s < 256; ++s) {
        const u8 scale[3] = {static_cast<u8>(s), static_cast<u8>(s / 2),
                             static_cast<u8>(255 - s)};
        u8 d[3], e[3];
        BinaryDither::init(d, e, scale, 0);
        for (int f = 0; f < 256; ++f) {
            u8 fd[3], fe[3];
            BinaryDither::init(fd, fe, scale, static_cast<u8>(f));
            BinaryDither::reseed(d, e, static_cast<u8>(f));
            for (int c = 0; c < 3; ++c) {
                FL_REQUIRE_EQ(d[c], fd[c]);
                FL_REQUIRE_EQ(e[c], fe[c]);
            }
        }
    }
}

FL_TEST_CASE("BinaryDither - step toggles neighbours to the complement") {
    u8 d[3], e[3];
    const u8 scale[3] = {51, 24, 200};
    BinaryDither::init(d, e, scale, 3);
    const u8 d0[3] = {d[0], d[1], d[2]};

    BinaryDither::step(d, e);
    for (int c = 0; c < 3; ++c) {
        FL_CHECK_EQ(d[c], static_cast<u8>(e[c] - d0[c]));
        FL_CHECK_LE(d[c], e[c]);
    }
    BinaryDither::step(d, e);
    for (int c = 0; c < 3; ++c) {
        FL_CHECK_EQ(d[c], d0[c]);
    }

    // stepChannel touches only its own channel.
    BinaryDither::stepChannel(d, e, 1);
    FL_CHECK_EQ(d[0], d0[0]);
    FL_CHECK_EQ(d[1], static_cast<u8>(e[1] - d0[1]));
    FL_CHECK_EQ(d[2], d0[2]);
}

FL_TEST_CASE("BinaryDither - apply never lifts black and saturates") {
    for (int off = 0; off < 256; ++off) {
        FL_CHECK_EQ(BinaryDither::apply(0, static_cast<u8>(off)), 0);
        FL_CHECK_EQ(BinaryDither::apply(255, static_cast<u8>(off)), 255);
    }
    FL_CHECK_EQ(BinaryDither::apply(10, 3), 13);
    FL_CHECK_EQ(BinaryDither::apply(250, 10), 255);
}

FL_TEST_CASE("BinaryDither - active iff any channel has a range") {
    u8 d[3], e[3];
    BinaryDither::clear(d, e);
    FL_CHECK_FALSE(BinaryDither::active(e));
    for (int c = 0; c < 3; ++c) {
        FL_CHECK_EQ(d[c], 0);
    }
    const u8 scale[3] = {0, 0, 51};
    BinaryDither::init(d, e, scale, 0);
    FL_CHECK(BinaryDither::active(e));
}

FL_TEST_CASE("BinaryDither - one cycle averages to the sub-code value") {
    // What dithering is for. Hold one pixel for a full cycle and compare the
    // mean output to the exact scaled value, against the undithered output.
    // Across every source code the dithered error must be far smaller, and
    // no single code may be off by a whole output step.
    const u8 scales[] = {8, 16, 32, 51, 64, 96, 128, 200, 254};
    for (u8 s : scales) {
        double ditheredError = 0.0;
        double plainError = 0.0;
        double worst = 0.0;
        for (int v = 1; v < 256; ++v) {
            const double exact = v * (s + 1) / 256.0;  // scale8's own scale
            int sum = 0;
            for (int f = 0; f < 8; ++f) {
                u8 d[3], e[3];
                initUniform(d, e, s, static_cast<u8>(f));
                sum += fl::scale8(BinaryDither::apply(static_cast<u8>(v), d[0]), s);
            }
            const double err = fl::fabs(sum / 8.0 - exact);
            ditheredError += err;
            plainError += fl::fabs(fl::scale8(static_cast<u8>(v), s) - exact);
            if (err > worst) { worst = err; }
        }
        FL_CHECK_LT(ditheredError, plainError * 0.55);
        FL_CHECK_LT(worst, 1.0);
    }
}

FL_TEST_CASE("NoDither - clears garbage and changes nothing") {
    // Under NO_DITHERING the controller still hands d/e to hand-written
    // drivers that add them in asm; init must leave them zero, not whatever
    // was on the stack (#4672).
    u8 d[3] = {0xAA, 0xBB, 0xCC};
    u8 e[3] = {0x11, 0x22, 0x33};
    const u8 scale[3] = {51, 51, 51};
    NoDither::init(d, e, scale, 5);
    for (int c = 0; c < 3; ++c) {
        FL_CHECK_EQ(d[c], 0);
        FL_CHECK_EQ(e[c], 0);
    }
    FL_CHECK_FALSE(NoDither::active(e));

    e[0] = 7;  // even with a stray range, NoDither stays inert
    NoDither::step(d, e);
    NoDither::stepChannel(d, e, 0);
    NoDither::reseed(d, e, 3);
    FL_CHECK_EQ(d[0], 0);
    FL_CHECK_FALSE(NoDither::active(e));
    for (int b = 0; b < 256; ++b) {
        FL_CHECK_EQ(NoDither::apply(static_cast<u8>(b), 9), b);
    }
}

FL_TEST_CASE("PixelController - dither state comes from fl::Dither") {
    CRGB pixel(40, 40, 40);
    ColorAdjustment adjustment = ColorAdjustment::noAdjustment();
    adjustment.premixed = CRGB(51, 24, 200);

    PixelController<GRB> pixels(&pixel, 1, adjustment, BINARY_DITHER);
    u8 d[3], e[3];
    Dither::init(d, e, adjustment.premixed.raw, fl::detail::ditherFrame());
    for (int c = 0; c < 3; ++c) {
        FL_CHECK_EQ(pixels.d[c], d[c]);
        FL_CHECK_EQ(pixels.e[c], e[c]);
    }
    FL_CHECK_EQ(pixels.ditherActive(), Dither::active(e));

    PixelController<GRB> off(&pixel, 1, adjustment, DISABLE_DITHER);
    FL_CHECK_FALSE(off.ditherActive());
    for (int c = 0; c < 3; ++c) {
        FL_CHECK_EQ(off.d[c], 0);
        FL_CHECK_EQ(off.e[c], 0);
    }
}

} // FL_TEST_FILE

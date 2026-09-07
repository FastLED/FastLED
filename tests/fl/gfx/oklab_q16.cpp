// OKLab working-domain transform for color pipeline P7 (#4041).

#include "fl/gfx/oklab_q16.h"
#include "fl/math/math.h"
#include "fl/stl/int.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

constexpr float kQ16 = 65536.0f;

i32 toQ16(float v) {
    const float scaled = v * kQ16;
    return static_cast<i32>(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
}

float fromQ16(i32 raw) { return static_cast<float>(raw) / kQ16; }

/// Matches `signedCubeRoot` in colorutils: an out-of-gamut XYZ can drive a
/// cone response negative, and `powf` of a negative base is not a number.
float signedCubeRootF(float value) {
    const float magnitude = fl::powf(fl::fabsf(value), 1.0f / 3.0f);
    return value < 0.0f ? -magnitude : magnitude;
}

/// The float OKLab forward transform, written out independently so the
/// fixed-point path is checked against the definition rather than against
/// itself. Coefficients are Ottosson's, as in the implementation.
void referenceXyzToOklab(const float (&xyz)[3], float (&lab)[3]) {
    const float l = 0.8190224432164319f * xyz[0] + 0.3619062562801221f * xyz[1] -
                    0.12887378261216414f * xyz[2];
    const float m = 0.0329836671980271f * xyz[0] + 0.9292868468965546f * xyz[1] +
                    0.03614466816999844f * xyz[2];
    const float s = 0.048177199566046255f * xyz[0] +
                    0.26423952494422764f * xyz[1] + 0.6335478258136937f * xyz[2];
    // The grid below contains points with a negative cone response
    // deliberately, so the root has to be signed.
    const float lr = signedCubeRootF(l);
    const float mr = signedCubeRootF(m);
    const float sr = signedCubeRootF(s);
    lab[0] = 0.2104542553f * lr + 0.7936177850f * mr - 0.0040720468f * sr;
    lab[1] = 1.9779984951f * lr - 2.4285922050f * mr + 0.4505937099f * sr;
    lab[2] = 0.0259040371f * lr + 0.7827717662f * mr - 0.8086757660f * sr;
}

/// D65 white, the point every OKLab identity is anchored on.
constexpr float kD65[3] = {0.9504559270516716f, 1.0f, 1.0890577507598784f};

}  // namespace

FL_TEST_CASE("OKLab Q16 sends D65 white to lightness 1 with no chroma") {
    // The defining normalization of OKLab. If the matrices were transposed,
    // mis-scaled or paired with the wrong inverse, this is what would move.
    const i32 white[3] = {toQ16(kD65[0]), toQ16(kD65[1]), toQ16(kD65[2])};
    i32 lab[3];
    xyzToOklabQ16(white, lab);
    FL_CHECK_LT(fl::fabsf(fromQ16(lab[0]) - 1.0f), 0.001f);
    FL_CHECK_LT(fl::fabsf(fromQ16(lab[1])), 0.001f);
    FL_CHECK_LT(fl::fabsf(fromQ16(lab[2])), 0.001f);
}

FL_TEST_CASE("OKLab Q16 forward agrees with the float definition") {
    // A grid over the working domain rather than a handful of points, so a
    // coefficient wrong in one row cannot hide between samples.
    const float kSamples[] = {0.02f, 0.15f, 0.4f, 0.75f, 1.0f, 1.6f};
    for (float x : kSamples) {
        for (float y : kSamples) {
            for (float z : kSamples) {
                const float xyz[3] = {x, y, z};
                const i32 raw[3] = {toQ16(x), toQ16(y), toQ16(z)};
                i32 lab[3];
                xyzToOklabQ16(raw, lab);
                float want[3];
                referenceXyzToOklab(xyz, want);
                for (int i = 0; i < 3; ++i) {
                    FL_CHECK_LT(fl::fabsf(fromQ16(lab[i]) - want[i]), 0.002f);
                }
            }
        }
    }
}

FL_TEST_CASE("OKLab Q16 round-trips back to the XYZ it came from") {
    // Neither direction is exact -- both round to nearest and the cube root
    // truncates -- so this pins what a round trip actually costs rather than
    // pretending it is lossless.
    const float kSamples[] = {0.05f, 0.3f, 0.62f, 1.0f, 1.4f};
    float worst = 0.0f;
    for (float x : kSamples) {
        for (float y : kSamples) {
            for (float z : kSamples) {
                const i32 raw[3] = {toQ16(x), toQ16(y), toQ16(z)};
                i32 lab[3];
                i32 back[3];
                xyzToOklabQ16(raw, lab);
                oklabToXyzQ16(lab, back);
                for (int i = 0; i < 3; ++i) {
                    const float error = fl::fabsf(fromQ16(back[i] - raw[i]));
                    if (error > worst) {
                        worst = error;
                    }
                }
            }
        }
    }
    FL_CHECK_LT(worst, 32.0f / kQ16);
}

FL_TEST_CASE("OKLab Q16 inverse round-trips over the well-conditioned range") {
    // The other direction, over a grid that crosses the neutral axis in both
    // a and b. A sign error in one row of either inverse matrix survives the
    // forward test above but not this one.
    //
    // Lightness starts at 0.35 because below that the round trip is
    // genuinely worse -- see the test below, which pins why.
    const float kLightness[] = {0.4f, 0.6f, 0.85f};
    const float kChroma[] = {-0.15f, -0.05f, 0.0f, 0.05f, 0.15f};
    for (float lightness : kLightness) {
        for (float a : kChroma) {
            for (float b : kChroma) {
                const i32 lab[3] = {toQ16(lightness), toQ16(a), toQ16(b)};
                i32 xyz[3];
                i32 back[3];
                oklabToXyzQ16(lab, xyz);
                xyzToOklabQ16(xyz, back);
                for (int i = 0; i < 3; ++i) {
                    FL_CHECK_LT(fl::fabsf(fromQ16(back[i] - lab[i])),
                                32.0f / kQ16);
                }
            }
        }
    }
}

FL_TEST_CASE("OKLab Q16 loses accuracy near black, and this is why") {
    // Recording a real limit rather than choosing a grid that hides it.
    //
    // OKLab's forward transform cube-roots the cone responses, and the cube
    // root's derivative is 1 / (3 * lms^(2/3)) -- which diverges as a cone
    // response approaches zero. At L = 0.05 with any appreciable chroma the
    // smallest response is around 5e-6, where that factor is roughly 1200,
    // so any error at all in the cone response is amplified enormously on
    // the way back out.
    //
    // This is conditioning, not representation. Two candidate fixes were
    // measured and neither works: carrying lms at Q32 instead of Q16 leaves
    // 3655 ULP at L = 0.05 (against 5474) and makes the mapper slightly
    // *worse*, 0.156 against 0.153 dE2000; carrying XYZ itself at Q32 only
    // improves it about fourfold. So the simpler Q16 implementation is kept
    // and the limit is documented here.
    //
    // Restricted to in-gamut colours -- max chroma is about 0.03 at
    // L = 0.05 -- the worst round trip measured is ~300 ULP, about 0.005 in
    // OKLab lightness. It costs the P7 mapper nothing: its score over the
    // corpus is 0.153 dE2000 against a budget of 0.5.
    const i32 dark[3] = {toQ16(0.05f), toQ16(0.02f), toQ16(-0.02f)};
    i32 xyz[3];
    i32 back[3];
    oklabToXyzQ16(dark, xyz);
    xyzToOklabQ16(xyz, back);
    float worst = 0.0f;
    for (int i = 0; i < 3; ++i) {
        const float error = fl::fabsf(fromQ16(back[i] - dark[i]));
        if (error > worst) {
            worst = error;
        }
    }
    // Worse than the bright case by more than an order of magnitude...
    FL_CHECK_GT(worst, 32.0f / kQ16);
    // ...but still bounded, and far from arbitrary.
    FL_CHECK_LT(worst, 0.01f);
}

FL_TEST_CASE("OKLab Q16 chroma scaling preserves hue exactly") {
    // The mapper compresses chroma by scaling (a, b) toward zero rather than
    // converting to polar. That is only hue-preserving if the ratio b/a is
    // untouched, which scaling by a common factor guarantees -- no atan2,
    // hypot, cos or sin anywhere in the per-pixel path.
    const i32 xyz[3] = {toQ16(0.35f), toQ16(0.22f), toQ16(0.9f)};
    i32 lab[3];
    xyzToOklabQ16(xyz, lab);
    FL_REQUIRE_NE(lab[1], 0);
    for (i32 numerator = 1; numerator <= 8; ++numerator) {
        const i64 scale = static_cast<i64>(numerator) * 65536 / 8;
        const i32 scaled_a = static_cast<i32>((scale * lab[1] + 32768) >> 16);
        const i32 scaled_b = static_cast<i32>((scale * lab[2] + 32768) >> 16);
        // b/a is unchanged, to within the rounding of the two products.
        const float want = static_cast<float>(lab[2]) / static_cast<float>(lab[1]);
        const float got = static_cast<float>(scaled_b) / static_cast<float>(scaled_a);
        FL_CHECK_LT(fl::fabsf(got - want), 0.001f);
    }
}

FL_TEST_CASE("OKLab Q16 lightness is monotonic along the neutral axis") {
    i32 previous = -2147483647 - 1;
    for (int step = 1; step <= 12; ++step) {
        const float scale = static_cast<float>(step) / 12.0f;
        const i32 xyz[3] = {toQ16(kD65[0] * scale), toQ16(kD65[1] * scale),
                            toQ16(kD65[2] * scale)};
        i32 out[3];
        xyzToOklabQ16(xyz, out);
        FL_CHECK_GT(out[0], previous);
        previous = out[0];
    }
}

FL_TEST_CASE("OKLab Q16 clamps rather than overflowing on absurd input") {
    // The transform must not be a route from a caller's bad data to signed
    // overflow. INT32_MIN and INT32_MAX are the inputs that would do it.
    const i32 huge[3] = {2147483647, -2147483647 - 1, 2147483647};
    i32 lab[3];
    xyzToOklabQ16(huge, lab);
    for (int i = 0; i < 3; ++i) {
        // Clamped to +/-4.0 in, so every output stays inside the range the
        // coefficients can produce from it: 4.0 * (2.43 * 3) is under 30.
        FL_CHECK_LT(fl::fabsf(fromQ16(lab[i])), 30.0f);
    }
    i32 xyz[3];
    oklabToXyzQ16(huge, xyz);
    for (int i = 0; i < 3; ++i) {
        // The inverse cubes its intermediate, so its bound is 4^3 = 64 per
        // term before the final matrix widens it.
        FL_CHECK_LT(fl::fabsf(fromQ16(xyz[i])), 1000.0f);
    }
}

}  // FL_TEST_FILE

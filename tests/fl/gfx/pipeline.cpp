// Streaming pipeline conformance for color pipeline P6 (#4040).

#include "fl/gfx/pipeline.h"
#include "fl/gfx/colorimetric_response.h"
#include "fl/math/math.h"
#include "fl/stl/int.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

/// The corpus's `rgb` device: sRGB primaries, unit luminance each.
EmitterProfile rgbDevice() {
    EmitterProfile p = {};
    p.xy_r[0] = 0.6400f; p.xy_r[1] = 0.3300f;
    p.xy_g[0] = 0.3000f; p.xy_g[1] = 0.6000f;
    p.xy_b[0] = 0.1500f; p.xy_b[1] = 0.0600f;
    p.lum_r = 1.0f; p.lum_g = 1.0f; p.lum_b = 1.0f;
    p.native_code_depth = 8;
    return p;
}

enum class Src { Srgb, DisplayP3, Bt2020 };

SourceProfile sourceFor(Src which) {
    switch (which) {
    case Src::DisplayP3: return SourceProfile::displayP3();
    case Src::Bt2020:    return SourceProfile::bt2020();
    case Src::Srgb:      break;
    }
    return SourceProfile::srgbBt709();
}

float toFloat(i32 v) { return static_cast<float>(v) / 65536.0f; }

/// One reference vector: the RGB8 the reference was given, and the emitter
/// drives it produced. Taken from ci/golden/color-reference-v1.json, which
/// is the P5 reference this phase is scored against.
struct Vector {
    Src source;
    int code[3];
    i32 drives[3];
};

const Vector kVectors[] = {
    {Src::Bt2020, {  1,  1,  1}, {    12,    41,     4}},
    {Src::Bt2020, {  1,  0,  0}, {    16,     0,     0}},
    {Src::Bt2020, {  0,  1,  0}, {     0,    35,     1}},
    {Src::Bt2020, {  0,  0,  1}, {     0,     3,     1}},
    {Src::Bt2020, {  2,  4,  8}, {     5,   172,    35}},
    {Src::Bt2020, {  8, 32, 96}, {     0,  1628,   450}},
    {Src::Bt2020, {128,128,128}, {  3644, 12255,  1237}},
    {Src::Bt2020, { 16, 16, 16}, {   194,   654,    66}},
    {Src::Bt2020, {240,240,240}, { 12332, 41476,  4187}},
    {Src::Bt2020, {255,  0,  0}, { 18045,     0,   181}},
    {Src::Bt2020, {  0,255,  0}, {     0, 39791,   993}},
    {Src::Bt2020, {  0,  0,255}, {     0,  3942,  1088}},
    {Src::Bt2020, {255,255,  0}, { 14505, 47060,     0}},
    {Src::Bt2020, {  0,255,255}, {     0, 41320,  3819}},
    {Src::Bt2020, {255,  0,255}, { 18116,     0,  4364}},
    {Src::Bt2020, {255, 64,  0}, { 20484,   633,     0}},
    {Src::DisplayP3, {  1,  1,  1}, {     4,    14,     1}},
    {Src::DisplayP3, {  1,  0,  0}, {     5,     0,     0}},
    {Src::DisplayP3, {  0,  1,  0}, {     0,    13,     0}},
    {Src::DisplayP3, {  0,  0,  1}, {     0,     0,     2}},
    {Src::DisplayP3, {  2,  4,  8}, {     7,    58,    12}},
    {Src::DisplayP3, {  8, 32, 96}, {     0,   720,   585}},
    {Src::DisplayP3, {128,128,128}, {  3008, 10117,  1021}},
    {Src::DisplayP3, { 16, 16, 16}, {    72,   243,    25}},
    {Src::DisplayP3, {240,240,240}, { 12143, 40840,  4123}},
    {Src::DisplayP3, {255,  0,  0}, { 15345,     0,     8}},
    {Src::DisplayP3, {  0,255,  0}, {     0, 43563,   366}},
    {Src::DisplayP3, {  0,  0,255}, {     0,     0,  5196}},
    {Src::DisplayP3, {255,255,  0}, { 13657, 46586,     0}},
    {Src::DisplayP3, {  0,255,255}, {     0, 45114,  4425}},
    {Src::DisplayP3, {255,  0,255}, { 15916,     0,  4746}},
    {Src::DisplayP3, {255, 64,  0}, { 15321,  2334,     0}},
    {Src::Srgb, {  1,  1,  1}, {     4,    14,     1}},
    {Src::Srgb, {  1,  0,  0}, {     4,     0,     0}},
    {Src::Srgb, {  0,  1,  0}, {     0,    14,     0}},
    {Src::Srgb, {  0,  0,  1}, {     0,     0,     1}},
    {Src::Srgb, {  2,  4,  8}, {     8,    57,    11}},
    {Src::Srgb, {  8, 32, 96}, {    34,   677,   553}},
    {Src::Srgb, {128,128,128}, {  3008, 10117,  1021}},
    {Src::Srgb, { 16, 16, 16}, {    72,   243,    25}},
    {Src::Srgb, {240,240,240}, { 12143, 40840,  4123}},
    {Src::Srgb, {255,  0,  0}, { 13936,     0,     0}},
    {Src::Srgb, {  0,255,  0}, {     0, 46869,     0}},
    {Src::Srgb, {  0,  0,255}, {     0,     0,  4731}},
    {Src::Srgb, {255,255,  0}, { 13936, 46869,     0}},
    {Src::Srgb, {  0,255,255}, {     0, 46869,  4731}},
    {Src::Srgb, {255,  0,255}, { 13936,     0,  4731}},
    {Src::Srgb, {255, 64,  0}, { 13936,  2403,     0}},};

}  // namespace

FL_TEST_CASE("Streaming pipeline reproduces the P5 reference") {
    // P6's own acceptance criterion: the embedded implementation must stay
    // inside the documented budget against the reference. These 48 vectors
    // are the reference's own, across all three source profiles it covers,
    // and they exercise the whole chain -- decode, primaries to XYZ, the
    // gamut map, the solve -- rather than any stage alone.
    //
    // Worst drive error measured: 0.001312, about 86 raw units of s16.16.
    // A1's luminance budget is 0.5%; this is 0.13%.
    float worst = 0.0f;
    for (const auto& vector : kVectors) {
        StreamingPipelineQ16 pipeline;
        FL_REQUIRE(buildStreamingPipelineQ16(sourceFor(vector.source), rgbDevice(),
                                             GamutPolicy::ChromaCompress, &pipeline));
        i32 drives[3];
        processPixelQ16(pipeline, static_cast<u8>(vector.code[0]),
                        static_cast<u8>(vector.code[1]),
                        static_cast<u8>(vector.code[2]), drives);
        for (int i = 0; i < 3; ++i) {
            const float error = fl::fabsf(toFloat(drives[i] - vector.drives[i]));
            if (error > worst) {
                worst = error;
            }
        }
    }
    // The meaningful threshold: under one code at 8-bit output, so no
    // reference vector can differ by something a device could show.
    FL_CHECK_LT(worst, 1.0f / 255.0f);
    // And the measured one, so a regression that stays under a code still
    // fails rather than sliding by.
    FL_CHECK_LT(worst, 0.002f);
}

FL_TEST_CASE("Brightness scales the drives without moving the colour") {
    // C4: brightness is a linear flux scalar applied at one stage, and
    // scaling every drive together preserves chromaticity by construction.
    // The test is that the ratios between drives survive, since that is what
    // "chromaticity preserved" means at this point in the chain.
    StreamingPipelineQ16 pipeline;
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::srgbBt709(),
                                         rgbDevice(),
                                         GamutPolicy::ChromaCompress, &pipeline));

    i32 full[3];
    processPixelQ16(pipeline, 200, 120, 60, full);
    for (int i = 0; i < 3; ++i) {
        FL_REQUIRE_GT(full[i], 0);
    }

    const u8 kBrightnesses[] = {255, 128, 64, 8};
    for (u8 brightness : kBrightnesses) {
        setPipelineFluxQ16(&pipeline, FluxScalar::fromBrightness(brightness));
        i32 dimmed[3];
        processPixelQ16(pipeline, 200, 120, 60, dimmed);

        const float expected = static_cast<float>(brightness) / 255.0f;
        for (int i = 0; i < 3; ++i) {
            const float got = toFloat(dimmed[i]) / toFloat(full[i]);
            // Two raw units on the smallest drive is the resolution here, so
            // the tolerance loosens as the target dims -- which is also why
            // A1 allows 1.0 dE2000 at 1/32 brightness against 0.5 at unity.
            FL_CHECK_LT(fl::fabsf(got - expected), 0.01f);
        }
    }
}

FL_TEST_CASE("Full brightness is exactly a pass-through") {
    // fromBrightness(255) is unity, so the dimmed path must return bit-identical
    // drives rather than merely close ones. A rounding bug in the scalar
    // would show here and nowhere else.
    StreamingPipelineQ16 pipeline;
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::srgbBt709(),
                                         rgbDevice(),
                                         GamutPolicy::ChromaCompress, &pipeline));
    for (int code = 0; code <= 255; code += 17) {
        i32 unity_drives[3];
        processPixelQ16(pipeline, static_cast<u8>(code),
                        static_cast<u8>(255 - code), 128, unity_drives);
        setPipelineFluxQ16(&pipeline, FluxScalar::fromBrightness(255));
        i32 full_drives[3];
        processPixelQ16(pipeline, static_cast<u8>(code),
                        static_cast<u8>(255 - code), 128, full_drives);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_EQ(unity_drives[i], full_drives[i]);
        }
        setPipelineFluxQ16(&pipeline, FluxScalar::unity());
    }
}

FL_TEST_CASE("Streaming pipeline always returns drives inside [0, 1]") {
    // Every RGB8 code the pipeline can be handed, across all three source
    // profiles -- the widest of which reaches well outside the device's
    // gamut, so the mapper is doing real work on a lot of these.
    const Src kSources[] = {Src::Srgb, Src::DisplayP3, Src::Bt2020};
    for (Src which : kSources) {
        StreamingPipelineQ16 pipeline;
        FL_REQUIRE(buildStreamingPipelineQ16(sourceFor(which), rgbDevice(),
                                             GamutPolicy::ChromaCompress,
                                             &pipeline));
        for (int r = 0; r <= 255; r += 15) {
            for (int g = 0; g <= 255; g += 15) {
                for (int b = 0; b <= 255; b += 15) {
                    i32 drives[3];
                    processPixelQ16(pipeline, static_cast<u8>(r),
                                    static_cast<u8>(g), static_cast<u8>(b),
                                    drives);
                    for (int i = 0; i < 3; ++i) {
                        FL_CHECK_GE(drives[i], 0);
                        FL_CHECK_LE(drives[i], 65536);
                    }
                }
            }
        }
    }
}

FL_TEST_CASE("Black stays black and the profile round-trips its own white") {
    StreamingPipelineQ16 pipeline;
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::srgbBt709(),
                                         rgbDevice(),
                                         GamutPolicy::ChromaCompress, &pipeline));

    i32 black[3];
    processPixelQ16(pipeline, 0, 0, 0, black);
    for (int i = 0; i < 3; ++i) {
        FL_CHECK_EQ(black[i], 0);
    }

    // Full white must land on the device's D65 neutral. Not on full drive
    // everywhere -- this profile's emitters are normalized to unit
    // *luminance* each, so reaching D65 needs roughly 0.21 : 0.72 : 0.07,
    // and expecting three full drives was simply wrong about the profile.
    //
    // Checked as a chromaticity rather than against those three numbers,
    // since the numbers are what the solve produces and comparing them to
    // themselves would prove nothing.
    i32 white[3];
    processPixelQ16(pipeline, 255, 255, 255, white);
    float made[3] = {0.0f, 0.0f, 0.0f};
    const float emitters[3][2] = {
        {0.6400f, 0.3300f}, {0.3000f, 0.6000f}, {0.1500f, 0.0600f}};
    for (int e = 0; e < 3; ++e) {
        float column[3];
        colorimetric_response::xyY_to_XYZ(emitters[e][0], emitters[e][1], 1.0f,
                                          column);
        for (int i = 0; i < 3; ++i) {
            made[i] += toFloat(white[e]) * column[i];
        }
    }
    const float sum = made[0] + made[1] + made[2];
    FL_REQUIRE_GT(sum, 1e-3f);
    FL_CHECK_LT(fl::fabsf(made[0] / sum - 0.3127f), 0.002f);
    FL_CHECK_LT(fl::fabsf(made[1] / sum - 0.3290f), 0.002f);
    // And it must be at full luminance, not merely the right colour.
    FL_CHECK_GT(made[1], 0.99f);
    FL_CHECK_LT(made[1], 1.01f);
}

FL_TEST_CASE("Streaming pipeline rejects what its stages reject") {
    StreamingPipelineQ16 pipeline;
    FL_CHECK_FALSE(buildStreamingPipelineQ16(SourceProfile::srgbBt709(), rgbDevice(),
                                             GamutPolicy::ChromaCompress, nullptr));

    EmitterProfile collinear = rgbDevice();
    collinear.xy_g[0] = 0.6400f;
    collinear.xy_g[1] = 0.3300f;
    FL_CHECK_FALSE(buildStreamingPipelineQ16(SourceProfile::srgbBt709(),
                                             collinear,
                                             GamutPolicy::ChromaCompress, &pipeline));

    // Red and green identical: no source gamut at all.
    const SourceProfile degenerate = SourceProfile::custom(
        RgbPrimaries(Chromaticity(0.64f, 0.33f), Chromaticity(0.64f, 0.33f),
                     Chromaticity(0.15f, 0.06f), Chromaticity(0.3127f, 0.3290f)),
        TransferFunction::Srgb);
    FL_CHECK_FALSE(
        buildStreamingPipelineQ16(degenerate, rgbDevice(),
                                              GamutPolicy::ChromaCompress, &pipeline));

    // Nearly identical, which is the case the threshold is actually for.
    // `invert3x3`'s own guard rejects below 1e-20, and these primaries
    // compute a determinant around 1e-7 -- thirteen orders of magnitude
    // above it -- so without the area check they are accepted and the
    // inverse is 1/det times pure rounding noise. See #4194.
    const SourceProfile nearly = SourceProfile::custom(
        RgbPrimaries(Chromaticity(0.6400f, 0.33000f),
                     Chromaticity(0.6401f, 0.33005f),
                     Chromaticity(0.15f, 0.06f), Chromaticity(0.3127f, 0.3290f)),
        TransferFunction::Srgb);
    FL_CHECK_FALSE(buildStreamingPipelineQ16(nearly, rgbDevice(),
                                              GamutPolicy::ChromaCompress, &pipeline));

    // And a real gamut is nowhere near the threshold, so this cannot be
    // rejecting everything.
    FL_CHECK(buildStreamingPipelineQ16(SourceProfile::bt2020(), rgbDevice(),
                                       GamutPolicy::ChromaCompress, &pipeline));
}

}  // FL_TEST_FILE

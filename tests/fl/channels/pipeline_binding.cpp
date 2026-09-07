// Binding-to-pipeline adapter for color pipeline P6 (#4040).

#include "fl/channels/pipeline_binding.h"
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

const EmitterProfile& staticRgbDevice() {
    static const EmitterProfile kProfile = rgbDevice();
    return kProfile;
}

float toFloat(i32 v) { return static_cast<float>(v) / 65536.0f; }

}  // namespace

FL_TEST_CASE("An unbound binding builds no pipeline") {
    // Not an error: a channel with nothing bound stays on the legacy path,
    // and a false return is what tells the caller that.
    ColorProfileBinding binding;
    StreamingPipelineQ16 pipeline;
    FL_CHECK_FALSE(binding.active());
    FL_CHECK_FALSE(buildPipelineForBinding(binding, &pipeline));
    FL_CHECK_FALSE(buildPipelineForBinding(binding, nullptr));
}

FL_TEST_CASE("A bound binding carries its source, device and policy through") {
    ColorProfileBinding binding;
    binding.mStaticProfile = &staticRgbDevice();
    binding.mSource = SourceProfile::srgbBt709();
    binding.mGamut = GamutPolicy::ChromaCompress;

    StreamingPipelineQ16 pipeline;
    FL_REQUIRE(buildPipelineForBinding(binding, &pipeline));
    FL_CHECK(pipeline.transfer == TransferFunction::Srgb);
    FL_CHECK(pipeline.gamut_policy == GamutPolicy::ChromaCompress);

    // And it produces the same drives as building the pipeline directly, so
    // the adapter is not quietly substituting defaults.
    StreamingPipelineQ16 direct;
    FL_REQUIRE(buildStreamingPipelineQ16(SourceProfile::srgbBt709(), rgbDevice(),
                                         GamutPolicy::ChromaCompress, &direct));
    for (int code = 0; code <= 255; code += 51) {
        i32 through_binding[3];
        i32 through_direct[3];
        processPixelQ16(pipeline, static_cast<u8>(code), 200,
                        static_cast<u8>(255 - code), through_binding);
        processPixelQ16(direct, static_cast<u8>(code), 200,
                        static_cast<u8>(255 - code), through_direct);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_EQ(through_binding[i], through_direct[i]);
        }
    }
}

FL_TEST_CASE("The gamut policy reaches the pixels") {
    // `GamutPolicy` has been stored on the binding and read by nothing. It
    // now decides what happens to a target the device cannot reproduce, so
    // the two settings have to differ where that matters -- and agree where
    // it does not.
    ColorProfileBinding compress;
    compress.mStaticProfile = &staticRgbDevice();
    compress.mSource = SourceProfile::bt2020();
    compress.mGamut = GamutPolicy::ChromaCompress;

    ColorProfileBinding clamp = compress;
    clamp.mGamut = GamutPolicy::Clamp;

    StreamingPipelineQ16 compressing;
    StreamingPipelineQ16 clamping;
    FL_REQUIRE(buildPipelineForBinding(compress, &compressing));
    FL_REQUIRE(buildPipelineForBinding(clamp, &clamping));

    // BT.2020 primaries reach well outside this device's gamut, so saturated
    // codes are exactly where the policies part company.
    int differed = 0;
    const int kSaturated[][3] = {
        {255, 0, 0}, {0, 255, 0}, {0, 0, 255},
        {255, 255, 0}, {0, 255, 255}, {255, 0, 255},
    };
    for (const auto& code : kSaturated) {
        i32 with_compress[3];
        i32 with_clamp[3];
        processPixelQ16(compressing, static_cast<u8>(code[0]),
                        static_cast<u8>(code[1]), static_cast<u8>(code[2]),
                        with_compress);
        processPixelQ16(clamping, static_cast<u8>(code[0]),
                        static_cast<u8>(code[1]), static_cast<u8>(code[2]),
                        with_clamp);
        for (int i = 0; i < 3; ++i) {
            FL_CHECK_GE(with_clamp[i], 0);
            FL_CHECK_LE(with_clamp[i], 65536);
        }
        for (int i = 0; i < 3; ++i) {
            if (with_compress[i] != with_clamp[i]) {
                ++differed;
                break;
            }
        }
    }
    // Guard against the policies never diverging, which would let a
    // regression that ignores the setting pass.
    FL_CHECK_GT(differed, 3);

    // Grey is inside the gamut, so both policies must leave it identical --
    // clamping is only supposed to matter where the mapper would engage.
    i32 grey_compress[3];
    i32 grey_clamp[3];
    processPixelQ16(compressing, 128, 128, 128, grey_compress);
    processPixelQ16(clamping, 128, 128, 128, grey_clamp);
    for (int i = 0; i < 3; ++i) {
        FL_CHECK_EQ(grey_compress[i], grey_clamp[i]);
    }
}

FL_TEST_CASE("Clamping is the worse answer, which is why it is not default") {
    // The P7 study measured clipping at about 20 dE2000 from the reference
    // where chroma compression is at 0.15. That gap should be visible here
    // rather than taken on faith: for a deeply out-of-gamut target the two
    // must land far apart, and the clamped one must sit on the hull's
    // boundary with a channel pinned.
    ColorProfileBinding binding;
    binding.mStaticProfile = &staticRgbDevice();
    binding.mSource = SourceProfile::bt2020();
    binding.mGamut = GamutPolicy::Clamp;

    StreamingPipelineQ16 clamping;
    FL_REQUIRE(buildPipelineForBinding(binding, &clamping));

    i32 drives[3];
    processPixelQ16(clamping, 0, 255, 0, drives);
    // A BT.2020 primary green is outside this device entirely, so clipping
    // has to pin at least one channel at a limit.
    bool pinned = false;
    for (int i = 0; i < 3; ++i) {
        if (drives[i] == 0 || drives[i] == 65536) {
            pinned = true;
        }
    }
    FL_CHECK(pinned);

    binding.mGamut = GamutPolicy::ChromaCompress;
    StreamingPipelineQ16 compressing;
    FL_REQUIRE(buildPipelineForBinding(binding, &compressing));
    i32 mapped[3];
    processPixelQ16(compressing, 0, 255, 0, mapped);

    float distance = 0.0f;
    for (int i = 0; i < 3; ++i) {
        distance += fl::fabsf(toFloat(mapped[i] - drives[i]));
    }
    // Far apart, not a rounding difference.
    FL_CHECK_GT(distance, 0.02f);
}

}  // FL_TEST_FILE

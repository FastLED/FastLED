// ok standalone
// #4515: comparable native hot-path timings for RGB, RGBW and RGBWW profiles.
// This excludes bind-time setup, wire transfer and driver scheduling.

#include "fl/gfx/pipeline.h"
#include "fl/stl/chrono.h"
#include "fl/stl/stdio.h"
#include "profile_result.h"

namespace {

volatile fl::i32 gChecksum = 0;
constexpr int kPixels = 8192;

fl::u32 timePixels(const fl::StreamingPipelineQ16& pipeline,
                   int emitter_count) {
    const fl::u32 start = fl::micros();
    for (int i = 0; i < kPixels; ++i) {
        const fl::u8 r = static_cast<fl::u8>(i * 17);
        const fl::u8 g = static_cast<fl::u8>(i * 37);
        const fl::u8 b = static_cast<fl::u8>(i * 71);
        if (emitter_count == 3) {
            fl::i32 drives[3];
            fl::processPixelQ16(pipeline, r, g, b, drives);
            gChecksum += drives[0];
        } else {
            fl::i32 drives[5];
            fl::processPixelWideQ16(pipeline, r, g, b, drives);
            gChecksum += drives[emitter_count - 1];
        }
    }
    return fl::micros() - start;
}

}  // namespace

int main(int argc, char** argv) {
    const fl::SourceProfile source = fl::SourceProfile::bt2020();
    const fl::Chromaticity red(.6400f, .3300f);
    const fl::Chromaticity green(.3000f, .6000f);
    const fl::Chromaticity blue(.1500f, .0600f);
    const fl::Chromaticity warm(.3457f, .3585f);
    const fl::Chromaticity cool(.3127f, .3290f);
    const fl::colorimetric_response::EmitterProfile devices[] = {
        fl::colorimetric_response::EmitterProfile::rgb(
            "bench/rgb", red, green, blue, 1, 1, 1),
        fl::colorimetric_response::EmitterProfile::rgbw(
            "bench/rgbw", red, green, blue, warm, 1, 1, 1, 1),
        fl::colorimetric_response::EmitterProfile::rgbww(
            "bench/rgbww", red, green, blue, warm, cool,
            .22f, .60f, .08f, .35f, .35f),
    };
    fl::StreamingPipelineQ16 pipelines[3];
    for (int i = 0; i < 3; ++i) {
        if (!fl::buildStreamingPipelineQ16(
                source, devices[i], fl::GamutPolicy::ChromaCompress,
                &pipelines[i])) return 1;
    }
    const char* names[] = {"rgb", "rgbw", "rgbww"};
    for (int i = 0; i < 3; ++i) {
        timePixels(pipelines[i], i + 3);  // warm caches and branch predictor
        const fl::u32 elapsed = timePixels(pipelines[i], i + 3);
        ProfileResultBuilder::print_result(names[i], "color_managed_pixel",
                                           kPixels, elapsed);
    }
    if (argc == 1) {
        fl::printf("pipeline=%u B; optional wide state=%u B; checksum=%d\n",
                   static_cast<unsigned>(sizeof(fl::StreamingPipelineQ16)),
                   static_cast<unsigned>(sizeof(fl::WidePipelineQ16)),
                   static_cast<int>(gChecksum));
    }
    (void)argv;
    return 0;
}

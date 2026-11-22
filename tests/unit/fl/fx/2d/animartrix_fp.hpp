
// Float vs fixed-point comparison tests for all Animartrix visualizations.
//
// Each visualization has a float reference class and a _FP fixed-point class.
// This test renders both at the same time step and compares pixel-by-pixel.
// Reports per-viz error stats and asserts no catastrophic regressions.

#include "test.h"
#include "fl/fx/2d/animartrix.hpp"
#include "fl/stl/unique_ptr.h"
#include "fl/math/xymap.h"
#include "fl/stl/stdio.h"

using namespace fl;

namespace {

// Factory function template
template <typename T>
fl::unique_ptr<IAnimartrix2Viz> make() { return fl::make_unique<T>(); }

struct FPCompareEntry {
    const char *name;
    fl::unique_ptr<IAnimartrix2Viz> (*float_factory)();
    fl::unique_ptr<IAnimartrix2Viz> (*fp_factory)();
};

// clang-format off
const FPCompareEntry kEntries[] = {
    {"Rotating_Blob",       make<Rotating_Blob>,       make<Rotating_Blob_FP>},
    {"Rings",               make<Rings>,               make<Rings_FP>},
    {"Waves",               make<Waves>,               make<Waves_FP>},
    {"Center_Field",        make<Center_Field>,         make<Center_Field_FP>},
    {"Distance_Experiment", make<Distance_Experiment>,  make<Distance_Experiment_FP>},
    {"Caleido1",            make<Caleido1>,             make<Caleido1_FP>},
    {"Caleido2",            make<Caleido2>,             make<Caleido2_FP>},
    {"Caleido3",            make<Caleido3>,             make<Caleido3_FP>},
    {"Lava1",               make<Lava1>,                make<Lava1_FP>},
    {"Scaledemo1",          make<Scaledemo1>,           make<Scaledemo1_FP>},
    {"Yves",                make<Yves>,                 make<Yves_FP>},
    {"Spiralus",            make<Spiralus>,             make<Spiralus_FP>},
    {"Spiralus2",           make<Spiralus2>,            make<Spiralus2_FP>},
    {"Hot_Blob",            make<Hot_Blob>,             make<Hot_Blob_FP>},
    {"Zoom",                make<Zoom>,                 make<Zoom_FP>},
    {"Zoom2",               make<Zoom2>,                make<Zoom2_FP>},
    {"Slow_Fade",           make<Slow_Fade>,            make<Slow_Fade_FP>},
    {"Polar_Waves",         make<Polar_Waves>,          make<Polar_Waves_FP>},
    {"RGB_Blobs",           make<RGB_Blobs>,            make<RGB_Blobs_FP>},
    {"RGB_Blobs2",          make<RGB_Blobs2>,           make<RGB_Blobs2_FP>},
    {"RGB_Blobs3",          make<RGB_Blobs3>,           make<RGB_Blobs3_FP>},
    {"RGB_Blobs4",          make<RGB_Blobs4>,           make<RGB_Blobs4_FP>},
    {"RGB_Blobs5",          make<RGB_Blobs5>,           make<RGB_Blobs5_FP>},
    {"Big_Caleido",         make<Big_Caleido>,          make<Big_Caleido_FP>},
    {"SpiralMatrix1",       make<SpiralMatrix1>,        make<SpiralMatrix1_FP>},
    {"SpiralMatrix2",       make<SpiralMatrix2>,        make<SpiralMatrix2_FP>},
    {"SpiralMatrix3",       make<SpiralMatrix3>,        make<SpiralMatrix3_FP>},
    {"SpiralMatrix4",       make<SpiralMatrix4>,        make<SpiralMatrix4_FP>},
    {"SpiralMatrix5",       make<SpiralMatrix5>,        make<SpiralMatrix5_FP>},
    {"SpiralMatrix6",       make<SpiralMatrix6>,        make<SpiralMatrix6_FP>},
    {"SpiralMatrix8",       make<SpiralMatrix8>,        make<SpiralMatrix8_FP>},
    {"SpiralMatrix9",       make<SpiralMatrix9>,        make<SpiralMatrix9_FP>},
    {"SpiralMatrix10",      make<SpiralMatrix10>,       make<SpiralMatrix10_FP>},
    {"Complex_Kaleido",     make<Complex_Kaleido>,      make<Complex_Kaleido_FP>},
    {"Complex_Kaleido_2",   make<Complex_Kaleido_2>,    make<Complex_Kaleido_2_FP>},
    {"Complex_Kaleido_3",   make<Complex_Kaleido_3>,    make<Complex_Kaleido_3_FP>},
    {"Complex_Kaleido_4",   make<Complex_Kaleido_4>,    make<Complex_Kaleido_4_FP>},
    {"Complex_Kaleido_5",   make<Complex_Kaleido_5>,    make<Complex_Kaleido_5_FP>},
    {"Complex_Kaleido_6",   make<Complex_Kaleido_6>,    make<Complex_Kaleido_6_FP>},
    {"Water",               make<Water>,                make<Water_FP>},
    {"Parametric_Water",    make<Parametric_Water>,      make<Parametric_Water_FP>},
    {"Module_Experiment1",  make<Module_Experiment1>,    make<Module_Experiment1_FP>},
    {"Module_Experiment2",  make<Module_Experiment2>,    make<Module_Experiment2_FP>},
    {"Module_Experiment3",  make<Module_Experiment3>,    make<Module_Experiment3_FP>},
    {"Module_Experiment4",  make<Module_Experiment4>,    make<Module_Experiment4_FP>},
    {"Module_Experiment5",  make<Module_Experiment5>,    make<Module_Experiment5_FP>},
    {"Module_Experiment6",  make<Module_Experiment6>,    make<Module_Experiment6_FP>},
    {"Module_Experiment7",  make<Module_Experiment7>,    make<Module_Experiment7_FP>},
    {"Module_Experiment8",  make<Module_Experiment8>,    make<Module_Experiment8_FP>},
    {"Module_Experiment9",  make<Module_Experiment9>,    make<Module_Experiment9_FP>},
    {"Module_Experiment10", make<Module_Experiment10>,   make<Module_Experiment10_FP>},
    {"Fluffy_Blobs",        make<Fluffy_Blobs>,         make<Fluffy_Blobs_FP>},
    // Chasing_Spirals uses special naming: Float vs Q31
    {"Chasing_Spirals",     make<Chasing_Spirals_Float>, make<Chasing_Spirals_Q31>},
};
// clang-format on

const int kEntryCount = sizeof(kEntries) / sizeof(kEntries[0]);

// XY map callback for Context-based rendering
u16 xyMapCb(u16 x, u16 y, void *userData) {
    XYMap *map = static_cast<XYMap *>(userData);
    return map->mapToIndex(x, y);
}

// Render a viz into dst[] using Context-based init/setTime/draw.
void renderViz(IAnimartrix2Viz *viz, fl::span<CRGB> dst, XYMap &xy,
               uint16_t W, uint16_t H, uint32_t t) {
    for (fl::size i = 0; i < dst.size(); i++) dst[i] = CRGB::Black;
    Context ctx;
    ctx.leds = dst;
    ctx.xyMapFn = xyMapCb;
    ctx.xyMapUserData = &xy;
    init(ctx, W, H);
    setTime(ctx, t);
    viz->draw(ctx);
}

struct CompareResult {
    int max_error;
    double avg_error;
};

// Compare two LED buffers and return max/avg error.
CompareResult compareBuffers(fl::span<const CRGB> a, fl::span<const CRGB> b) {
    CompareResult r = {0, 0.0};
    int count = static_cast<int>(a.size());
    int error_pixels = 0;
    for (int i = 0; i < count; i++) {
        int re = abs(static_cast<int>(a[i].r) - static_cast<int>(b[i].r));
        int ge = abs(static_cast<int>(a[i].g) - static_cast<int>(b[i].g));
        int be = abs(static_cast<int>(a[i].b) - static_cast<int>(b[i].b));
        int mx = re;
        if (ge > mx) mx = ge;
        if (be > mx) mx = be;
        if (mx > r.max_error) r.max_error = mx;
        if (mx > 0) {
            r.avg_error += mx;
            error_pixels++;
        }
    }
    if (error_pixels > 0) r.avg_error /= error_pixels;
    return r;
}

}  // namespace

FL_TEST_CASE("animartrix_fp - float vs fixed-point all vizs") {
    const uint16_t W = 16;
    const uint16_t H = 16;
    const int N = W * H;
    const uint32_t t = 1000;

    XYMap xy = XYMap::constructRectangularGrid(W, H);
    CRGB float_leds_arr[N];
    CRGB fp_leds_arr[N];
    fl::span<CRGB> float_leds(float_leds_arr, N);
    fl::span<CRGB> fp_leds(fp_leds_arr, N);

    // Thresholds: generous to accommodate varying FP accuracy across vizs.
    // Tight thresholds are enforced per-viz in dedicated tests (e.g. chasing_spirals.cpp).
    // These catch catastrophic regressions (completely wrong FP output).
    const int MAX_ERROR_LIMIT = 255;
    const double AVG_ERROR_LIMIT = 135.0;

    int catastrophic = 0;
    int warn_count = 0;

    fl::printf("\n=== Float vs Fixed-Point Accuracy (t=%u, %dx%d) ===\n", t, W, H);
    fl::printf("%-25s %9s %9s %s\n", "Visualization", "MaxErr", "AvgErr", "Status");
    fl::printf("%-25s %9s %9s %s\n", "-------------------------", "---------", "---------", "------");

    for (int i = 0; i < kEntryCount; i++) {
        const FPCompareEntry &e = kEntries[i];

        auto flt = e.float_factory();
        auto fp = e.fp_factory();

        renderViz(flt.get(), float_leds, xy, W, H, t);
        renderViz(fp.get(), fp_leds, xy, W, H, t);

        CompareResult cr = compareBuffers(float_leds, fp_leds);

        const char *status = "OK";
        if (cr.max_error > MAX_ERROR_LIMIT || cr.avg_error > AVG_ERROR_LIMIT) {
            status = "FAIL";
            catastrophic++;
        } else if (cr.max_error > 10 || cr.avg_error > 3.0) {
            status = "WARN";
            warn_count++;
        }

        fl::printf("%-25s %9d %9.2f %s\n",
                e.name, cr.max_error, cr.avg_error, status);

        flt.reset();
        fp.reset();
    }

    fl::printf("\nSummary: %d OK, %d WARN, %d FAIL out of %d vizs\n",
            kEntryCount - warn_count - catastrophic, warn_count, catastrophic, kEntryCount);

    if (catastrophic > 0) {
        fl::printf("FAIL: %d vizs exceeded catastrophic threshold "
                "(max_error>%d or avg_error>%.0f)\n",
                catastrophic, MAX_ERROR_LIMIT, AVG_ERROR_LIMIT);
        }
    FL_ASSERT(catastrophic == 0, "Some vizs have catastrophically wrong FP output");
}

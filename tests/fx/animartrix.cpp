
#include "fl/fx/2d/animartrix.hpp"
#include "crgb.h"
#include "test.h"
#include "fl/xymap.h"

using namespace fl;

FL_TEST_CASE("Animartrix determinism") {
    const uint16_t W = 32, H = 32;
    const uint16_t N = W * H;

    XYMap xy1 = XYMap::constructRectangularGrid(W, H);
    XYMap xy2 = XYMap::constructRectangularGrid(W, H);

    Animartrix fx1(xy1, RGB_BLOBS);
    Animartrix fx2(xy2, RGB_BLOBS);

    CRGB leds1[N] = {};
    CRGB leds2[N] = {};

    Fx::DrawContext ctx1(0, leds1);
    Fx::DrawContext ctx2(0, leds2);

    fx1.draw(ctx1);
    fx2.draw(ctx2);

    // Calculate mismatch percentage and report first 10 mismatches.
    int mismatch_count = 0;
    long total_diff = 0;
    for (uint16_t i = 0; i < N; i++) {
        if (leds1[i] != leds2[i]) {
            if (mismatch_count < 10) {
                FL_MESSAGE("Mismatch at index ", i,
                        ": (", int(leds1[i].r), ",", int(leds1[i].g), ",", int(leds1[i].b), ")"
                        " vs "
                        "(", int(leds2[i].r), ",", int(leds2[i].g), ",", int(leds2[i].b), ")");
            }
            mismatch_count++;
        }
        int dr = leds1[i].r > leds2[i].r ? leds1[i].r - leds2[i].r : leds2[i].r - leds1[i].r;
        int dg = leds1[i].g > leds2[i].g ? leds1[i].g - leds2[i].g : leds2[i].g - leds1[i].g;
        int db = leds1[i].b > leds2[i].b ? leds1[i].b - leds2[i].b : leds2[i].b - leds1[i].b;
        total_diff += dr + dg + db;
    }
    double mismatch_pct = (double)total_diff / (double)(N * 3 * 255) * 100.0;
    FL_MESSAGE("Mismatch percentage: ", mismatch_pct, "%");
    FL_MESSAGE("Mismatched pixels: ", mismatch_count, " / ", N);
    FL_CHECK(mismatch_count == 0);
}

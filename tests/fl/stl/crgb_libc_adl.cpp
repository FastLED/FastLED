// Regression test for FastLED#4827: unqualified libc calls on CRGB* must not be
// ambiguous between ::memcpy and fl::memcpy (found via ADL on fl::CRGB).

#include <stdlib.h>  // ok include
#include <string.h>  // ok include

#include "FastLED.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("unqualified libc memory calls accept CRGB*") {
    CRGB src[4];
    CRGB* dst = static_cast<CRGB*>(malloc(sizeof(CRGB) * 4));
    FL_REQUIRE(dst != nullptr);
    for (int i = 0; i < 4; ++i) {
        src[i] = CRGB(i, i + 1, i + 2);
    }
    memset(dst, 0, sizeof(CRGB) * 4);
    FL_CHECK(dst[3] == CRGB(0, 0, 0));
    memcpy(dst, src, sizeof(CRGB) * 4);
    FL_CHECK(memcmp(dst, src, sizeof(CRGB) * 4) == 0);
    memmove(dst + 1, dst, sizeof(CRGB) * 3);
    FL_CHECK(dst[1] == src[0]);
    dst = static_cast<CRGB*>(realloc(dst, sizeof(CRGB) * 8));
    FL_REQUIRE(dst != nullptr);
    free(dst);
}

}  // FL_TEST_FILE

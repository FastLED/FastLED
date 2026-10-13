// Tests for fl/gfx/pixel_fwd.h: CRGB and CHSV live in fl::pixel so that
// argument-dependent lookup never drags fl's generic templates (fl::swap,
// fl::sort, fl::move, fl::memcpy, ...) into calls on pixel types. Every call
// below was ambiguous when CRGB lived directly in namespace fl (FastLED#4827).
//
// This file deliberately does NOT say `using namespace fl;`.

#include <algorithm>  // ok include
#include <iterator>  // ok include
#include <numeric>  // ok include
#include <random>  // ok include
#include <stdlib.h>  // ok include
#include <string.h>  // ok include
#include <utility>  // ok include

#include "FastLED.h"
#include "fl/gfx/pixel_fwd.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/cstring.h"
#include "fl/stl/malloc.h"
#include "fl/stl/range_access.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("pixel types live in fl::pixel and are re-exported") {
    FL_CHECK((fl::is_same<fl::CRGB, fl::pixel::CRGB>::value));
    FL_CHECK((fl::is_same<CRGB, fl::pixel::CRGB>::value));
    FL_CHECK((fl::is_same<CHSV, fl::pixel::hsv8>::value));
    CRGB a(1, 2, 3);
    CRGB b(1, 2, 3);
    FL_CHECK(a == b);          // operators found via ADL in fl::pixel
    FL_CHECK((a + b) == CRGB(2, 4, 6));
}

// FastLED#4827: unqualified libc calls on CRGB* must not be ambiguous
// between ::memcpy and fl::memcpy (found via ADL on fl::CRGB).
FL_TEST_CASE("unqualified libc memory calls accept CRGB*") {
    CRGB src[4];
    CRGB* dst = static_cast<CRGB*>(malloc(sizeof(CRGB) * 4));  // ok bare allocation
    FL_REQUIRE(dst != nullptr);
    for (int i = 0; i < 4; ++i) {
        src[i] = CRGB(i, i + 1, i + 2);
    }
    memset(dst, 0, sizeof(CRGB) * 4);  // ok ctype
    FL_CHECK(dst[3] == CRGB(0, 0, 0));
    memcpy(dst, src, sizeof(CRGB) * 4);  // ok ctype
    FL_CHECK(memcmp(dst, src, sizeof(CRGB) * 4) == 0);  // ok ctype
    memmove(dst + 1, dst, sizeof(CRGB) * 3);  // ok ctype
    FL_CHECK(dst[1] == src[0]);
    (void)memchr(dst, 0, sizeof(CRGB) * 4);  // ok ctype
    qsort(dst, 4, sizeof(CRGB), [](const void*, const void*) { return 0; });
    dst = static_cast<CRGB*>(realloc(dst, sizeof(CRGB) * 8));  // ok bare allocation
    FL_REQUIRE(dst != nullptr);
    free(dst);  // ok bare allocation
}

namespace std_qualified {
static bool lessR(const CRGB& a, const CRGB& b) { return a.r < b.r; }
// FastLED#4827: libstdc++ calls swap() unqualified, so ADL on fl::CRGB used to
// make std::reverse/std::sort/std::swap ambiguous with the generic fl::swap.
FL_TEST_CASE("std:: algorithms compile and run on CRGB") {  // okay std namespace
    CRGB a[4] = {CRGB(3,0,0), CRGB(1,0,0), CRGB(4,0,0), CRGB(2,0,0)};
    CRGB b[4];
    std::reverse(a, a + 4);  // okay std namespace
    std::sort(a, a + 4, lessR);  // okay std namespace
    std::stable_sort(a, a + 4, lessR);  // okay std namespace
    std::rotate(a, a + 1, a + 4);  // okay std namespace
    std::swap(a[0], a[1]);  // okay std namespace
    std::iter_swap(a, a + 1);  // okay std namespace
    std::fill(b, b + 4, CRGB(0,0,0));  // okay std namespace
    std::copy(a, a + 4, b);  // okay std namespace
    (void)std::find(a, a + 4, CRGB(1,0,0));  // okay std namespace
    (void)std::min_element(a, a + 4, lessR);  // okay std namespace
    (void)std::max_element(a, a + 4, lessR);  // okay std namespace
    (void)std::equal(a, a + 4, b);  // okay std namespace
    (void)std::lower_bound(a, a + 4, CRGB(2,0,0), lessR);  // okay std namespace
    std::replace(a, a + 4, CRGB(9,0,0), CRGB(8,0,0));  // okay std namespace
    (void)std::remove(a, a + 4, CRGB(9,0,0));  // okay std namespace
    (void)std::begin(a); (void)std::end(a);  // okay std namespace
    std::swap(a, b);  // okay std namespace
    CRGB c[3] = {CRGB(1,0,0), CRGB(2,0,0), CRGB(3,0,0)};
    std::reverse(c, c + 3);  // okay std namespace
    FL_CHECK(c[0] == CRGB(3,0,0));
    FL_CHECK(c[2] == CRGB(1,0,0));
}
}
namespace using_std {
static bool lessR(const CRGB& a, const CRGB& b) { return a.r < b.r; }
// FastLED#4827: with `using namespace std`, unqualified algorithm calls on CRGB*
// must resolve to std:: rather than tie with the fl:: algorithm via ADL.
FL_TEST_CASE("unqualified algorithms on CRGB with using namespace std") {
    using namespace std;
    CRGB a[4] = {CRGB(3,0,0), CRGB(1,0,0), CRGB(4,0,0), CRGB(2,0,0)};
    CRGB b[4];
    reverse(a, a + 4);
    sort(a, a + 4, lessR);
    stable_sort(a, a + 4, lessR);
    swap(a[0], a[1]);
    fill(b, b + 4, CRGB(0,0,0));
    (void)find(a, a + 4, CRGB(1,0,0));
    (void)min_element(a, a + 4, lessR);
    (void)max_element(a, a + 4, lessR);
    (void)equal(a, a + 4, b);
    (void)lower_bound(a, a + 4, CRGB(2,0,0), lessR);
    replace(a, a + 4, CRGB(9,0,0), CRGB(8,0,0));
    (void)remove(a, a + 4, CRGB(9,0,0));
    (void)begin(a); (void)end(a);
    swap(a, b);
#if defined(__clang__)
    FL_DISABLE_WARNING_PUSH
    FL_DISABLE_WARNING(unknown-warning-option)
    FL_DISABLE_WARNING(unqualified-std-cast-call)
#endif
    CRGB m = move(a[0]); (void)m;
    CRGB& fr = forward<CRGB&>(a[0]); (void)fr;
#if defined(__clang__)
    FL_DISABLE_WARNING_POP
#endif
    copy(a, a + 4, b);
    copy_n(a, 4, b);
    (void)count(a, a + 4, CRGB(1,0,0));
    (void)distance(a, a + 4);
    (void)next(a); (void)prev(a + 1);
    CRGB* p = a; advance(p, 1);
    (void)all_of(a, a + 4, [](const CRGB&) { return true; });
    (void)any_of(a, a + 4, [](const CRGB&) { return true; });
    (void)none_of(a, a + 4, [](const CRGB&) { return true; });
    (void)search(a, a + 4, b, b + 1);
    (void)unique(a, a + 4);
    (void)is_sorted(a, a + 4, lessR);
    (void)upper_bound(a, a + 4, CRGB(2,0,0), lessR);
    (void)binary_search(a, a + 4, CRGB(2,0,0), lessR);
    (void)min(a[0], a[1], lessR); (void)max(a[0], a[1], lessR);
    (void)transform(a, a + 4, b, [](const CRGB& c) { return c; });
    for_each(a, a + 4, [](CRGB&) {});
    iota(b, b + 0, CRGB(0,0,0));
    (void)accumulate(a, a + 0, 0, [](int x, const CRGB&) { return x; });
    rotate(a, a + 1, a + 4);
    iter_swap(a, a + 1);
    mt19937 g(1);
    shuffle(a, a + 4, g);
    CHSV h[2];
    swap(h[0], h[1]);
    sort(h, h + 2, [](const CHSV& x, const CHSV& y) { return x.h < y.h; });
    CRGB c[3] = {CRGB(3,0,0), CRGB(1,0,0), CRGB(2,0,0)};
    sort(c, c + 3, lessR);
    FL_CHECK(c[0] == CRGB(1,0,0));
    FL_CHECK(c[2] == CRGB(3,0,0));
}
}

} // FL_TEST_FILE

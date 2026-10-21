// Tests for fl/stl/adl_barrier.h: fl's generic templates and libc wrappers
// (fl::swap, fl::sort, fl::move, fl::memcpy, ...) live behind a using-directive
// so argument-dependent lookup on fl types (CRGB, CHSV, palettes, containers)
// never drags them into user calls. Every unqualified call below used to be
// ambiguous between the libc/std function and its fl:: twin (FastLED#4827).
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
#include "fl/stl/adl_barrier.h"
#include "fl/stl/vector.h"
#include "fl/stl/list.h"
#include "fl/stl/string.h"
#include "fl/stl/functional.h"
#include "fl/stl/shared_ptr.h"
#include "fl/stl/unique_ptr.h"
#include "fl/stl/tuple.h"
#include "fl/stl/pair.h"
#include "fl/gfx/downscale.h"
#include "fl/stl/algorithm.h"
#include "fl/stl/cstring.h"
#include "fl/stl/malloc.h"
#include "fl/stl/cstdlib.h"
#include "fl/system/exit.h"
#include "fl/stl/range_access.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("fl API still reachable: qualified, unqualified-in-fl, and ADL") {
    // Pixel types stay in fl, so fl functions on them are still found by ADL.
    FL_CHECK((fl::is_same<CRGB, fl::CRGB>::value));
    CRGB src[4] = {CRGB(4, 4, 4), CRGB(4, 4, 4), CRGB(4, 4, 4), CRGB(4, 4, 4)};
    CRGB dst[1];
    downscaleHalf(src, 2, 2, dst);  // ADL into fl, no fl:: prefix
    FL_CHECK(dst[0] == CRGB(4, 4, 4));
    // Qualified calls into the barrier keep working.
    int x = 1, y = 2;
    fl::swap(x, y);
    FL_CHECK(x == 2);
    char buf[4] = {};
    fl::memset(buf, 'a', 3);
    FL_CHECK(fl::memcmp(buf, "aaa", 3) == 0);
    int v[3] = {3, 1, 2};
    fl::sort(v, v + 3);
    FL_CHECK(v[0] == 1);
    FL_CHECK(fl::clamp(5, 0, 3) == 3);
    int m = fl::move(x);
    FL_CHECK(m == 2);
    // swap stays in fl, so unqualified swap on fl containers still works.
    fl::vector<int> a1, a2;
    a1.push_back(1);
    swap(a1, a2);
    FL_CHECK(a2.size() == 1u);
    fl::list<int> l1, l2;
    l1.push_back(7);
    swap(l1, l2);
    FL_CHECK(l2.size() == 1u);
}

FL_TEST_CASE("libc and std on other fl types: palettes, containers") {
    // Legacy sketches memcpy palettes; compilers warn because the type is not
    // trivially copyable, which is unrelated to the lookup under test.
    FL_DISABLE_WARNING_PUSH
#if defined(__clang__)
    FL_DISABLE_WARNING(unknown-warning-option)
    FL_DISABLE_WARNING(nontrivial-memcall)
#elif defined(__GNUC__)
    FL_DISABLE_WARNING(class-memaccess)
#endif
    CRGBPalette16 p1(CRGB::Red);
    CRGBPalette16 p2(CRGB::Blue);
    memcpy(&p1, &p2, sizeof(p1));  // ok ctype
    FL_CHECK(p1[0] == p2[0]);
    memset(&p1, 0, sizeof(p1));  // ok ctype
    FL_DISABLE_WARNING_POP
    fl::vector<fl::string> names;
    names.push_back("b");
    names.push_back("a");
    std::sort(names.begin(), names.end());  // okay std namespace
    FL_CHECK(names[0] == "a");
    fl::string s1("x"), s2("y");
    std::swap(s1, s2);  // okay std namespace
    FL_CHECK(s1 == "y");
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

// 3.10.3 had no fl::memset etc., so `using namespace fl;` sketches calling
// libc unqualified compiled. fl's libc mirrors are templates that lose ties
// to the C library, so this must keep compiling.
FL_TEST_CASE("using namespace fl keeps libc calls unambiguous") {
    using namespace fl;
    char buf[8];
    memset(buf, 0, sizeof(buf));  // ok ctype
    memcpy(buf, "abc", 4);  // ok ctype
    FL_CHECK(strlen(buf) == 3u);  // ok ctype
    FL_CHECK(strcmp(buf, "abc") == 0);  // ok ctype
    void* p = malloc(4);  // ok bare allocation
    p = realloc(p, 8);  // ok bare allocation
    free(p);  // ok bare allocation
    CRGB leds[2];
    memset(leds, 0, sizeof(leds));  // ok ctype
    FL_CHECK(atoi("42") == 42);
}

// FastLED#4834: compile-time back-stop for the libc_mirror lint rule. Every
// wrapper in fl/stl/{cstring,malloc,cstdlib}.h and fl/system/exit.h is called
// unqualified in an unevaluated context, both with fl-typed (CRGB*) arguments
// and no using-directive (the ADL path) and under `using namespace fl`. A
// mirror that ties with libc makes one of these lines ambiguous.
// The LibcBackstopChecker lint pass (FastLED#4838) fails if a wrapper under src/
// is missing from `libc_every_using_fl`, if a wrapper with a void* parameter is
// missing from `libc_every_adl`, or if either block calls a removed wrapper.
namespace libc_every_adl {
FL_TEST_CASE("every pointer-taking libc wrapper is unambiguous on CRGB*") {
    CRGB* p = nullptr;
    const CRGB* cp = nullptr;
    FL_STATIC_ASSERT(sizeof(decltype(memcpy(p, cp, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(memmove(p, cp, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(memset(p, 0, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(memcmp(cp, cp, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(memchr(p, 0, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(memchr(cp, 0, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(free(p))*) > 0, "");  // ok bare allocation
    FL_STATIC_ASSERT(sizeof(decltype(realloc(p, 1))*) > 0, "");  // ok bare allocation
    FL_STATIC_ASSERT(sizeof(decltype(qsort(p, 1, 1, nullptr))*) > 0, "");
    FL_CHECK(true);
}
}  // namespace libc_every_adl

namespace libc_every_using_fl {
FL_TEST_CASE("every libc wrapper is unambiguous under using namespace fl") {
    using namespace fl;
    char* s = nullptr;
    const char* cs = nullptr;
    char** end = nullptr;
    void* v = nullptr;
    FL_STATIC_ASSERT(sizeof(decltype(strlen(cs))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strcmp(cs, cs))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strncmp(cs, cs, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strcpy(s, cs))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strncpy(s, cs, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strcat(s, cs))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strncat(s, cs, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strstr(cs, cs))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strchr(s, 'a'))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strchr(cs, 'a'))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strrchr(s, 'a'))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strrchr(cs, 'a'))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strspn(cs, cs))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strcspn(cs, cs))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strpbrk(s, cs))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strpbrk(cs, cs))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strtok(s, cs))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(strerror(0))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(memcpy(v, v, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(memmove(v, v, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(memset(v, 0, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(memcmp(v, v, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(memchr(v, 0, 1))*) > 0, "");  // ok ctype
    FL_STATIC_ASSERT(sizeof(decltype(malloc(1))*) > 0, "");  // ok bare allocation
    FL_STATIC_ASSERT(sizeof(decltype(calloc(1, 1))*) > 0, "");  // ok bare allocation
    FL_STATIC_ASSERT(sizeof(decltype(realloc(v, 1))*) > 0, "");  // ok bare allocation
    FL_STATIC_ASSERT(sizeof(decltype(free(v))*) > 0, "");  // ok bare allocation
    FL_STATIC_ASSERT(sizeof(decltype(aligned_alloc(8, 8))*) > 0, "");  // ok bare allocation
    FL_STATIC_ASSERT(sizeof(decltype(strtol(cs, end, 10))*) > 0, "");
    FL_STATIC_ASSERT(sizeof(decltype(strtoul(cs, end, 10))*) > 0, "");
    FL_STATIC_ASSERT(sizeof(decltype(atoi(cs))*) > 0, "");
    FL_STATIC_ASSERT(sizeof(decltype(atol(cs))*) > 0, "");
    FL_STATIC_ASSERT(sizeof(decltype(strtod(cs, end))*) > 0, "");
    FL_STATIC_ASSERT(sizeof(decltype(qsort(v, 1, 1, nullptr))*) > 0, "");
    FL_STATIC_ASSERT(sizeof(decltype(rand())*) > 0, "");
    FL_STATIC_ASSERT(sizeof(decltype(getenv(cs))*) > 0, "");
    FL_STATIC_ASSERT(sizeof(decltype(exit(0))*) > 0, "");
    FL_CHECK(true);
}
}  // namespace libc_every_using_fl

// Qualified fl::name must still reach every function behind the barrier. If a
// future header declares one of these names directly in fl, qualified lookup
// stops there and this stops compiling (see fl/stl/adl_barrier.h).
// The AdlBarrierChecker lint pass (FastLED#4851) fails if a function declared in
// fl::adl_barrier is not called here as fl::name, or if any overload of a
// barrier name is declared directly in fl.
FL_TEST_CASE("every adl_barrier name is reachable as fl::name") {
    int v[4] = {4, 3, 2, 1};
    int w[4] = {1, 2, 3, 4};
    fl::reverse(v, v + 4);
    FL_CHECK(fl::equal(v, v + 4, w));
    FL_CHECK(fl::equal(v, v + 4, w, w + 4));
    fl::vector<int> c1;
    c1.push_back(1);
    fl::vector<int> c2 = c1;
    FL_CHECK(fl::equal_container(c1, c2));
    FL_CHECK(fl::equal_container(c1, c2, [](int a, int b) { return a == b; }));
    FL_CHECK(!fl::lexicographical_compare(v, v + 4, w, w + 4));
    FL_CHECK(*fl::max_element(v, v + 4) == 4);
    FL_CHECK(*fl::min_element(v, v + 4) == 1);
    FL_CHECK(fl::find(v, v + 4, 3) == v + 2);
    FL_CHECK(fl::find_if(v, v + 4, [](int x) { return x > 2; }) == v + 2);
    FL_CHECK(fl::find_if_not(v, v + 4, [](int x) { return x < 2; }) == v + 1);
    FL_CHECK(fl::lower_bound(v, v + 4, 3) == v + 2);
    fl::replace(v, v + 4, 4, 5);
    fl::replace_if(v, v + 4, [](int x) { return x == 5; }, 4);
    FL_CHECK(fl::remove(v, v + 4, 9) == v + 4);
    FL_CHECK(fl::remove_if(v, v + 4, [](int) { return false; }) == v + 4);
    fl::sort(v, v + 4, [](int a, int b) { return a > b; });
    fl::stable_sort(v, v + 4);
    fl::sort_small(v, v + 4);
    fl::sort_small(v, v + 4, [](int a, int b) { return a > b; });
    fl::shuffle(v, v + 4);
    fl::fill(v, v + 4, 7);
    FL_CHECK(fl::begin(v) == v);
    FL_CHECK(fl::end(v) == v + 4);
    int m = fl::move(v[0]);
    FL_CHECK(fl::forward<int&>(m) == 7);
    FL_CHECK(fl::make_pair(1, 2).second == 2);
    FL_CHECK(fl::get<1>(fl::make_tuple(1, 2)) == 2);
    FL_CHECK(fl::invoke([](int x) { return x + 1; }, 1) == 2);
    FL_CHECK(*fl::make_shared<int>(3) == 3);
    FL_CHECK(*fl::allocate_shared<int>(fl::allocator<int>(), 4) == 4);
    FL_CHECK(*fl::make_unique<int>(5) == 5);
    FL_CHECK(fl::clamp(5, 0, 3) == 3);
}

} // FL_TEST_FILE

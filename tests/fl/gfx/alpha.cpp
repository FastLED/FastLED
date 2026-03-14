#include "test.h"

#include "fl/gfx/alpha.h"

using fl::alpha8;
using fl::alpha16;

// ---------------------------------------------------------------------------
// Template test helper — validates operations for any alpha type.
// ---------------------------------------------------------------------------
template <typename Alpha, typename Raw, int MaxVal>
static void test_alpha_basics() {
    Alpha a;
    FL_CHECK_EQ((int)a.raw(), 0);

    Alpha b(static_cast<Raw>(MaxVal));
    FL_CHECK_EQ((int)b.raw(), MaxVal);

    Raw r = b;
    FL_CHECK_EQ((int)r, MaxVal);

    Alpha c = b;
    FL_CHECK_EQ((int)c.raw(), MaxVal);
}

template <typename Alpha, typename Raw, int MaxVal>
static void test_alpha_float_clamp() {
    Alpha zero(0.0f);
    FL_CHECK_EQ((int)zero.raw(), 0);

    Alpha one(1.0f);
    FL_CHECK_EQ((int)one.raw(), MaxVal);

    Alpha above(2.0f);
    FL_CHECK_EQ((int)above.raw(), MaxVal);

    Alpha large(100.0f);
    FL_CHECK_EQ((int)large.raw(), MaxVal);

    Alpha below(-1.0f);
    FL_CHECK_EQ((int)below.raw(), 0);

    Alpha neg(-100.0f);
    FL_CHECK_EQ((int)neg.raw(), 0);

    Alpha d_above(2.0);
    FL_CHECK_EQ((int)d_above.raw(), MaxVal);

    Alpha d_below(-1.0);
    FL_CHECK_EQ((int)d_below.raw(), 0);

    Alpha half(0.5f);
    FL_CHECK((int)half.raw() > 0);
    FL_CHECK((int)half.raw() < MaxVal);
}

template <typename Alpha, typename Raw, int MaxVal>
static void test_alpha_from_float() {
    Alpha a = Alpha::from_float(0.0f);
    FL_CHECK_EQ((int)a.raw(), 0);

    Alpha b = Alpha::from_float(1.0f);
    FL_CHECK_EQ((int)b.raw(), MaxVal);

    Alpha c = Alpha::from_float(2.0f);
    FL_CHECK_EQ((int)c.raw(), MaxVal);

    Alpha d = Alpha::from_float(-1.0f);
    FL_CHECK_EQ((int)d.raw(), 0);
}

template <typename Alpha, typename Raw, int MaxVal>
static void test_alpha_to_float() {
    Alpha zero(static_cast<Raw>(0));
    FL_CHECK_CLOSE(zero.to_float(), 0.0f, 0.0001f);

    Alpha full(static_cast<Raw>(MaxVal));
    FL_CHECK_CLOSE(full.to_float(), 1.0f, 0.0001f);
}

template <typename Alpha, typename Raw, int MaxVal>
static void test_alpha_compound_assign() {
    Alpha a(static_cast<Raw>(10));
    a += 5;
    FL_CHECK_EQ((int)a.raw(), 15);

    a -= 3;
    FL_CHECK_EQ((int)a.raw(), 12);

    Alpha b(static_cast<Raw>(4));
    b *= 3;
    FL_CHECK_EQ((int)b.raw(), 12);

    b /= 2;
    FL_CHECK_EQ((int)b.raw(), 6);
}

template <typename Alpha, typename Raw, int MaxVal>
static void test_alpha_shift() {
    Alpha a(static_cast<Raw>(64));
    a >>= 2;
    FL_CHECK_EQ((int)a.raw(), 16);

    a <<= 3;
    FL_CHECK_EQ((int)a.raw(), 128);
}

template <typename Alpha, typename Raw, int MaxVal>
static void test_alpha_increment_decrement() {
    Alpha a(static_cast<Raw>(10));

    Alpha& ref = ++a;
    FL_CHECK_EQ((int)a.raw(), 11);
    FL_CHECK_EQ((int)ref.raw(), 11);

    Alpha old = a++;
    FL_CHECK_EQ((int)old.raw(), 11);
    FL_CHECK_EQ((int)a.raw(), 12);

    --a;
    FL_CHECK_EQ((int)a.raw(), 11);

    old = a--;
    FL_CHECK_EQ((int)old.raw(), 11);
    FL_CHECK_EQ((int)a.raw(), 10);
}

template <typename Alpha, typename Raw, int MaxVal>
static void test_alpha_cross_instance_ops() {
    Alpha a(static_cast<Raw>(100));
    Alpha b(static_cast<Raw>(50));

    a += Raw(b);
    FL_CHECK_EQ((int)a.raw(), 150);

    a -= Raw(b);
    FL_CHECK_EQ((int)a.raw(), 100);

    Alpha c(static_cast<Raw>(100));
    FL_CHECK(Raw(a) == Raw(c));
    FL_CHECK(Raw(b) < Raw(a));
    FL_CHECK(Raw(a) > Raw(b));
}

// ---------------------------------------------------------------------------
// alpha8 tests
// ---------------------------------------------------------------------------
FL_TEST_CASE("alpha8 basics") {
    test_alpha_basics<alpha8, unsigned char, 255>();
}

FL_TEST_CASE("alpha8 float clamp") {
    test_alpha_float_clamp<alpha8, unsigned char, 255>();
}

FL_TEST_CASE("alpha8 from_float") {
    test_alpha_from_float<alpha8, unsigned char, 255>();
}

FL_TEST_CASE("alpha8 to_float") {
    test_alpha_to_float<alpha8, unsigned char, 255>();
}

FL_TEST_CASE("alpha8 compound assign") {
    test_alpha_compound_assign<alpha8, unsigned char, 255>();
}

FL_TEST_CASE("alpha8 shift") {
    test_alpha_shift<alpha8, unsigned char, 255>();
}

FL_TEST_CASE("alpha8 increment decrement") {
    test_alpha_increment_decrement<alpha8, unsigned char, 255>();
}

FL_TEST_CASE("alpha8 cross-instance ops") {
    test_alpha_cross_instance_ops<alpha8, unsigned char, 255>();
}

// ---------------------------------------------------------------------------
// alpha16 tests
// ---------------------------------------------------------------------------
FL_TEST_CASE("alpha16 basics") {
    test_alpha_basics<alpha16, unsigned short, 65535>();
}

FL_TEST_CASE("alpha16 float clamp") {
    test_alpha_float_clamp<alpha16, unsigned short, 65535>();
}

FL_TEST_CASE("alpha16 from_float") {
    test_alpha_from_float<alpha16, unsigned short, 65535>();
}

FL_TEST_CASE("alpha16 to_float") {
    test_alpha_to_float<alpha16, unsigned short, 65535>();
}

FL_TEST_CASE("alpha16 compound assign") {
    test_alpha_compound_assign<alpha16, unsigned short, 65535>();
}

FL_TEST_CASE("alpha16 shift") {
    test_alpha_shift<alpha16, unsigned short, 65535>();
}

FL_TEST_CASE("alpha16 increment decrement") {
    test_alpha_increment_decrement<alpha16, unsigned short, 65535>();
}

FL_TEST_CASE("alpha16 cross-instance ops") {
    test_alpha_cross_instance_ops<alpha16, unsigned short, 65535>();
}

// ---------------------------------------------------------------------------
// Integer constructor disambiguation
// ---------------------------------------------------------------------------
FL_TEST_CASE("alpha8 integer constructor disambiguation") {
    alpha8 from_uchar((unsigned char)128);
    alpha8 from_int(128);
    alpha8 from_uint(128u);
    alpha8 from_long(128L);
    alpha8 from_ulong(128UL);
    alpha8 from_ll(128LL);
    alpha8 from_ull(128ULL);
    FL_CHECK_EQ((int)from_uchar.raw(), 128);
    FL_CHECK_EQ((int)from_int.raw(), 128);
    FL_CHECK_EQ((int)from_uint.raw(), 128);
    FL_CHECK_EQ((int)from_long.raw(), 128);
    FL_CHECK_EQ((int)from_ulong.raw(), 128);
    FL_CHECK_EQ((int)from_ll.raw(), 128);
    FL_CHECK_EQ((int)from_ull.raw(), 128);
}

FL_TEST_CASE("alpha16 integer constructor disambiguation") {
    alpha16 from_ushort((unsigned short)1000);
    alpha16 from_int(1000);
    alpha16 from_uint(1000u);
    alpha16 from_long(1000L);
    alpha16 from_ulong(1000UL);
    FL_CHECK_EQ((int)from_ushort.raw(), 1000);
    FL_CHECK_EQ((int)from_int.raw(), 1000);
    FL_CHECK_EQ((int)from_uint.raw(), 1000);
    FL_CHECK_EQ((int)from_long.raw(), 1000);
    FL_CHECK_EQ((int)from_ulong.raw(), 1000);
}

// ---------------------------------------------------------------------------
// Constexpr verification — static_assert proves compile-time evaluation.
// If any of these operations are not truly constexpr, this file won't compile.
// ---------------------------------------------------------------------------
template <typename Alpha, typename Raw, int MaxVal>
static void test_alpha_constexpr() {
    // Default construction
    constexpr Alpha def;
    static_assert(def.raw() == 0, "default ctor");

    // Integer construction — every width and signedness
    constexpr Alpha from_raw_type(static_cast<Raw>(MaxVal));
    static_assert(from_raw_type.raw() == MaxVal, "raw type ctor");

    constexpr Alpha from_char(static_cast<signed char>(42));
    static_assert(from_char.raw() == 42, "signed char ctor");

    constexpr Alpha from_uchar(static_cast<unsigned char>(42));
    static_assert(from_uchar.raw() == 42, "unsigned char ctor");

    constexpr Alpha from_short(static_cast<short>(42));
    static_assert(from_short.raw() == 42, "short ctor");

    constexpr Alpha from_ushort(static_cast<unsigned short>(42));
    static_assert(from_ushort.raw() == 42, "unsigned short ctor");

    constexpr Alpha from_int(42);
    static_assert(from_int.raw() == 42, "int ctor");

    constexpr Alpha from_uint(42u);
    static_assert(from_uint.raw() == 42, "unsigned int ctor");

    constexpr Alpha from_long(42L);
    static_assert(from_long.raw() == 42, "long ctor");

    constexpr Alpha from_ulong(42UL);
    static_assert(from_ulong.raw() == 42, "unsigned long ctor");

    constexpr Alpha from_ll(42LL);
    static_assert(from_ll.raw() == 42, "long long ctor");

    constexpr Alpha from_ull(42ULL);
    static_assert(from_ull.raw() == 42, "unsigned long long ctor");

    // Float / double construction (explicit)
    constexpr Alpha from_f0(0.0f);
    static_assert(from_f0.raw() == 0, "float ctor 0.0");

    constexpr Alpha from_f1(1.0f);
    static_assert(from_f1.raw() == MaxVal, "float ctor 1.0");

    constexpr Alpha from_d0(0.0);
    static_assert(from_d0.raw() == 0, "double ctor 0.0");

    constexpr Alpha from_d1(1.0);
    static_assert(from_d1.raw() == MaxVal, "double ctor 1.0");

    // Float clamp boundaries
    constexpr Alpha clamp_above(2.0f);
    static_assert(clamp_above.raw() == MaxVal, "float clamp above");

    constexpr Alpha clamp_below(-1.0f);
    static_assert(clamp_below.raw() == 0, "float clamp below");

    // Implicit conversion to Raw
    constexpr Raw r = from_raw_type;
    static_assert(r == static_cast<Raw>(MaxVal), "implicit conversion");

    // raw() accessor
    static_assert(from_int.raw() == 42, "raw()");

    // to_float() — verify constexpr (boundaries are exact in IEEE 754)
    constexpr float f_zero = Alpha(static_cast<Raw>(0)).to_float();
    static_assert(f_zero == 0.0f, "to_float 0");

    constexpr float f_one = Alpha(static_cast<Raw>(MaxVal)).to_float();
    static_assert(f_one == 1.0f, "to_float max");

    // from_float() static method
    constexpr Alpha ff0 = Alpha::from_float(0.0f);
    static_assert(ff0.raw() == 0, "from_float 0");

    constexpr Alpha ff1 = Alpha::from_float(1.0f);
    static_assert(ff1.raw() == MaxVal, "from_float 1");

    constexpr Alpha ff_clamp = Alpha::from_float(5.0f);
    static_assert(ff_clamp.raw() == MaxVal, "from_float clamp");

    // Copy construction
    constexpr Alpha copy = from_raw_type;
    static_assert(copy.raw() == MaxVal, "copy ctor");

    // All checks passed at compile time; runtime just confirms we got here.
    FL_CHECK(true);
}

FL_TEST_CASE("alpha8 constexpr") {
    test_alpha_constexpr<alpha8, unsigned char, 255>();
}

FL_TEST_CASE("alpha16 constexpr") {
    test_alpha_constexpr<alpha16, unsigned short, 65535>();
}

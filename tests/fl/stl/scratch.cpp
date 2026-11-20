// ok cpp include
/// @file scratch.cpp
/// @brief Tests for fl::scratch<T> and its fixed-capacity side-stack backend
/// (fl::detail::side_stack). Host builds default FL_SCRATCH_STACK_BYTES to 0,
/// so the side-stack logic is exercised directly on small test arenas.

// This TU runs fl::scratch on its production FL_SCRATCH_STACK_BYTES > 0
// backend (global arena) with a 256 B capacity. The host library is built
// with the default 0 and so defines no arena; this TU links the library's
// own arena definitions from fl/stl/detail/scratch_arena.hpp. (Real builds must set the capacity build-wide
// with -D; a test binary is the one place a per-TU value is safe.)
#define FL_SCRATCH_STACK_BYTES 256
#define FL_SCRATCH_ALLOW_MULTITHREADED 1  // the tests below are single-threaded

#include "test.h"
#include "fl/stl/scratch.h"
#include "fl/stl/detail/scratch_arena.hpp"
#include "fl/stl/int.h"
#include "fl/stl/cstring.h"
#include "fl/stl/bit_cast.h"
#include "fl/stl/align.h"
#include "fl/stl/unique_ptr.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

// Arena surrounded by guard bytes so a write past capacity is detectable.
template <fl::size N>
struct GuardedArena {
    enum : fl::size { kGuard = 16 };
    FL_ALIGNAS(16) u8 bytes[kGuard + N + kGuard];

    GuardedArena() { fl::memset(bytes, 0xA5, sizeof(bytes)); }
    u8* data() { return bytes + kGuard; }
    bool guardsIntact() const {
        for (fl::size i = 0; i < kGuard; ++i) {
            if (bytes[i] != 0xA5) return false;
            if (bytes[kGuard + N + i] != 0xA5) return false;
        }
        return true;
    }
};

fl::uptr addr(const void* p) { return fl::ptr_to_int(p); }

// Arena policy for detail::arena_scratch (the class behind fl::scratch when
// FL_SCRATCH_STACK_BYTES > 0), backed by a test-owned side_stack.
struct TestArena {
    static detail::side_stack* sStack;
    static int sNonLifoReleases;
    static void* acquire(fl::size bytes, fl::size align, fl::size* saved_top) {
        return sStack->push(bytes, align, saved_top);
    }
    static void release(fl::size saved_top, const void* block, fl::size bytes) {
        if (!sStack->pop(saved_top, block, bytes)) {
            ++sNonLifoReleases;
        }
    }
};
detail::side_stack* TestArena::sStack = nullptr;
int TestArena::sNonLifoReleases = 0;

template <typename T>
using test_scratch = detail::arena_scratch<T, TestArena>;

} // anonymous namespace

FL_TEST_CASE("side_stack - nested pushes are LIFO and restore the top") {
    GuardedArena<64> arena;
    detail::side_stack ss(arena.data(), 64);
    FL_CHECK_EQ(ss.capacity(), fl::size(64));
    FL_CHECK_EQ(ss.used(), fl::size(0));

    fl::size savedA = 99;
    void* a = ss.push(16, 1, &savedA);
    FL_REQUIRE(a != nullptr);
    FL_CHECK_EQ(savedA, fl::size(0));
    FL_CHECK_EQ(ss.used(), fl::size(16));

    fl::size savedB = 99;
    void* b = ss.push(8, 1, &savedB);
    FL_REQUIRE(b != nullptr);
    FL_CHECK_EQ(savedB, fl::size(16));
    FL_CHECK_EQ(addr(b), addr(a) + 16);
    FL_CHECK_EQ(ss.used(), fl::size(24));

    fl::memset(a, 0x11, 16);
    fl::memset(b, 0x22, 8);

    FL_CHECK(ss.pop(savedB, b, 8));
    FL_CHECK_EQ(ss.used(), fl::size(16));
    FL_CHECK(ss.pop(savedA, a, 16));
    FL_CHECK_EQ(ss.used(), fl::size(0));

    // Space is reused after release.
    fl::size savedC = 99;
    void* c = ss.push(16, 1, &savedC);
    FL_CHECK_EQ(addr(c), addr(a));
    FL_CHECK(ss.pop(savedC, c, 16));
    FL_CHECK(arena.guardsIntact());
}

FL_TEST_CASE("side_stack - exhaustion returns null and never writes past capacity") {
    GuardedArena<64> arena;
    detail::side_stack ss(arena.data(), 64);

    fl::size saved0 = 0;
    void* a = ss.push(48, 1, &saved0);
    FL_REQUIRE(a != nullptr);
    fl::memset(a, 0x33, 48);

    // 17 bytes do not fit in the remaining 16.
    fl::size saved1 = 1234;
    FL_CHECK(ss.push(17, 1, &saved1) == nullptr);
    FL_CHECK_EQ(ss.used(), fl::size(48));  // failed push leaves the top alone

    // A request larger than the whole arena (and one that would overflow
    // fl::size arithmetic) also fails cleanly.
    FL_CHECK(ss.push(1000, 1, &saved1) == nullptr);
    FL_CHECK(ss.push(fl::size(-1), 1, &saved1) == nullptr);

    // Exactly the remaining 16 bytes fit.
    fl::size saved2 = 0;
    void* b = ss.push(16, 1, &saved2);
    FL_REQUIRE(b != nullptr);
    fl::memset(b, 0x44, 16);
    FL_CHECK_EQ(ss.used(), fl::size(64));
    FL_CHECK(ss.push(1, 1, &saved1) == nullptr);

    FL_CHECK(ss.pop(saved2, b, 16));
    FL_CHECK(ss.pop(saved0, a, 48));
    FL_CHECK(arena.guardsIntact());
}

FL_TEST_CASE("side_stack - returned pointers honour the requested alignment") {
    GuardedArena<256> arena;
    // Start one byte into the arena so alignment must come from padding.
    detail::side_stack ss(arena.data() + 1, 255);

    fl::size s1 = 0, s2 = 0, s3 = 0, s4 = 0;
    void* p1 = ss.push(1, 1, &s1);
    void* p2 = ss.push(4, 4, &s2);
    void* p3 = ss.push(8, 8, &s3);
    void* p4 = ss.push(16, 16, &s4);
    FL_REQUIRE(p1 != nullptr);
    FL_REQUIRE(p2 != nullptr);
    FL_REQUIRE(p3 != nullptr);
    FL_REQUIRE(p4 != nullptr);
    FL_CHECK_EQ(addr(p2) % 4, fl::uptr(0));
    FL_CHECK_EQ(addr(p3) % 8, fl::uptr(0));
    FL_CHECK_EQ(addr(p4) % 16, fl::uptr(0));
    FL_CHECK(addr(p2) >= addr(p1) + 1);
    FL_CHECK(addr(p3) >= addr(p2) + 4);
    FL_CHECK(addr(p4) >= addr(p3) + 8);

    FL_CHECK(ss.pop(s4, p4, 16));
    FL_CHECK(ss.pop(s3, p3, 8));
    FL_CHECK(ss.pop(s2, p2, 4));
    FL_CHECK(ss.pop(s1, p1, 1));
    FL_CHECK_EQ(ss.used(), fl::size(0));

    // Padding is counted against capacity: an aligned request that would
    // only fit without its padding fails.
    detail::side_stack tight(arena.data() + 1, 8);
    fl::size st = 0;
    FL_CHECK(tight.push(8, 8, &st) == nullptr);
    FL_CHECK_EQ(tight.used(), fl::size(0));
    FL_CHECK(arena.guardsIntact());
}

FL_TEST_CASE("side_stack - high-water mark tracks the deepest nesting") {
    GuardedArena<128> arena;
    detail::side_stack ss(arena.data(), 128);
    FL_CHECK_EQ(ss.high_water(), fl::size(0));

    fl::size s1 = 0, s2 = 0;
    void* a = ss.push(40, 1, &s1);
    void* b = ss.push(30, 1, &s2);
    FL_CHECK_EQ(ss.high_water(), fl::size(70));
    FL_CHECK(ss.pop(s2, b, 30));
    FL_CHECK(ss.pop(s1, a, 40));
    FL_CHECK_EQ(ss.used(), fl::size(0));
    FL_CHECK_EQ(ss.high_water(), fl::size(70));  // sticky

    // A shallower pass does not lower it; a failed push does not raise it.
    void* c = ss.push(10, 1, &s1);
    fl::size sx = 0;
    FL_CHECK(ss.push(1000, 1, &sx) == nullptr);
    FL_CHECK(ss.pop(s1, c, 10));
    FL_CHECK_EQ(ss.high_water(), fl::size(70));

    ss.reset_high_water();
    FL_CHECK_EQ(ss.high_water(), fl::size(0));
    void* d = ss.push(5, 1, &s1);
    FL_CHECK_EQ(ss.high_water(), fl::size(5));
    FL_CHECK(ss.pop(s1, d, 5));
}

FL_TEST_CASE("side_stack - pop reports a non-LIFO release") {
    GuardedArena<64> arena;
    detail::side_stack ss(arena.data(), 64);

    fl::size sa = 0, sb = 0;
    void* a = ss.push(8, 1, &sa);
    void* b = ss.push(8, 1, &sb);
    FL_REQUIRE(a != nullptr);
    FL_REQUIRE(b != nullptr);
    // Releasing the older block while a newer one is live breaks the LIFO
    // contract; fl::scratch asserts on this result in debug builds.
    FL_CHECK_FALSE(ss.pop(sa, a, 8));
    // The top must not drop below the live block `b`, or the next push
    // would overlap it.
    FL_CHECK_EQ(ss.used(), fl::size(16));
    fl::size sc = 0;
    void* c = ss.push(8, 1, &sc);
    FL_CHECK_EQ(addr(c), addr(b) + 8);
    FL_CHECK(ss.pop(sc, c, 8));
    // Releasing `b` (now the top) unwinds to its saved top; `a`'s 8 bytes
    // stay reserved (leaked for the arena's life, never overlapped).
    FL_CHECK(ss.pop(sb, b, 8));
    FL_CHECK_EQ(ss.used(), fl::size(8));
}

FL_TEST_CASE("side_stack - a 4.5 KB arena holds a drawPolygon-sized frame") {
    // ESP8266 default capacity: drawPolygon's 32/64 Exact working set is
    // ~4.1 KB and must fit with room for a small nested user.
    static u8 big[4608];
    detail::side_stack ss(big, sizeof(big));
    fl::size s1 = 0, s2 = 0;
    void* a = ss.push(4134, 4, &s1);
    FL_REQUIRE(a != nullptr);
    void* b = ss.push(256, 4, &s2);
    FL_REQUIRE(b != nullptr);
    FL_CHECK(ss.pop(s2, b, 256));
    FL_CHECK(ss.pop(s1, a, 4134));
    FL_CHECK_EQ(ss.used(), fl::size(0));
}

FL_TEST_CASE("arena_scratch - nested scopes lease and release in LIFO order") {
    GuardedArena<64> arena;
    detail::side_stack ss(arena.data(), 64);
    TestArena::sStack = &ss;
    TestArena::sNonLifoReleases = 0;

    struct Frame { u32 words[4]; };  // 16 B, 4-byte aligned
    {
        test_scratch<Frame> outer;
        FL_REQUIRE(static_cast<bool>(outer));
        FL_CHECK_EQ(addr(outer.get()) % alignof(Frame), fl::uptr(0));
        outer->words[0] = 0xDEADBEEF;
        FL_CHECK_EQ(ss.used(), fl::size(16));
        {
            test_scratch<u8[20]> inner;
            FL_REQUIRE(static_cast<bool>(inner));
            (*inner)[19] = 1;
            FL_CHECK_EQ(ss.used(), fl::size(36));
        }
        FL_CHECK_EQ(ss.used(), fl::size(16));
        FL_CHECK_EQ((*outer).words[0], u32(0xDEADBEEF));
    }
    FL_CHECK_EQ(ss.used(), fl::size(0));
    FL_CHECK_EQ(ss.high_water(), fl::size(36));
    FL_CHECK_EQ(TestArena::sNonLifoReleases, 0);
    FL_CHECK(arena.guardsIntact());
}

FL_TEST_CASE("arena_scratch - exhaustion yields an empty object") {
    GuardedArena<64> arena;
    detail::side_stack ss(arena.data(), 64);
    TestArena::sStack = &ss;
    TestArena::sNonLifoReleases = 0;
    {
        test_scratch<u8[48]> a;
        FL_REQUIRE(static_cast<bool>(a));
        test_scratch<u8[32]> b;  // only 16 B left
        FL_CHECK_FALSE(static_cast<bool>(b));
        FL_CHECK(b.get() == nullptr);
        FL_CHECK_EQ(ss.used(), fl::size(48));
    }  // the empty `b` releases nothing; `a` releases cleanly
    FL_CHECK_EQ(ss.used(), fl::size(0));
    FL_CHECK_EQ(TestArena::sNonLifoReleases, 0);
    FL_CHECK(arena.guardsIntact());
}

FL_TEST_CASE("arena_scratch - out-of-order release is detected") {
    GuardedArena<64> arena;
    detail::side_stack ss(arena.data(), 64);
    TestArena::sStack = &ss;
    TestArena::sNonLifoReleases = 0;
    // Only possible when a scratch outlives a younger one, e.g. on the heap.
    auto older = fl::make_unique<test_scratch<u8[8]>>();
    auto younger = fl::make_unique<test_scratch<u8[8]>>();
    older.reset();
    FL_CHECK_EQ(TestArena::sNonLifoReleases, 1);
    FL_CHECK_EQ(ss.used(), fl::size(16));  // younger is still live
    younger.reset();
    FL_CHECK_EQ(TestArena::sNonLifoReleases, 1);  // younger was the top
    FL_CHECK_EQ(ss.used(), fl::size(8));  // older's bytes stay reserved
    TestArena::sStack = nullptr;
}

FL_TEST_CASE("inline_scratch - capacity-0 backend is a plain member") {
    // fl::scratch<T> is detail::inline_scratch<T> when the capacity is 0.
    struct Frame { int values[32]; };
    detail::inline_scratch<Frame> s;
    FL_CHECK(static_cast<bool>(s));
    FL_REQUIRE(s.get() != nullptr);
    for (int i = 0; i < 32; ++i) s->values[i] = i;
    FL_CHECK_EQ((*s).values[31], 31);
    FL_CHECK_EQ(s.get()->values[7], 7);
    // The value lives inside the object: no side stack involved.
    FL_CHECK_EQ(sizeof(detail::inline_scratch<Frame>), sizeof(Frame));

    detail::inline_scratch<u8[100]> arr;
    FL_CHECK(static_cast<bool>(arr));
    (*arr)[99] = 7;
    FL_CHECK_EQ((*arr)[99], 7);
}

FL_TEST_CASE("scratch - global side stack (production path)") {
    FL_STATIC_ASSERT(FL_SCRATCH_STACK_BYTES == 256,
                     "this TU must see the per-test capacity");
    FL_CHECK_EQ(detail::scratch_state.top, fl::size(0));
    fl::scratch_reset_high_water();
    FL_CHECK_EQ(fl::scratch_high_water(), fl::size(0));

    struct Frame { u32 words[16]; };  // 64 B
    {
        fl::scratch<Frame> outer;
        FL_REQUIRE(static_cast<bool>(outer));
        FL_CHECK(static_cast<void*>(outer.get()) >= static_cast<void*>(detail::scratch_arena));
        FL_CHECK_EQ(fl::ptr_to_int(outer.get()) % alignof(Frame), fl::uptr(0));
        outer->words[15] = 0xA5A5A5A5u;
        {
            fl::scratch<u8[128]> inner;
            FL_REQUIRE(static_cast<bool>(inner));
            (*inner)[127] = 9;
            // 64 + 128 + up to 3 B padding used; 100 B more does not fit.
            fl::scratch<u8[100]> tooBig;
            FL_CHECK_FALSE(static_cast<bool>(tooBig));
            FL_CHECK(tooBig.get() == nullptr);
        }
        FL_CHECK_EQ((*outer).words[15], u32(0xA5A5A5A5u));
        // The inner lease was returned: 128 B fit again.
        fl::scratch<u8[128]> again;
        FL_CHECK(static_cast<bool>(again));
    }
    FL_CHECK_EQ(detail::scratch_state.top, fl::size(0));
    FL_CHECK(fl::scratch_high_water() >= fl::size(64 + 128));
    FL_CHECK(fl::scratch_high_water() <= fl::size(256));

    fl::scratch_reset_high_water();
    FL_CHECK_EQ(fl::scratch_high_water(), fl::size(0));
    // The largest T the static_assert admits (sizeof + alignof - 1 <= 256)
    // always fits in the empty arena.
    {
        fl::scratch<u8[256]> whole;
        FL_CHECK(static_cast<bool>(whole));
    }
    FL_CHECK_EQ(fl::scratch_high_water(), fl::size(256));
    FL_CHECK_EQ(detail::scratch_state.top, fl::size(0));
}

FL_TEST_CASE("scratch - is_trivially_destructible gate") {
    struct Pod { int a; float b; };
    struct NonTrivial { ~NonTrivial() {} };
    FL_CHECK(fl::is_trivially_destructible<int>::value);
    FL_CHECK(fl::is_trivially_destructible<u8[16]>::value);
    FL_CHECK(fl::is_trivially_destructible<Pod>::value);
    FL_CHECK_FALSE(fl::is_trivially_destructible<NonTrivial>::value);
}

} // FL_TEST_FILE

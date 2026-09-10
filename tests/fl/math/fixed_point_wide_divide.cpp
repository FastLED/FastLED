// The narrow 64/32 division that replaces the libgcc call on cores with a
// 32-bit hardware divider (FastLED#4307).
//
// The macro is forced on before any FastLED header, so this file exercises
// the path a Cortex-M33 build takes even though the host's own build of
// `operator/` keeps the wide expression. Without that, the narrow path would
// ship compiled by no host and tested by nothing -- which is how
// `simd_noop.hpp` took two rounds of tuning against a build that excluded it
// (FastLED#4216).
#define FL_FIXED_POINT_NARROW_DIVIDE 1

#include "fl/math/fixed_point/s16x16.h"
#include "fl/math/fixed_point/u16x16.h"
#include "fl/math/fixed_point/wide_divide.h"
#include "fl/stl/int.h"
#include "test.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

namespace {

/// The expression `operator/` uses where a 64-bit divide is one instruction.
/// This is the reference the narrow path has to reproduce.
i32 wideSignedDivide(i32 a, i32 b) {
    return static_cast<i32>((static_cast<i64>(a) * static_cast<i64>(65536)) / b);
}

u32 wideUnsignedDivide(u32 a, u32 b) {
    return static_cast<u32>((static_cast<u64>(a) << 16) / b);
}

/// True when the exact quotient fits the 32 bits `operator/` returns.
///
/// Outside that the two paths are allowed to disagree, and do -- see the
/// case that pins it. Every other case here restricts itself to inputs where
/// agreement is the contract.
bool signedQuotientFits(i32 a, i32 b) {
    const i64 exact = (static_cast<i64>(a) * 65536) / b;
    return exact >= -2147483648LL && exact <= 2147483647LL;
}

bool unsignedQuotientFits(u32 a, u32 b) {
    return ((static_cast<u64>(a) << 16) / b) <= 0xFFFFFFFFull;
}

/// A cheap deterministic generator. `fl::random` would do, but a fixed
/// sequence written here makes a failing case reproducible from the file
/// alone.
u32 nextRandom(u32& state) {
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

}  // namespace

FL_TEST_CASE("the narrow divide reproduces the wide one on signed values") {
    const i32 kEdges[] = {1,      -1,         2,          -2,     3,
                          -3,     32767,      -32768,     0x7FFF, 0x8000,
                          65535,  65536,      -65536,     123456, -123456,
                          2147483647,         -2147483647 - 1};
    int checked = 0;
    for (i32 a : kEdges) {
        for (i32 b : kEdges) {
            if (b == 0 || !signedQuotientFits(a, b)) {
                continue;
            }
            const i32 narrow = (s16x16::from_raw(a) / s16x16::from_raw(b)).raw();
            FL_CHECK_EQ(narrow, wideSignedDivide(a, b));
            ++checked;
        }
    }
    // Vacuity guard: an edge list that skipped everything would pass in
    // silence. Measured 244 of the 289 pairs are in range; the rest overflow
    // 32 bits, which is where the two paths are allowed to differ.
    FL_CHECK_EQ(checked, 244);
}

FL_TEST_CASE("the narrow divide reproduces the wide one over a random sweep") {
    u32 state = 0x4307u;
    int checked = 0;
    for (int i = 0; i < 60000; ++i) {
        const i32 a = static_cast<i32>(nextRandom(state));
        const i32 b = static_cast<i32>(nextRandom(state));
        if (b == 0 || !signedQuotientFits(a, b)) {
            continue;
        }
        FL_CHECK_EQ((s16x16::from_raw(a) / s16x16::from_raw(b)).raw(),
                    wideSignedDivide(a, b));
        ++checked;
    }
    // Full-range operands mostly overflow, so this also sweeps the small
    // magnitudes a real Q16.16 division uses, where the correction loops in
    // Algorithm D are the part that has to be right.
    for (int i = 0; i < 60000; ++i) {
        const i32 a = static_cast<i32>(nextRandom(state) % 400000u) - 200000;
        const i32 b = static_cast<i32>(nextRandom(state) % 400000u) - 200000;
        if (b == 0 || !signedQuotientFits(a, b)) {
            continue;
        }
        FL_CHECK_EQ((s16x16::from_raw(a) / s16x16::from_raw(b)).raw(),
                    wideSignedDivide(a, b));
        ++checked;
    }
    FL_CHECK_GT(checked, 20000);
}

FL_TEST_CASE("the narrow divide reproduces the wide one on unsigned values") {
    u32 state = 0x16u;
    int checked = 0;
    for (int i = 0; i < 60000; ++i) {
        const u32 a = nextRandom(state);
        const u32 b = nextRandom(state);
        if (b == 0 || !unsignedQuotientFits(a, b)) {
            continue;
        }
        FL_CHECK_EQ((u16x16::from_raw(a) / u16x16::from_raw(b)).raw(),
                    wideUnsignedDivide(a, b));
        ++checked;
    }
    for (int i = 0; i < 60000; ++i) {
        const u32 a = nextRandom(state) % 400000u;
        const u32 b = nextRandom(state) % 400000u;
        if (b == 0 || !unsignedQuotientFits(a, b)) {
            continue;
        }
        FL_CHECK_EQ((u16x16::from_raw(a) / u16x16::from_raw(b)).raw(),
                    wideUnsignedDivide(a, b));
        ++checked;
    }
    FL_CHECK_GT(checked, 20000);
}

FL_TEST_CASE("the digit corrections are exercised, and are load-bearing") {
    // Algorithm D estimates each base-2^16 digit from the divisor's high half
    // alone, which can overshoot by one; two correction loops fix it. They
    // fire rarely -- sampling 40 million random pairs, the second corrected
    // 24,952 times and the *first* only 4 -- so a random sweep of any
    // plausible size can miss them entirely.
    //
    // It did. Disabling the second loop passed every other case in this file,
    // which is why these inputs are written down rather than trusted to come
    // up.
    struct Correcting {
        u32 numerator;
        u32 divisor;
        u32 quotient;
    };
    // Verified against the 64-bit division, and verified to change if either
    // loop is removed: without the first, the top four here return 1003954,
    // 15515131, 805094815 and 279820747; without the second, the bottom four
    // each return one too many.
    const Correcting kFirstLoop[] = {
        {2848704515u, 203480146u, 917498u},
        {2260423740u, 9618910u, 15400822u},
        {3409494998u, 277581u, 804971032u},
        {2591042145u, 607086u, 279707550u},
    };
    const Correcting kSecondLoop[] = {
        {656177183u, 3703039811u, 11612u},
        {3971301165u, 1536162826u, 169424u},
        {2226096481u, 2886857536u, 50535u},
        {1452674904u, 1535254716u, 62010u},
    };

    for (const auto& item : kFirstLoop) {
        FL_CHECK_EQ(divide64By32(item.numerator >> 16, item.numerator << 16,
                                 item.divisor),
                    item.quotient);
        FL_CHECK_EQ(wideUnsignedDivide(item.numerator, item.divisor),
                    item.quotient);
    }
    for (const auto& item : kSecondLoop) {
        FL_CHECK_EQ(divide64By32(item.numerator >> 16, item.numerator << 16,
                                 item.divisor),
                    item.quotient);
        FL_CHECK_EQ(wideUnsignedDivide(item.numerator, item.divisor),
                    item.quotient);
    }

    // And through the operator, so the wiring is covered too and not just
    // the routine.
    for (const auto& item : kSecondLoop) {
        FL_CHECK_EQ((u16x16::from_raw(item.numerator) /
                     u16x16::from_raw(item.divisor))
                        .raw(),
                    item.quotient);
    }
}

FL_TEST_CASE("the narrow divide truncates toward zero, as C++ division does") {
    // The sign is reapplied to a magnitude, which is only equivalent to
    // signed division because C++ truncates toward zero rather than flooring.
    // One raw unit divided by three is a third of a unit, so every one of
    // these has a fraction to lose and the direction it goes is visible.
    FL_CHECK_EQ((s16x16::from_raw(7) / s16x16::from_raw(3 * 65536)).raw(), 2);
    FL_CHECK_EQ((s16x16::from_raw(-7) / s16x16::from_raw(3 * 65536)).raw(), -2);
    FL_CHECK_EQ((s16x16::from_raw(7) / s16x16::from_raw(-3 * 65536)).raw(), -2);
    FL_CHECK_EQ((s16x16::from_raw(-7) / s16x16::from_raw(-3 * 65536)).raw(), 2);
    // Flooring would give -3 for the two negative-result cases, so this
    // distinguishes the two conventions rather than merely exercising them.
    FL_CHECK_EQ(wideSignedDivide(-7, 3 * 65536), -2);
}

FL_TEST_CASE("divide64By32 saturates where the wide path is undefined") {
    // The one place the two paths differ, stated rather than left to be
    // found. A zero divisor and a quotient too wide for 32 bits are both
    // undefined for the wide expression -- integer division by zero, and a
    // narrowing cast that cannot represent its value. The narrow routine
    // returns the largest 32-bit value for each.
    //
    // Neither is something a caller may rely on. This exists so that if
    // anyone ever wants them unified, the current behaviour is written down.
    FL_CHECK_EQ(divide64By32(0u, 12345u, 0u), 0xFFFFFFFFu);
    // hi >= divisor is exactly "the quotient needs more than 32 bits".
    FL_CHECK_EQ(divide64By32(5u, 0u, 5u), 0xFFFFFFFFu);
    FL_CHECK_EQ(divide64By32(5u, 0u, 4u), 0xFFFFFFFFu);
    // And one either side of that boundary, so the check is a boundary and
    // not a blanket refusal.
    FL_CHECK_EQ(divide64By32(4u, 0u, 5u), 0xCCCCCCCCu);
}

FL_TEST_CASE("divide64By32 handles a divisor that needs no normalisation") {
    // `shift` is zero when the divisor already has its top bit set, and a
    // 32-bit shift is undefined, so that case is spelled out separately in
    // the implementation. If it were folded in with the others this would
    // read whatever the undefined shift produced.
    FL_CHECK_EQ(divide64By32(0u, 0x80000000u, 0x80000000u), 1u);
    FL_CHECK_EQ(divide64By32(1u, 0u, 0x80000000u), 2u);
    FL_CHECK_EQ(divide64By32(0x7FFFFFFFu, 0xFFFFFFFFu, 0x80000000u),
                0xFFFFFFFFu);
}

}  // FL_TEST_FILE

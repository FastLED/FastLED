// ok no namespace fl
#pragma once

// IWYU pragma: private

/// @file simd_lane4.h
/// Four-lane loop replacement for the scalar SIMD backends.

/// Run @p ... once per lane of a 4-lane vector, with `i` bound to the lane
/// index as a compile-time constant.
///
/// This exists to keep the scalar backends from writing
/// `for (int i = 0; i < 4; ++i)`, which looks equivalent and is not.
///
/// Those backends hold their lanes in a `u32 data[4]` member. A trip-4 loop
/// indexes that array with a runtime induction variable, so the vector has to
/// live in memory: every lane costs a load and a store, and a four-lane
/// operation never reaches registers at all. Once the index is a constant the
/// subscripts fold away, the register allocator keeps the whole vector live,
/// and the stack traffic disappears.
///
/// Measured on one packed multiply-add -- `mulhi_i32_4` then `add_i32_4`, the
/// pair `fl::s16x16x4`'s operators reduce to -- compiled at each backend's
/// shipping optimisation level and counted over the whole function:
///
/// | backend | target | before | after |
/// |---|---|---|---|
/// | `simd_arm_dsp.hpp` | Cortex-M33, `-Os -march=armv8-m.main+fp+dsp` | 67 insns, 60 B frame | 41 insns, no frame |
/// | `simd_noop.hpp` | Cortex-M0+, `-Os -march=armv6-m` | 107 insns, 84 B frame | 82 insns, 28 B frame |
/// | `simd_riscv.hpp` | RV32IMAC, `-Os` | 21 insns, 64 B frame | 14 insns, no frame |
///
/// Cortex-M0+ keeps a frame because eight low registers cannot hold twelve
/// live lanes; it still loses a quarter of its instructions.
///
/// Nothing here needs a macro to work -- four hand-written statements with
/// literal subscripts generate the same code. The macro is how the bodies stay
/// single copies, and it is what stops the loops growing back: a future reader
/// who sees four near-identical statements will fold them into a `for`, and
/// this file is the note explaining why that undoes the fix.
///
/// The `constexpr int i` shadows nothing, because the loops this replaces used
/// `i` for exactly the same thing -- which is why their bodies transplanted
/// verbatim. Each lane gets its own scope so a body may declare locals.
///
/// FastLED#4216.
#define FL_SIMD_LANE4(...)                                                     \
    do {                                                                       \
        { constexpr int i = 0; __VA_ARGS__ }                                   \
        { constexpr int i = 1; __VA_ARGS__ }                                   \
        { constexpr int i = 2; __VA_ARGS__ }                                   \
        { constexpr int i = 3; __VA_ARGS__ }                                   \
    } while (0)

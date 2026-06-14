#pragma once

// IWYU pragma: private

// fl::json_number — 64-bit packed tagged Q-format number (FastLED #3022 / #3029).
//
// Bit layout: top 61 bits = payload (signed or unsigned per tag),
//             bottom 3 bits = tag enum value (8 slots, 3 currently used).
//
// Tags (current):
//   Q30_31  — signed Q30.31 (30 integer bits + 31 fractional bits, sign in
//             the top integer bit per two's-complement convention)
//   Q15_46  — signed Q15.46 (15 integer bits + 46 fractional bits)
//   UQ32_29 — unsigned UQ32.29 (32 integer bits + 29 fractional bits)
//
// All three formats occupy 61 bits, so any single value is round-trippable
// through one `i64` variant slot with the tag selecting the interpretation
// at read time. The intent is to give the FastLED JSON path a single
// integer-only numeric storage cell that has strictly greater precision
// than the existing 32-bit-wide `fl::fixed_point<>` zoo (s16x16, s0x32,
// u24x8, …) without ever pulling soft-FP into the link on Low-memory
// targets. See #3022 for the original design rationale and #3029 for the
// gap this resolves.
//
// Phase-1 limitations (this file):
//   * No JSON-text serializer yet: a json_number written to JSON falls
//     back to integer truncation of its int_part (lossy round-trip
//     through text). The proper Q-format decimal printer is phase-2.
//   * Tag space reserves five values (4..7 + 0) for future formats — for
//     example, a hypothetical packed-decimal-256 or rational variant.
//   * Tag 0 is reserved (not INT64) so that a default-constructed
//     json_number reads as "uninitialized" rather than a meaningful value.

#include "fl/stl/stdint.h"
#include "fl/stl/type_traits.h"
#include "fl/stl/pair.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"

namespace fl {

class json_number {
  public:
    // 3-bit tag space. Reserve slot 0 so default-constructed json_number is
    // distinguishable from a meaningful zero-valued tagged number.
    enum class tag_t : i32 {
        UNINIT  = 0,
        Q30_31  = 1,
        Q15_46  = 2,
        UQ32_29 = 3,
        // 4..7 reserved for future formats.
    };

    static constexpr int kTagBits = 3;
    static constexpr i64 kTagMask = (static_cast<i64>(1) << kTagBits) - 1;  // 0x7

  private:
    i64 mPacked = 0;

    constexpr json_number(i64 packed, int) FL_NOEXCEPT : mPacked(packed) {}

    // Combine a 61-bit signed payload with a tag into the packed i64.
    // Caller must guarantee `payload` fits in 61 bits (range [-2^60, 2^60)).
    static constexpr i64 pack_signed(i64 payload, tag_t t) FL_NOEXCEPT {
        // Arithmetic left-shift on signed values has UB if the result
        // overflows; reinterpret through u64 to dodge the rule.
        return static_cast<i64>(
            (static_cast<u64>(payload) << kTagBits) |
            static_cast<u64>(static_cast<i32>(t)));
    }

    static constexpr i64 pack_unsigned(u64 payload, tag_t t) FL_NOEXCEPT {
        return static_cast<i64>(
            (payload << kTagBits) | static_cast<u64>(static_cast<i32>(t)));
    }

  public:
    constexpr json_number() FL_NOEXCEPT = default;

    // ---- Construction ------------------------------------------------------

    // Raw packed-bits construct — escape hatch for serializers / tests.
    static constexpr json_number from_packed(i64 packed) FL_NOEXCEPT {
        return json_number(packed, 0);
    }

    // Construct from a signed Q30.31 payload. The full 61-bit payload is
    // the Q30.31 value (sign bit at position 60).
    static constexpr json_number from_q30_31(i64 q) FL_NOEXCEPT {
        return json_number(pack_signed(q, tag_t::Q30_31), 0);
    }

    // Construct from a signed Q15.46 payload.
    static constexpr json_number from_q15_46(i64 q) FL_NOEXCEPT {
        return json_number(pack_signed(q, tag_t::Q15_46), 0);
    }

    // Construct from an unsigned UQ32.29 payload.
    static constexpr json_number from_uq32_29(u64 q) FL_NOEXCEPT {
        return json_number(pack_unsigned(q, tag_t::UQ32_29), 0);
    }

    // Construct from any fl::fixed_point<>-shaped type. Picks the tag that
    // preserves precision and fits the integer range. Detection requires
    // `INT_BITS`, `FRAC_BITS`, and `raw()` members on FP (see the SFINAE
    // probe `is_fl_fixed_point` in fl/stl/json.h).
    template <typename FP>
    static json_number from_fixed_point(const FP& v) FL_NOEXCEPT {
        using RawT = decltype(v.raw());
        constexpr bool is_signed_fp = fl::is_signed<RawT>::value;
        constexpr int int_bits = FP::INT_BITS;
        constexpr int frac_bits = FP::FRAC_BITS;
        const i64 raw_i64 = static_cast<i64>(v.raw());

        // Existing fl::fixed_point types in src/fl/math/fixed_point/ are
        // 32-bit-wide (raw_type = i32 or u32), so the source value has at
        // most 32 significant bits. All three target Q-formats have 61 bits
        // and trivially fit it; the choice maximizes precision preservation
        // by promoting fractional bits as far as possible without
        // truncating the integer range.
        return select_tag<is_signed_fp, int_bits, frac_bits>(raw_i64);
    }

  private:
    // Compile-time tag dispatch. Branching is done through overload
    // selection on fl::true_type / fl::false_type rather than a ternary,
    // so the unselected branch is never instantiated — that matters here
    // because the unselected branch would otherwise contain a shift by a
    // negative count (UB in C++) for source FRAC_BITS values that exceed
    // the target Q-format's fractional capacity.
    //
    // Selection rules (covers every existing fl::fixed_point type in
    // src/fl/math/fixed_point/):
    //   Signed,  FRAC_BITS <= 31 → Q30.31  (shift left by 31 - FRAC_BITS)
    //   Signed,  FRAC_BITS  > 31 → Q15.46  (shift left by 46 - FRAC_BITS;
    //                                       only s0x32 (FRAC=32) hits this)
    //   Unsigned, FRAC_BITS <= 29 → UQ32.29
    //   Unsigned, FRAC_BITS  > 29 → Q15.46 (unsigned value packed into
    //                                       the signed slot; only u0x32
    //                                       (INT=0, FRAC=32) hits this so
    //                                       the sign bit at position 60 is
    //                                       always clear)
    template <bool IsSigned, int IntBits, int FracBits>
    static constexpr json_number select_tag(i64 raw) FL_NOEXCEPT {
        return select_signed(
            raw,
            fl::integral_constant<bool, IsSigned>(),
            fl::integral_constant<int, FracBits>());
    }

    // Signed source: dispatch on whether FRAC_BITS fits Q30.31.
    template <int FracBits>
    static constexpr json_number select_signed(
        i64 raw, fl::true_type /*IsSigned*/, fl::integral_constant<int, FracBits>) FL_NOEXCEPT {
        return select_signed_frac(
            raw,
            fl::integral_constant<bool, (FracBits <= 31)>(),
            fl::integral_constant<int, FracBits>());
    }

    template <int FracBits>
    static constexpr json_number select_signed_frac(
        i64 raw, fl::true_type /*FracFitsQ30_31*/, fl::integral_constant<int, FracBits>) FL_NOEXCEPT {
        return from_q30_31(raw << (31 - FracBits));
    }

    template <int FracBits>
    static constexpr json_number select_signed_frac(
        i64 raw, fl::false_type /*FracFitsQ30_31*/, fl::integral_constant<int, FracBits>) FL_NOEXCEPT {
        return from_q15_46(raw << (46 - FracBits));
    }

    // Unsigned source: dispatch on whether FRAC_BITS fits UQ32.29.
    template <int FracBits>
    static constexpr json_number select_signed(
        i64 raw, fl::false_type /*IsSigned*/, fl::integral_constant<int, FracBits>) FL_NOEXCEPT {
        return select_unsigned_frac(
            static_cast<u64>(raw),
            fl::integral_constant<bool, (FracBits <= 29)>(),
            fl::integral_constant<int, FracBits>());
    }

    template <int FracBits>
    static constexpr json_number select_unsigned_frac(
        u64 raw, fl::true_type /*FracFitsUQ32_29*/, fl::integral_constant<int, FracBits>) FL_NOEXCEPT {
        return from_uq32_29(raw << (29 - FracBits));
    }

    template <int FracBits>
    static constexpr json_number select_unsigned_frac(
        u64 raw, fl::false_type /*FracFitsUQ32_29*/, fl::integral_constant<int, FracBits>) FL_NOEXCEPT {
        return from_q15_46(static_cast<i64>(raw) << (46 - FracBits));
    }

  public:
    // ---- Accessors ---------------------------------------------------------

    constexpr i64 packed() const FL_NOEXCEPT { return mPacked; }

    constexpr tag_t tag() const FL_NOEXCEPT {
        return static_cast<tag_t>(static_cast<i32>(mPacked & kTagMask));
    }

    constexpr bool is_uninit() const FL_NOEXCEPT { return tag() == tag_t::UNINIT; }
    constexpr bool is_q30_31() const FL_NOEXCEPT { return tag() == tag_t::Q30_31; }
    constexpr bool is_q15_46() const FL_NOEXCEPT { return tag() == tag_t::Q15_46; }
    constexpr bool is_uq32_29() const FL_NOEXCEPT { return tag() == tag_t::UQ32_29; }

    // Raw signed 61-bit payload (arithmetic shift sign-extends from the
    // i64 top bit). Use this when the caller already knows the tag.
    constexpr i64 raw_payload_signed() const FL_NOEXCEPT {
        return mPacked >> kTagBits;
    }

    // Raw unsigned 61-bit payload.
    constexpr u64 raw_payload_unsigned() const FL_NOEXCEPT {
        return static_cast<u64>(mPacked) >> kTagBits;
    }

    // ---- Pair-style decomposition per FastLED #3022 design body ----------
    //
    // Each accessor splits the 61-bit payload into a (integer_part,
    // fractional_part) pair sized to fit the format's bit widths. The
    // accessor is well-defined regardless of the current tag — callers
    // should consult tag() first if they need to honor the stored format.

    // Q30.31: 30-bit integer (sign-extended into i32) + 31-bit fractional.
    fl::pair<i32, u32> to_q30_31() const FL_NOEXCEPT {
        const i64 p = raw_payload_signed();
        const i32 int_part = static_cast<i32>(p >> 31);                // sign-ext.
        const u32 frac_part = static_cast<u32>(p & ((static_cast<i64>(1) << 31) - 1));
        return fl::pair<i32, u32>(int_part, frac_part);
    }

    // Q15.46: 15-bit integer + 46-bit fractional. Frac doesn't fit in u32;
    // returning the top 32 of those 46 bits matches the signature spelled
    // out in the issue body — callers wanting the full fractional should
    // call raw_payload_signed() and shift directly.
    fl::pair<i32, u32> to_q15_46() const FL_NOEXCEPT {
        const i64 p = raw_payload_signed();
        const i32 int_part = static_cast<i32>(p >> 46);
        const u32 frac_top32 = static_cast<u32>(
            (static_cast<u64>(p) & ((static_cast<u64>(1) << 46) - 1)) >> 14);
        return fl::pair<i32, u32>(int_part, frac_top32);
    }

    // UQ32.29: 32-bit integer + 29-bit fractional.
    fl::pair<u32, u32> to_uq32_29() const FL_NOEXCEPT {
        const u64 p = raw_payload_unsigned();
        const u32 int_part = static_cast<u32>(p >> 29);
        const u32 frac_part = static_cast<u32>(p & ((static_cast<u64>(1) << 29) - 1));
        return fl::pair<u32, u32>(int_part, frac_part);
    }

    // ---- Extract into a fl::fixed_point<>-shaped type --------------------
    //
    // Reverse of from_fixed_point<FP>: re-aligns the stored Q-format
    // payload to the target FP's INT_BITS / FRAC_BITS layout and
    // reconstructs via FP::from_raw. Lossy when target FRAC_BITS <
    // source-format FRAC_BITS (typical case for the 32-bit Q types since
    // the source is one of 61-bit Q30.31 / Q15.46 / UQ32.29).
    template <typename FP>
    FP to_fixed_point() const FL_NOEXCEPT {
        using RawT = decltype(FP().raw());
        const int fp_frac = FP::FRAC_BITS;
        const tag_t t = tag();
        if (t == tag_t::Q30_31) {
            return FP::from_raw(align_shift<RawT>(raw_payload_signed(), 31 - fp_frac));
        }
        if (t == tag_t::Q15_46) {
            return FP::from_raw(align_shift<RawT>(raw_payload_signed(), 46 - fp_frac));
        }
        if (t == tag_t::UQ32_29) {
            return FP::from_raw(align_shift<RawT>(static_cast<i64>(raw_payload_unsigned()), 29 - fp_frac));
        }
        return FP();
    }

  private:
    // Right-shift by `delta` if non-negative; left-shift by -delta otherwise.
    // Routes negative shift through an unsigned reinterpret so no branch
    // evaluates `v << -1`-style undefined behavior at runtime — the
    // selected direction always sees a non-negative count.
    template <typename RawT>
    static constexpr RawT align_shift(i64 v, int delta) FL_NOEXCEPT {
        return delta >= 0
            ? static_cast<RawT>(v >> static_cast<unsigned>(delta))
            : static_cast<RawT>(static_cast<u64>(v) << static_cast<unsigned>(-delta));
    }

  public:

    // ---- Comparisons -------------------------------------------------------

    constexpr bool operator==(const json_number& o) const FL_NOEXCEPT {
        return mPacked == o.mPacked;
    }
    constexpr bool operator!=(const json_number& o) const FL_NOEXCEPT {
        return mPacked != o.mPacked;
    }
};

FL_STATIC_ASSERT(sizeof(json_number) == 8, "json_number must be 8 bytes (one i64 slot)");

}  // namespace fl

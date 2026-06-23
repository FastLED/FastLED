#pragma once

// fl::number -- 64-bit packed tagged numeric value.
//
// A purpose-built single-slot replacement for `fl::variant<i64, float,
// double>` that fits the JSON / RPC / serialized-numeric use case at half
// the storage cost. Designed for FastLED #3022; reusable beyond JSON for
// anyone trafficking small numeric values whose precision needs are
// dominated by integer ranges with optional fractional resolution.
//
// Layout (64 bits total):
//   Bits [63:61]  tag      -- 3-bit discriminator (8 representations)
//   Bits [60:0]   payload  -- 61-bit value, interpretation per `tag`
//
// The shared 64-bit slot means a `number` slots into the same union storage
// as a `double` with zero size overhead -- no separate discriminator byte
// gets padded into a 16-byte cell.
//
// Tag semantics (initial set; tags 4..7 reserved for future expansion):
//   SIGNED_INT   -- payload is a 61-bit two's-complement signed integer
//                   (range: ~ +/- 1.15e18). Common case for JSON / RPC
//                   integers; covers microsecond timestamps and
//                   LED-count-like values without truncation.
//   UNSIGNED_INT -- payload is a 61-bit unsigned integer.
//                   (range: 0 .. ~2.3e18).
//   FLOAT_60     -- payload is a custom 60-bit IEEE-754-like float:
//                     bit 60     sign
//                     bits 59:49 11-bit exponent (bias 1023, same as
//                                IEEE-754 double, but range clipped to fit)
//                     bits 48:0  49-bit mantissa (implicit-1, like double)
//                   Reuses the soft_float bit-codec for widening to/from
//                   IEEE-754 double when an FPU is present.
//                   This is the slot the "float60 with identifier tag"
//                   note in the design refers to.
//   (4..7 reserved -- future tags for Q-format fixed-point, IEEE-half
//   on host, error/NaN sentinel for parse failures, etc.)
//
// This header defines only the type, tag enum, and basic accessors.
// Arithmetic, parser, serializer, and JSON-side adoption land in
// follow-up PRs so each step ships with verifiable LPC-side bloat
// reductions without bundling unrelated design changes.

#include "fl/stl/cstddef.h"
#include "fl/stl/noexcept.h"
#include "fl/stl/static_assert.h"
#include "fl/stl/stdint.h"

namespace fl {

class number {
public:
    enum class tag : fl::u8 {
        SIGNED_INT   = 0,
        UNSIGNED_INT = 1,
        FLOAT_60     = 2,
        RESERVED_3   = 3,
        RESERVED_4   = 4,
        RESERVED_5   = 5,
        RESERVED_6   = 6,
        RESERVED_7   = 7,
    };

    // 64-bit raw representation: tag in top 3 bits, payload in low 61.
    fl::u64 raw;

    static constexpr fl::u64 kPayloadMask = (fl::u64(1) << 61) - 1;
    static constexpr int     kPayloadBits = 61;
    static constexpr int     kTagShift    = 61;

    constexpr number() FL_NO_EXCEPT : raw(0) {}

    // Direct construction from (tag, payload). Caller is responsible for
    // ensuring the payload fits in 61 bits (or has been masked).
    constexpr number(tag t, fl::u64 payload) FL_NO_EXCEPT
        : raw((static_cast<fl::u64>(t) << kTagShift) |
              (payload & kPayloadMask)) {}

    // Factories for the initial tag set.

    // Construct a SIGNED_INT number from any 64-bit signed integer that
    // fits in 61 signed bits. Saturates at +/-(2^60 - 1) for out-of-range
    // inputs (returns SIGNED_INT_MAX / MIN-of-61-bits) so the call site
    // never silently aliases into a different tag's payload.
    static number from_signed(fl::i64 v) FL_NO_EXCEPT {
        constexpr fl::i64 kMax = (fl::i64(1) << 60) - 1;
        constexpr fl::i64 kMin = -(fl::i64(1) << 60);
        if (v > kMax) v = kMax;
        if (v < kMin) v = kMin;
        // Sign-extend into 61 bits, then mask.
        const fl::u64 payload = static_cast<fl::u64>(v) & kPayloadMask;
        return number(tag::SIGNED_INT, payload);
    }

    // Construct an UNSIGNED_INT number; saturates at 2^61 - 1.
    static number from_unsigned(fl::u64 v) FL_NO_EXCEPT {
        if (v > kPayloadMask) v = kPayloadMask;
        return number(tag::UNSIGNED_INT, v);
    }

    // Tag / payload accessors.
    constexpr tag get_tag() const FL_NO_EXCEPT {
        return static_cast<tag>(raw >> kTagShift);
    }
    constexpr fl::u64 payload() const FL_NO_EXCEPT {
        return raw & kPayloadMask;
    }

    // Sign-extended payload for SIGNED_INT extraction. Caller is
    // responsible for asserting `get_tag() == tag::SIGNED_INT`.
    fl::i64 as_signed_payload() const FL_NO_EXCEPT {
        // Sign-extend bit 60 across the upper 3 bits.
        const fl::u64 p = payload();
        const fl::u64 sign_bit = p & (fl::u64(1) << 60);
        return static_cast<fl::i64>(sign_bit ? (p | ~kPayloadMask) : p);
    }
};

FL_STATIC_ASSERT(sizeof(number) == sizeof(fl::u64),
                 "fl::number must be exactly 64 bits to slot into JSON / RPC "
                 "value cells without padding overhead.");

} // namespace fl

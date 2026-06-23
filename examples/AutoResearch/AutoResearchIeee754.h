/// @file AutoResearchIeee754.h
/// @brief On-device verification for the integer-only IEEE 754 decimal codec.

#pragma once

#include "fl/stl/ieee754_string.h"
#include "fl/stl/int.h"
#include "fl/stl/string.h"
#include "fl/stl/cstring.h"

namespace autoresearch {
namespace ieee754_check {

struct Result {
    bool success;
    fl::u32 tests_run;
    fl::u32 tests_failed;
    const char* first_failure;
    fl::u32 expected_bits;
    fl::u32 actual_bits;
};

struct ParseCase {
    const char* text;
    fl::u32 expected_bits;
    bool exact;
};

struct FormatCase {
    fl::u32 bits;
    int precision;
    const char* expected_text;
};

inline fl::u32 absDiff(fl::u32 a, fl::u32 b) FL_NO_EXCEPT {
    return a > b ? a - b : b - a;
}

inline bool withinOneUlp(fl::u32 actual, fl::u32 expected) FL_NO_EXCEPT {
    if (actual == expected) {
        return true;
    }
    if ((actual & 0x80000000u) != (expected & 0x80000000u)) {
        return false;
    }
    return absDiff(actual, expected) <= 1u;
}

inline void recordCheck(Result& r, const char* name, bool passed,
                        fl::u32 expected = 0, fl::u32 actual = 0) FL_NO_EXCEPT {
    ++r.tests_run;
    if (passed) {
        return;
    }
    ++r.tests_failed;
    if (r.first_failure == nullptr) {
        r.first_failure = name;
        r.expected_bits = expected;
        r.actual_bits = actual;
    }
}

inline Result run() {
    Result r{};
    r.success = true;
    r.first_failure = nullptr;

    static const ParseCase kParseCases[] = {
        {"0", 0x00000000u, true},
        {"-0", 0x80000000u, true},
        {"1", 0x3F800000u, true},
        {"-1", 0xBF800000u, true},
        {"0.5", 0x3F000000u, true},
        {"2.5", 0x40200000u, true},
        {"3.14159", 0x40490FD0u, false},
        {"2.71828", 0x402DF84Du, false},
        {"100", 0x42C80000u, true},
        {"1000", 0x447A0000u, true},
        {"1e30", 0x7149F2CAu, false},
        {"1e-30", 0x0DA24260u, false},
        {"1e-40", 0x00000000u, true},
        {"1e40", 0x7F800000u, true},
        {"-1e40", 0xFF800000u, true},
    };

    for (fl::size i = 0; i < sizeof(kParseCases) / sizeof(kParseCases[0]); ++i) {
        const ParseCase& c = kParseCases[i];
        fl::size consumed = 0;
        const fl::u32 actual =
            fl::ieee754_parse_decimal(c.text, fl::strlen(c.text), &consumed);
        const bool value_ok = c.exact
            ? actual == c.expected_bits
            : withinOneUlp(actual, c.expected_bits);
        recordCheck(r, c.text, value_ok && consumed == fl::strlen(c.text),
                    c.expected_bits, actual);
    }

    static const FormatCase kFormatCases[] = {
        {0x00000000u, 6, "0.000000"},
        {0x80000000u, 6, "-0.000000"},
        {0x3F800000u, 6, "1.000000"},
        {0xBF800000u, 6, "-1.000000"},
        {0x3F000000u, 6, "0.500000"},
        {0x41200000u, 2, "10.00"},
        {0x7F800000u, 6, "inf"},
        {0xFF800000u, 6, "-inf"},
        {0x7FC00000u, 6, "nan"},
    };

    for (fl::size i = 0; i < sizeof(kFormatCases) / sizeof(kFormatCases[0]); ++i) {
        const FormatCase& c = kFormatCases[i];
        const fl::string actual = fl::ieee754_format_decimal(c.bits, c.precision);
        recordCheck(r, c.expected_text, actual == c.expected_text,
                    c.bits, c.bits);
    }

    static const fl::u32 kRoundTripBits[] = {
        0x00000000u, 0x80000000u, 0x3F800000u, 0xBF800000u,
        0x3F000000u, 0x40200000u, 0x40490FDBu, 0x40B00000u,
        0x42C80000u, 0x447A0000u,
    };

    for (fl::size i = 0; i < sizeof(kRoundTripBits) / sizeof(kRoundTripBits[0]); ++i) {
        const fl::u32 expected = kRoundTripBits[i];
        const fl::string text = fl::ieee754_format_decimal(expected, 9);
        const fl::u32 actual =
            fl::ieee754_parse_decimal(text.c_str(), text.size());
        recordCheck(r, "roundtrip", withinOneUlp(actual, expected),
                    expected, actual);
    }

    fl::size consumed = 123u;
    (void)fl::ieee754_parse_decimal("nan", 3, &consumed);
    recordCheck(r, "nan-reject", consumed == 0u, 0u, static_cast<fl::u32>(consumed));

    r.success = r.tests_failed == 0u;
    return r;
}

} // namespace ieee754_check
} // namespace autoresearch

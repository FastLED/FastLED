// standalone test
#include "FastLED.h"
#include "fl/stl/stdio.h"

#include "test.h"

using namespace fl;

FL_TEST_CASE("fl::printf handles int64_t") {
    fl::i64 large_signed = 9223372036854775807LL;  // Max int64_t
    fl::i64 negative = -9223372036854775807LL;
    fl::u64 large_unsigned = 18446744073709551615ULL;  // Max uint64_t

    char buf[128];

    // Test %d with int64_t
    fl::snprintf(buf, sizeof(buf), "Value: %d", large_signed);
    FL_CHECK(fl::string(buf) == "Value: 9223372036854775807");

    // Test %d with negative int64_t
    fl::snprintf(buf, sizeof(buf), "Negative: %d", negative);
    FL_CHECK(fl::string(buf) == "Negative: -9223372036854775807");

    // Test %u with uint64_t
    fl::snprintf(buf, sizeof(buf), "Unsigned: %u", large_unsigned);
    FL_CHECK(fl::string(buf) == "Unsigned: 18446744073709551615");

    // Test %d with regular int
    fl::snprintf(buf, sizeof(buf), "Small: %d", 42);
    FL_CHECK(fl::string(buf) == "Small: 42");
}

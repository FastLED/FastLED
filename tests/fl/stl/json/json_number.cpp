// fl::json ↔ fl::json_number round-trip tests (FastLED #3022 / #3029).
// Integer-only path; deliberately no <cmath>, no float comparisons.

#include "test.h"

#include "fl/stl/json.h"
#include "fl/stl/json/json_number.h"
#include "fl/math/fixed_point/s16x16.h"
#include "fl/math/fixed_point/s0x32.h"
#include "fl/math/fixed_point/u24x8.h"
#include "fl/stl/string.h"
#include "fl/stl/utility.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("json_number: round-trip s16x16 through json variant") {
    // s16x16 raw value: 1.5 → 0x00018000 (1 << 16 + 0x8000).
    const s16x16 src = s16x16::from_raw(0x00018000);
    json doc = json::object();
    doc.set("v", src);

    FL_CHECK(doc["v"].is_fixed_point());

    auto out = doc["v"].as_fixed_point<s16x16>();
    FL_REQUIRE(out.has_value());
    FL_CHECK(out->raw() == src.raw());
}

FL_TEST_CASE("json_number: round-trip s0x32 through Q15.46 path") {
    // s0x32 has 32 fractional bits — from_fixed_point routes through Q15.46
    // (32 ≤ 46 frac bits → lossless).
    const s0x32 src = s0x32::from_raw(0x12345678);
    json doc;
    doc = src;

    FL_CHECK(doc.is_fixed_point());

    auto n = doc.as_json_number();
    FL_REQUIRE(n.has_value());
    FL_CHECK(n->is_q15_46());

    auto out = doc.as_fixed_point<s0x32>();
    FL_REQUIRE(out.has_value());
    FL_CHECK(out->raw() == src.raw());
}

FL_TEST_CASE("json_number: round-trip u24x8 through UQ32.29 path") {
    const u24x8 src = u24x8::from_raw(0x00ABCDEF);
    json doc(src);

    FL_CHECK(doc.is_fixed_point());

    auto n = doc.as_json_number();
    FL_REQUIRE(n.has_value());
    FL_CHECK(n->is_uq32_29());

    auto out = doc.as_fixed_point<u24x8>();
    FL_REQUIRE(out.has_value());
    FL_CHECK(out->raw() == src.raw());
}

FL_TEST_CASE("json_number: pair accessors expose Q-format decomposition") {
    // Build a json_number directly with a known Q30.31 payload.
    // Q30.31 value = 3.5 → integer 3, fractional 0.5
    //              = (3 << 31) | (1 << 30) = 0x000000018_4000_0000
    const i64 q30_31_payload = (static_cast<i64>(3) << 31) | (static_cast<i64>(1) << 30);
    json_number n = json_number::from_q30_31(q30_31_payload);

    FL_CHECK(n.is_q30_31());
    fl::pair<i32, u32> p = n.to_q30_31();
    FL_CHECK(p.first == 3);
    FL_CHECK(p.second == static_cast<u32>(1) << 30);
}

FL_TEST_CASE("json_number: tagged number is_number() but not is_int()") {
    json doc;
    doc = s16x16::from_raw(0x00010000);  // 1.0
    FL_CHECK(doc.is_number());
    FL_CHECK(doc.is_fixed_point());
    FL_CHECK(!doc.is_int());  // i64 slot is distinct from json_number slot
}

}  // FL_TEST_FILE

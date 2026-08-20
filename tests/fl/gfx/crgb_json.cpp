#include "test.h"

#include "fl/gfx/crgb_json.h"
#include "fl/stl/json.h"
#include "fl/stl/span.h"

FL_TEST_FILE(FL_FILEPATH) {

using namespace fl;

FL_TEST_CASE("CRGB JSON round-trip preserves pixels") {
    const CRGB source[] = {
        CRGB(0, 1, 2),
        CRGB(255, 128, 64),
        CRGB::Blue,
    };

    const optional<json> encoded = crgbToJson(source);
    FL_REQUIRE_TRUE(encoded);
    FL_REQUIRE_TRUE(encoded->is_object());
    FL_CHECK_EQ((*encoded)["version"] | 0, 1);
    FL_REQUIRE_TRUE((*encoded)["pixels"].is_array());
    FL_CHECK_EQ((*encoded)["pixels"].size(), 3);

    const json parsed = json::parse(encoded->to_string());
    CRGB restored[3] = {};
    FL_REQUIRE_TRUE(crgbFromJson(parsed, restored));

    for (size i = 0; i < 3; ++i) {
        FL_CHECK_EQ(restored[i], source[i]);
    }
}

FL_TEST_CASE("CRGB JSON rejects invalid input without partial writes") {
    const CRGB sentinel(7, 8, 9);
    CRGB output[] = {sentinel, sentinel};

    FL_SUBCASE("wrong schema version") {
        const json input = json::parse(
            R"({"version":2,"pixels":[[1,2,3],[4,5,6]]})");
        FL_CHECK_FALSE(crgbFromJson(input, output));
    }

    FL_SUBCASE("pixel count mismatch") {
        const json input = json::parse(
            R"({"version":1,"pixels":[[1,2,3]]})");
        FL_CHECK_FALSE(crgbFromJson(input, output));
    }

    FL_SUBCASE("invalid later pixel") {
        const json input = json::parse(
            R"({"version":1,"pixels":[[1,2,3],[4,999,6]]})");
        FL_CHECK_FALSE(crgbFromJson(input, output));
    }

    FL_SUBCASE("wrong pixel shape") {
        const json input = json::parse(
            R"({"version":1,"pixels":[[1,2,3],[4,5]]})");
        FL_CHECK_FALSE(crgbFromJson(input, output));
    }

    FL_SUBCASE("non-numeric channel") {
        const json input = json::parse(
            R"({"version":1,"pixels":[[1,2,3],[4,"5",6]]})");
        FL_CHECK_FALSE(crgbFromJson(input, output));
    }

    FL_SUBCASE("boolean schema version") {
        const json input = json::parse(
            R"({"version":true,"pixels":[[1,2,3],[4,5,6]]})");
        FL_CHECK_FALSE(crgbFromJson(input, output));
    }

    FL_SUBCASE("boolean channel") {
        const json input = json::parse(
            R"({"version":1,"pixels":[[1,2,3],[4,true,6]]})");
        FL_CHECK_FALSE(crgbFromJson(input, output));
    }

    FL_SUBCASE("schema version wider than int") {
        const json input = json::parse(
            R"({"version":2147483648,"pixels":[[1,2,3],[4,5,6]]})");
        FL_CHECK_FALSE(crgbFromJson(input, output));
    }

    FL_SUBCASE("channel wider than int") {
        const json input = json::parse(
            R"({"version":1,"pixels":[[1,2,3],[4,2147483648,6]]})");
        FL_CHECK_FALSE(crgbFromJson(input, output));
    }

    FL_CHECK_EQ(output[0], sentinel);
    FL_CHECK_EQ(output[1], sentinel);
}

FL_TEST_CASE("CRGB JSON supports an empty pixel span") {
    const span<const CRGB> emptyInput;
    const optional<json> encoded = crgbToJson(emptyInput);
    FL_REQUIRE_TRUE(encoded);
    FL_CHECK_EQ((*encoded)["pixels"].size(), 0);

    span<CRGB> emptyOutput;
    FL_CHECK_TRUE(crgbFromJson(*encoded, emptyOutput));
}

} // FL_TEST_FILE

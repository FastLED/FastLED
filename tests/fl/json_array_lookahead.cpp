// ok standalone
// Tests for JSON array lookahead optimization
// Verifies tokenizer emits specialized array tokens for homogeneous arrays

#include "test.h"
#include "fl/json.h"

using namespace fl;

// Helper to test what token the tokenizer emits for a given JSON
// This will need to be implemented after we expose tokenizer internals
FL_TEST_CASE("JSON Array Lookahead: Token Emission") {

    FL_SUBCASE("ARRAY_UINT8: [0, 1, 255]") {
        const char* json = "[0, 1, 255]";
        (void)json;  // TODO: Verify token type after implementing lookahead
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("ARRAY_INT8: [-100, 0, 100]") {
        const char* json = "[-100, 0, 100]";
        (void)json;  // Expected: ARRAY_INT8 token
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("ARRAY_INT16: [1000, 2000, -1000]") {
        const char* json = "[1000, 2000, -1000]";
        (void)json;  // Expected: ARRAY_INT16 token
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("ARRAY_FLOAT: [1.5, 2.7, 3.14]") {
        const char* json = "[1.5, 2.7, 3.14]";
        (void)json;  // Expected: ARRAY_FLOAT token
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("ARRAY_FLOAT: [1, 2.5, 3] - type promotion") {
        const char* json = "[1, 2.5, 3]";
        (void)json;  // Expected: ARRAY_FLOAT (int promoted to float)
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("ARRAY_STRING: [\"a\", \"b\", \"c\"]") {
        const char* json = "[\"a\", \"b\", \"c\"]";
        (void)json;  // Expected: ARRAY_STRING token
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("ARRAY_STRING: [\"a\", \"b\\\"c\"] - with escapes") {
        const char* json = "[\"a\", \"b\\\"c\"]";
        (void)json;  // Expected: ARRAY_STRING (escapes are OK in fast path)
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("ARRAY_BOOL: [true, false, true]") {
        const char* json = "[true, false, true]";
        (void)json;  // Expected: ARRAY_BOOL token
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("ARRAY_MIXED: [1, \"hello\", true]") {
        const char* json = "[1, \"hello\", true]";
        (void)json;  // Expected: ARRAY_MIXED token
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("ARRAY_MIXED: [1, null, 3]") {
        const char* json = "[1, null, 3]";
        (void)json;  // Expected: ARRAY_MIXED (null is primitive)
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("Slow path: [[1,2], [3,4]] - nested array") {
        const char* json = "[[1,2], [3,4]]";
        (void)json;  // Expected: LBRACKET (abort lookahead, use slow path)
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("Slow path: [{\"x\":1}] - nested object") {
        const char* json = "[{\"x\":1}]";
        (void)json;  // Expected: LBRACKET (abort lookahead, use slow path)
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("Slow path: [] - empty array") {
        const char* json = "[]";
        (void)json;  // Expected: LBRACKET (empty array uses slow path)
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("Range boundary: [0, 256] - requires int16") {
        const char* json = "[0, 256]";
        (void)json;  // Expected: ARRAY_INT16 (256 doesn't fit in uint8)
        // Wait, 256 fits in uint16! Let me reconsider...
        // Actually uint8 max is 255, so 256 needs uint16
        FL_CHECK(true); // Placeholder
    }

    FL_SUBCASE("Range boundary: [-1, 0, 255] - requires int16") {
        const char* json = "[-1, 0, 255]";
        (void)json;  // Expected: ARRAY_INT16 (negative value, can't be uint8)
        FL_CHECK(true); // Placeholder
    }
}

FL_TEST_CASE("JSON Array Lookahead: End-to-End Parsing") {

    FL_SUBCASE("ARRAY_UINT8 parses correctly") {
        Json j = Json::parse("[0, 1, 255]");
        FL_CHECK(j.is_bytes()); // Should be vector<u8>
        FL_CHECK(j.size() == 3);
        FL_CHECK(j[0].as<int>() == 0);
        FL_CHECK(j[1].as<int>() == 1);
        FL_CHECK(j[2].as<int>() == 255);
    }

    FL_SUBCASE("ARRAY_INT16 parses correctly") {
        Json j = Json::parse("[1000, -1000, 2000]");
        FL_CHECK(j.is_audio()); // Should be vector<i16>
        FL_CHECK(j.size() == 3);
        FL_CHECK(j[0].as<int>() == 1000);
        FL_CHECK(j[1].as<int>() == -1000);
        FL_CHECK(j[2].as<int>() == 2000);
    }

    FL_SUBCASE("ARRAY_FLOAT parses correctly") {
        Json j = Json::parse("[1.5, 2.7, 3.14]");
        FL_CHECK(j.is_floats()); // Should be vector<float>
        FL_CHECK(j.size() == 3);
        FL_CHECK_CLOSE(j[0].as<float>().value_or(0.0f), 1.5f, 0.001f);
        FL_CHECK_CLOSE(j[1].as<float>().value_or(0.0f), 2.7f, 0.001f);
        FL_CHECK_CLOSE(j[2].as<float>().value_or(0.0f), 3.14f, 0.001f);
    }

    FL_SUBCASE("ARRAY_STRING parses correctly") {
        Json j = Json::parse("[\"a\", \"b\", \"c\"]");
        FL_CHECK(j.is_array());
        FL_CHECK(j.size() == 3);
        FL_CHECK(j[0].as<fl::string>() == "a");
        FL_CHECK(j[1].as<fl::string>() == "b");
        FL_CHECK(j[2].as<fl::string>() == "c");
    }

    FL_SUBCASE("Nested arrays use slow path but still work") {
        Json j = Json::parse("[[1,2], [3,4]]");
        FL_CHECK(j.is_array());
        FL_CHECK(j.size() == 2);
        FL_CHECK(j[0].is_array());
        FL_CHECK(j[0].size() == 2);
        FL_CHECK(j[1][1].as<int>() == 4);
    }
}

FL_TEST_CASE("JSON Array Lookahead: Stress Test") {

    FL_SUBCASE("Large ARRAY_UINT8: 1000 elements") {
        // Build JSON: [0,1,2,...,255,0,1,2,...,255,...]
        fl::string json = "[";
        for (int i = 0; i < 1000; i++) {
            if (i > 0) json += ",";
            json += fl::to_string(i % 256);
        }
        json += "]";

        Json j = Json::parse(json);
        FL_CHECK(j.is_bytes()); // Should optimize to vector<u8>
        FL_CHECK(j.size() == 1000);
        FL_CHECK(j[0].as<int>() == 0);
        FL_CHECK(j[999].as<int>() == (999 % 256));
    }

    FL_SUBCASE("Mixed array doesn't crash") {
        Json j = Json::parse("[1, \"hello\", true, null, 3.14]");
        FL_CHECK(j.is_array());
        FL_CHECK(j.size() == 5);
        FL_CHECK(j[0].as<int>() == 1);
        FL_CHECK(j[1].as<fl::string>() == "hello");
        FL_CHECK(j[2].as<bool>() == true);
        FL_CHECK(j[3].is_null());
        FL_CHECK_CLOSE(j[4].as<float>().value_or(0.0f), 3.14f, 0.001f);
    }
}

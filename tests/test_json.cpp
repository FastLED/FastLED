
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "platforms/stub/wasm/ui/json.h"

TEST_CASE("Test JsonIdValueDecoder") {
    SUBCASE("Test simple JSON parsing") {
        // R"(...)" is a raw string literal in C++, allowing us to write JSON strings without escaping quotes
        const char* json_str = R"({"0": "value"})";
        std::map<int, std::string> result;
        bool success = JsonIdValueDecoder::parseJson(json_str, &result);
        CHECK(success);
        REQUIRE(result.size() == 1);
        std::map<int, std::string>::iterator it = result.begin();
        CHECK_EQ(it->first, 0);
        CHECK_EQ(it->second, "value");
    }
    SUBCASE("More complex JSON parsing") {
        const char* json_str = R"({
            "0": "value"
        })";
        std::map<int, std::string> result;
        bool success = JsonIdValueDecoder::parseJson(json_str, &result);
        CHECK(success);
        REQUIRE(result.size() == 1);
        std::map<int, std::string>::iterator it = result.begin();
        CHECK_EQ(it->first, 0);
        CHECK_EQ(it->second, "value");
    }

    SUBCASE("Multiple values") {
        const char* json_str = R"({
            "0": "value",
            "1": "value1"
        })";
        std::map<int, std::string> result;
        bool success = JsonIdValueDecoder::parseJson(json_str, &result);
        CHECK(success);
        REQUIRE(result.size() == 2);
        std::map<int, std::string>::iterator it = result.begin();
        CHECK_EQ(it->first, 0);
        CHECK_EQ(it->second, "value");
        it++;
        CHECK_EQ(it->first, 1);
        CHECK_EQ(it->second, "value1");
    }

    SUBCASE("Trailing comma") {
        const char* json_str = R"({
            "0": "value",
            "1": "value1",
        })";
        std::map<int, std::string> result;
        bool success = JsonIdValueDecoder::parseJson(json_str, &result);
        CHECK(success);
        REQUIRE(result.size() == 2);
        std::map<int, std::string>::iterator it = result.begin();
        CHECK_EQ(it->first, 0);
        CHECK_EQ(it->second, "value");
        it++;
        CHECK_EQ(it->first, 1);
        CHECK_EQ(it->second, "value1");
    }
}

TEST_CASE("Test JsonStringValueDecoder") {
    SUBCASE("Test simple JSON parsing") {
        const char* json_str = R"({"key": "value"})";
        std::map<std::string, std::string> result;
        bool success = JsonStringValueDecoder::parseJson(json_str, &result);
        CHECK(success);
        REQUIRE(result.size() == 1);
        CHECK(result["key"] == "value");
    }

    SUBCASE("More complex JSON parsing") {
        const char* json_str = R"({
            "key1": "value1",
            "key2": "value2"
        })";
        std::map<std::string, std::string> result;
        bool success = JsonStringValueDecoder::parseJson(json_str, &result);
        CHECK(success);
        REQUIRE(result.size() == 2);
        CHECK(result["key1"] == "value1");
        CHECK(result["key2"] == "value2");
    }

    SUBCASE("Trailing comma") {
        const char* json_str = R"({
            "key1": "value1",
            "key2": "value2",
        })";
        std::map<std::string, std::string> result;
        bool success = JsonStringValueDecoder::parseJson(json_str, &result);
        CHECK(success);
        REQUIRE(result.size() == 2);
        CHECK(result["key1"] == "value1");
        CHECK(result["key2"] == "value2");
    }

    SUBCASE("Empty JSON") {
        const char* json_str = "{}";
        std::map<std::string, std::string> result;
        bool success = JsonStringValueDecoder::parseJson(json_str, &result);
        CHECK(success);
        CHECK(result.empty());
    }

    //SUBCASE("Invalid JSON") {
    //    const char* json_str = R"({"key": "value",})";
    //    std::map<std::string, std::string> result;
    //    bool success = JsonStringValueDecoder::parseJson(json_str, &result);
    //    CHECK_FALSE(success);
    //}
}

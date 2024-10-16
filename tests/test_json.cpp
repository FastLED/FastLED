
// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "lib8tion/intmap.h"
#include "platforms/stub/wasm/ui/json.h"

TEST_CASE("Test JsonIdValueDecoder") {
    SUBCASE("Test simple JSON parsing") {
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
}

#include "test.h"
#include "fl/json.h"
#include "fl/string.h"

using namespace fl;
using namespace fl::json2;

TEST_CASE("Json const char* assignment test") {
    SUBCASE("Simple const char* assignment") {
        const char* testStr = "Hello World";
        
        // Create a Json object and assign const char* to it
        fl::json2::Json json;
        json = testStr;
        
        // Check if it's correctly recognized as a string
        CHECK(json.is_string());
        CHECK_FALSE(json.is_bool());
        
        // Get the value back
        auto strOpt = json.as_string();
        REQUIRE(strOpt.has_value());
        CHECK_EQ(*strOpt, "Hello World");
    }
}
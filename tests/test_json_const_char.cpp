#include "test.h"
#include "fl/json.h"
#include "fl/string.h"
#include "platforms/shared/ui/json/title.h"

using namespace fl;
using namespace fl::json2;

TEST_CASE("Json const char* assignment test") {
    SUBCASE("Assign const char* to Json") {
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
    
    SUBCASE("Direct construction with const char*") {
        const char* testStr = "Direct Construction";
        fl::json2::Json json(testStr);
        
        // Check if it's correctly recognized as a string
        CHECK(json.is_string());
        CHECK_FALSE(json.is_bool());
        
        // Get the value back
        auto strOpt = json.as_string();
        REQUIRE(strOpt.has_value());
        CHECK_EQ(*strOpt, "Direct Construction");
    }
    
    SUBCASE("JsonTitleImpl with const char*") {
        const char* titleText = "Test Title";
        
        // This is where we'll test the JsonTitleImpl
        // We'll create one and check if it works correctly
        fl::JsonTitleImpl title(titleText);
        
        // Convert to Json and verify
        fl::json2::Json json;
        title.toJson(json);
        
        // Check if the text field is correctly stored
        CHECK(json.contains("text"));
        auto textValue = json["text"];
        CHECK(textValue.is_string());
        CHECK_FALSE(textValue.is_bool());
        
        auto strOpt = textValue.as_string();
        REQUIRE(strOpt.has_value());
        CHECK_EQ(*strOpt, "Test Title");
    }
}
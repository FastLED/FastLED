#include "test.h"
#include "fl/json.h"
#include "fl/string.h"
#include "platforms/shared/ui/json/title.h"

using namespace fl;

TEST_CASE("JsonTitleImpl const char* test") {
    SUBCASE("JsonTitleImpl with const char*") {
        const char* titleText = "Test Title";
        
        // This is where we'll test the JsonTitleImpl
        // We'll create one and check if it works correctly
        fl::JsonTitleImpl title(titleText);
        
        // Convert to Json and verify
        fl::Json json;
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

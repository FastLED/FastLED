#include "test.h"
#include "fl/json.h"
#include "fl/json_compact.h"
#include "fl/string.h"

#ifdef FASTLED_ENABLE_JSON

TEST_CASE("Json compact basics") {
    // Test with simple object
    fl::string input1 = "{ \"key\" : \"value\" }";
    fl::string expected1 = "{\"key\":\"value\"}";
    fl::string result1 = fl::compactJsonString(input1.c_str());
    CHECK_EQ(strcmp(result1.c_str(), expected1.c_str()), 0);
    
    // Test with array
    fl::string input2 = "[ 1 , 2 , 3 ]";
    fl::string expected2 = "[1,2,3]";
    fl::string result2 = fl::compactJsonString(input2.c_str());
    CHECK_EQ(strcmp(result2.c_str(), expected2.c_str()), 0);
    
    // Test with nested structure
    fl::string input3 = "{ \"array\" : [ 1 , 2 , 3 ] , \"obj\" : { \"nested\" : true } }";
    fl::string expected3 = "{\"array\":[1,2,3],\"obj\":{\"nested\":true}}";
    fl::string result3 = fl::compactJsonString(input3.c_str());
    CHECK_EQ(strcmp(result3.c_str(), expected3.c_str()), 0);
    
    // Test with various whitespace characters
    fl::string input4 = "{\n  \"key\"\t: \"value\"\r\n}";
    fl::string expected4 = "{\"key\":\"value\"}";
    fl::string result4 = fl::compactJsonString(input4.c_str());
    CHECK_EQ(strcmp(result4.c_str(), expected4.c_str()), 0);
    
    // Test with string containing whitespace (should be preserved)
    fl::string input5 = "{ \"message\" : \"hello world\" }";
    fl::string expected5 = "{\"message\":\"hello world\"}";
    fl::string result5 = fl::compactJsonString(input5.c_str());
    CHECK_EQ(strcmp(result5.c_str(), expected5.c_str()), 0);
}

TEST_CASE("Json parse with compact") {
    // Test parsing a JSON with extra whitespace
    fl::string jsonWithWhitespace = "{ \"name\" : \"FastLED\" , \"version\" : 5 }";
    fl::Json parsed = fl::Json::parse(jsonWithWhitespace.c_str());
    
    CHECK(parsed.has_value());
    CHECK(parsed.is_object());
    CHECK_EQ(strcmp(parsed["name"].getStringValue().c_str(), "FastLED"), 0);
    CHECK_EQ(parsed["version"].getIntValue(), 5);
    
    // Test parsing a compacted JSON
    fl::string compactJson = "{\"name\":\"FastLED\",\"version\":5}";
    fl::Json parsed2 = fl::Json::parse(compactJson.c_str());
    
    CHECK(parsed2.has_value());
    CHECK(parsed2.is_object());
    CHECK_EQ(strcmp(parsed2["name"].getStringValue().c_str(), "FastLED"), 0);
    CHECK_EQ(parsed2["version"].getIntValue(), 5);
    
    // Both should produce the same result
    CHECK_EQ(strcmp(parsed.serialize().c_str(), parsed2.serialize().c_str()), 0);
}

TEST_CASE("Json compact edge cases") {
    // Test with empty string
    fl::string empty = "";
    fl::string resultEmpty = fl::compactJsonString(empty.c_str());
    CHECK_EQ(strcmp(resultEmpty.c_str(), ""), 0);
    
    // Test with only whitespace
    fl::string whitespace = " \t\n\r ";
    fl::string resultWhitespace = fl::compactJsonString(whitespace.c_str());
    CHECK_EQ(strcmp(resultWhitespace.c_str(), ""), 0);
    
    // Test with no whitespace
    fl::string noWhitespace = "{\"key\":\"value\"}";
    fl::string resultNoWhitespace = fl::compactJsonString(noWhitespace.c_str());
    CHECK_EQ(strcmp(resultNoWhitespace.c_str(), noWhitespace.c_str()), 0);
    
    // Test with escaped quotes in strings
    fl::string escapedQuotes = "{ \"message\" : \"He said \\\"Hello\\\"\" }";
    fl::string expectedEscaped = "{\"message\":\"He said \\\"Hello\\\"\"}";
    fl::string resultEscaped = fl::compactJsonString(escapedQuotes.c_str());
    CHECK_EQ(strcmp(resultEscaped.c_str(), expectedEscaped.c_str()), 0);
}

#endif // FASTLED_ENABLE_JSON
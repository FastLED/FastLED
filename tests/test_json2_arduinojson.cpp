#include "test.h"
#include "fl/json.h"
#include "fl/math.h" // for fabs

TEST_CASE("FLArduinoJson Integration Tests") {
    SUBCASE("Integer Parsing") {
        // Test various integer representations
        fl::json2::Json int64Json = fl::json2::Json::parse("9223372036854775807"); // Max int64
        REQUIRE(int64Json.is_int());
        auto int64Value = int64Json.as_int();
        REQUIRE(int64Value.has_value());
        CHECK_EQ(*int64Value, 9223372036854775807LL);
        
        // Test negative integers
        fl::json2::Json negativeIntJson = fl::json2::Json::parse("-9223372036854775807");
        REQUIRE(negativeIntJson.is_int());
        auto negativeIntValue = negativeIntJson.as_int();
        REQUIRE(negativeIntValue.has_value());
        CHECK_EQ(*negativeIntValue, -9223372036854775807LL);
        
        // Test zero
        fl::json2::Json zeroJson = fl::json2::Json::parse("0");
        REQUIRE(zeroJson.is_int());
        auto zeroValue = zeroJson.as_int();
        REQUIRE(zeroValue.has_value());
        CHECK_EQ(*zeroValue, 0);
    }
    
    SUBCASE("Float Parsing") {
        // Test various float representations
        fl::json2::Json doubleJson = fl::json2::Json::parse("3.141592653589793");
        REQUIRE(doubleJson.is_double());
        auto doubleValue = doubleJson.as_double();
        REQUIRE(doubleValue.has_value());
        CHECK(*doubleValue == 3.141592653589793);
        
        // Test scientific notation
        fl::json2::Json scientificJson = fl::json2::Json::parse("1.23e-4");
        REQUIRE(scientificJson.is_double());
        auto scientificValue = scientificJson.as_double();
        REQUIRE(scientificValue.has_value());
        // Use approximate comparison for floating point values
        CHECK(fabs(*scientificValue - 0.000123) < 1e-10);
        
        // Test negative float
        fl::json2::Json negativeFloatJson = fl::json2::Json::parse("-2.5");
        REQUIRE(negativeFloatJson.is_double());
        auto negativeFloatValue = negativeFloatJson.as_double();
        REQUIRE(negativeFloatValue.has_value());
        CHECK(*negativeFloatValue == -2.5);
    }
    
    SUBCASE("String Parsing") {
        // Test string parsing
        fl::json2::Json stringJson = fl::json2::Json::parse("\"Hello World\"");
        REQUIRE(stringJson.is_string());
        auto stringValue = stringJson.as_string();
        REQUIRE(stringValue.has_value());
        CHECK(*stringValue == "Hello World");
        
        // Test string with escaped characters
        fl::json2::Json escaped = fl::json2::Json::parse("\"Hello\\nWorld\"");
        REQUIRE(escaped.is_string());
        auto escapedValue = escaped.as_string();
        REQUIRE(escapedValue.has_value());
        CHECK(*escapedValue == "Hello\nWorld");
    }
    
    SUBCASE("Boolean and Null Values") {
        // Test boolean values
        fl::json2::Json trueJson = fl::json2::Json::parse("true");
        REQUIRE(trueJson.is_bool());
        auto trueValue = trueJson.as_bool();
        REQUIRE(trueValue.has_value());
        CHECK(*trueValue == true);
        
        fl::json2::Json falseJson = fl::json2::Json::parse("false");
        REQUIRE(falseJson.is_bool());
        auto falseValue = falseJson.as_bool();
        REQUIRE(falseValue.has_value());
        CHECK(*falseValue == false);
        
        // Test null value
        fl::json2::Json nullJson = fl::json2::Json::parse("null");
        REQUIRE(nullJson.is_null());
    }
    
    SUBCASE("Array Parsing") {
        // Test array with mixed types
        fl::json2::Json arrayJson = fl::json2::Json::parse("[1, 2.5, \"string\", true, null]");
        REQUIRE(arrayJson.is_array());
        CHECK_EQ(arrayJson.size(), 5);
        
        // Check individual elements using as_* methods
        auto firstElement = arrayJson[0].as_int();
        REQUIRE(firstElement.has_value());
        CHECK_EQ(*firstElement, 1);
        
        auto secondElement = arrayJson[1].as_double();
        REQUIRE(secondElement.has_value());
        CHECK(*secondElement == 2.5);
        
        auto thirdElement = arrayJson[2].as_string();
        REQUIRE(thirdElement.has_value());
        CHECK(*thirdElement == "string");
        
        auto fourthElement = arrayJson[3].as_bool();
        REQUIRE(fourthElement.has_value());
        CHECK(*fourthElement == true);
        
        CHECK(arrayJson[4].is_null());
    }
    
    SUBCASE("Object Parsing") {
        // Test object with mixed types
        fl::json2::Json objJson = fl::json2::Json::parse("{\"int\": 42, \"float\": 3.14, \"string\": \"value\", \"bool\": false, \"null\": null}");
        REQUIRE(objJson.is_object());
        CHECK_EQ(objJson.size(), 5);
        
        // Check individual elements using as_* methods
        auto intElement = objJson["int"].as_int();
        REQUIRE(intElement.has_value());
        CHECK_EQ(*intElement, 42);
        
        auto floatElement = objJson["float"].as_double();
        REQUIRE(floatElement.has_value());
        CHECK(*floatElement == 3.14);
        
        auto stringElement = objJson["string"].as_string();
        REQUIRE(stringElement.has_value());
        CHECK(*stringElement == "value");
        
        auto boolElement = objJson["bool"].as_bool();
        REQUIRE(boolElement.has_value());
        CHECK(*boolElement == false);
        
        CHECK(objJson["null"].is_null());
    }
    
    SUBCASE("Error Handling") {
        // Test malformed JSON
        fl::json2::Json malformed = fl::json2::Json::parse("{ invalid json }");
        CHECK(malformed.is_null());
        
        // Test truncated JSON
        fl::json2::Json truncated = fl::json2::Json::parse("{\"incomplete\":");
        CHECK(truncated.is_null());
    }
}

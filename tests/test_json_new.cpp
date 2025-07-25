#include "test.h"
#include "fl/json_new.h"

TEST_CASE("Json parsing and serialization") {
    SUBCASE("Parse simple JSON values") {
        // Test null
        auto nullJson = fl::JsonValue::parse("null");
        REQUIRE(nullJson->is_null());
        
        // Test boolean
        auto trueJson = fl::JsonValue::parse("true");
        REQUIRE(trueJson->is_bool());
        bool trueResult = (*trueJson) | false;
        REQUIRE(trueResult);
        
        auto falseJson = fl::JsonValue::parse("false");
        REQUIRE(falseJson->is_bool());
        bool falseResult = (*falseJson) | true;
        REQUIRE(!falseResult);
        
        // Test numbers
        auto intJson = fl::JsonValue::parse("42");
        REQUIRE(intJson->is_int());
        int64_t intResult = (*intJson) | int64_t(0);
        REQUIRE(intResult == 42);
        
        auto doubleJson = fl::JsonValue::parse("3.14");
        REQUIRE(doubleJson->is_double());
        double doubleResult = (*doubleJson) | 0.0;
        REQUIRE(doubleResult == doctest::Approx(3.14));
        
        // Test string
        auto stringJson = fl::JsonValue::parse("\"hello\"");
        REQUIRE(stringJson->is_string());
        fl::string stringResult = (*stringJson) | fl::string("");
        REQUIRE(stringResult == "hello");
    }
    
    SUBCASE("Parse arrays") {
        auto arrayJson = fl::JsonValue::parse("[1, 2, 3]");
        REQUIRE(arrayJson->is_array());
        REQUIRE(arrayJson->as_array().has_value());
        REQUIRE(arrayJson->as_array()->size() == 3);
        int64_t firstElement = ((*arrayJson)[0] | int64_t(0));
        REQUIRE(firstElement == 1);
        int64_t secondElement = ((*arrayJson)[1] | int64_t(0));
        REQUIRE(secondElement == 2);
        int64_t thirdElement = ((*arrayJson)[2] | int64_t(0));
        REQUIRE(thirdElement == 3);
    }
    
    SUBCASE("Parse objects") {
        auto objectJson = fl::JsonValue::parse("{\"name\": \"FastLED\", \"version\": 5}");
        REQUIRE(objectJson->is_object());
        REQUIRE(objectJson->as_object().has_value());
        fl::string nameResult = ((*objectJson)["name"] | fl::string(""));
        REQUIRE(nameResult == "FastLED");
        int64_t versionResult = ((*objectJson)["version"] | int64_t(0));
        REQUIRE(versionResult == 5);
    }
    
    SUBCASE("Default values") {
        auto json = fl::JsonValue::parse("{\"existing\": 42}");
        
        // Existing value
        int64_t existingResult = ((*json)["existing"] | int64_t(0));
        REQUIRE(existingResult == 42);
        
        // Missing value with default
        int64_t missingResult = ((*json)["missing"] | int64_t(99));
        REQUIRE(missingResult == 99);
    }
}
#include "test.h"
#include "fl/json.h"

using namespace fl;

TEST_CASE("Json as_float Comprehensive Conversion") {
    SUBCASE("Boolean true to float") {
        Json json(true);
        CHECK(json.is_bool());
        CHECK_FALSE(json.is_double());
        
        fl::optional<float> value = json.as_float<float>();
        REQUIRE(value);
        CHECK_EQ(*value, 1.0f);
        
        fl::optional<double> dvalue = json.as_float<double>();
        REQUIRE(dvalue);
        CHECK_EQ(*dvalue, 1.0);
    }
    
    SUBCASE("Boolean false to float") {
        Json json(false);
        CHECK(json.is_bool());
        CHECK_FALSE(json.is_double());
        
        fl::optional<float> value = json.as_float<float>();
        REQUIRE(value);
        CHECK_EQ(*value, 0.0f);
        
        fl::optional<double> dvalue = json.as_float<double>();
        REQUIRE(dvalue);
        CHECK_EQ(*dvalue, 0.0);
    }
    
    SUBCASE("Integer to float") {
        Json json(42);
        CHECK(json.is_int());
        CHECK_FALSE(json.is_double());
        
        fl::optional<float> value = json.as_float<float>();
        REQUIRE(value);
        CHECK_EQ(*value, 42.0f);
        
        fl::optional<double> dvalue = json.as_float<double>();
        REQUIRE(dvalue);
        CHECK_EQ(*dvalue, 42.0);
    }
    
    SUBCASE("String to float (should fail)") {
        Json json("hello");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_bool());
        
        fl::optional<float> value = json.as_float<float>();
        CHECK_FALSE(value);
        
        fl::optional<double> dvalue = json.as_float<double>();
        CHECK_FALSE(dvalue);
    }
    
    SUBCASE("Double to float") {
        Json json(3.14);
        CHECK(json.is_double());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_bool());
        
        fl::optional<float> value = json.as_float<float>();
        CHECK(value);
        CHECK_EQ(*value, 3.14f);
        
        fl::optional<double> dvalue = json.as_float<double>();
        CHECK(dvalue);
        CHECK_EQ(*dvalue, 3.14);
    }
    
    SUBCASE("Null to float (should fail)") {
        Json json(nullptr);
        CHECK(json.is_null());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_bool());
        
        fl::optional<float> value = json.as_float<float>();
        CHECK_FALSE(value);
        
        fl::optional<double> dvalue = json.as_float<double>();
        CHECK_FALSE(dvalue);
    }
}
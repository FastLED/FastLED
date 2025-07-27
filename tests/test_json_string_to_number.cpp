#include "test.h"
#include "fl/json.h"

using namespace fl;

TEST_CASE("Json String to Number Conversion") {
    SUBCASE("String \"5\" to int and float") {
        Json json("5");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t
        fl::optional<int64_t> value64 = json.as_int();
        REQUIRE(value64);
        CHECK_EQ(*value64, 5);
        
        // Test conversion to int32_t
        fl::optional<int32_t> value32 = json.as_int<int32_t>();
        REQUIRE(value32);
        CHECK_EQ(*value32, 5);
        
        // Test conversion to int16_t
        fl::optional<int16_t> value16 = json.as_int<int16_t>();
        REQUIRE(value16);
        CHECK_EQ(*value16, 5);
        
        // Test conversion to double
        fl::optional<double> valued = json.as_float();
        REQUIRE(valued);
        // Use a simple comparison for floating-point values
        CHECK(*valued == 5.0);
        
        // Test conversion to float
        fl::optional<float> valuef = json.as_float<float>();
        REQUIRE(valuef);
        // Use a simple comparison for floating-point values
        CHECK(*valuef == 5.0f);
    }
    
    SUBCASE("String integer to int") {
        Json json("42");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t
        fl::optional<int64_t> value64 = json.as_int();
        REQUIRE(value64);
        CHECK_EQ(*value64, 42);
        
        // Test conversion to int32_t
        fl::optional<int32_t> value32 = json.as_int<int32_t>();
        REQUIRE(value32);
        CHECK_EQ(*value32, 42);
        
        // Test conversion to int16_t
        fl::optional<int16_t> value16 = json.as_int<int16_t>();
        REQUIRE(value16);
        CHECK_EQ(*value16, 42);
    }
    
    SUBCASE("String integer to float") {
        Json json("42");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        
        // Test conversion to double
        fl::optional<double> valued = json.as_float();
        REQUIRE(valued);
        // Use a simple comparison for floating-point values
        CHECK(*valued == 42.0);
        
        // Test conversion to float
        fl::optional<float> valuef = json.as_float<float>();
        REQUIRE(valuef);
        // Use a simple comparison for floating-point values
        CHECK(*valuef == 42.0f);
    }
    
    SUBCASE("String float to int") {
        Json json("5.7");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t (should fail - can't convert float string to int)
        fl::optional<int64_t> value64 = json.as_int();
        CHECK_FALSE(value64);
        
        // Test conversion to int32_t (should fail - can't convert float string to int)
        fl::optional<int32_t> value32 = json.as_int<int32_t>();
        CHECK_FALSE(value32);
    }
    
    SUBCASE("String float to float") {
        Json json("5.5");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        
        // Test conversion to double
        fl::optional<double> valued = json.as_float();
        REQUIRE(valued);
        // Use a simple comparison for floating-point values
        CHECK(*valued == 5.5);
        
        // Test conversion to float
        fl::optional<float> valuef = json.as_float<float>();
        REQUIRE(valuef);
        // Use a simple comparison for floating-point values
        CHECK(*valuef == 5.5f);
    }
    
    SUBCASE("Invalid string to number") {
        Json json("hello");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t (should fail)
        fl::optional<int64_t> value64 = json.as_int();
        CHECK_FALSE(value64);
        
        // Test conversion to double (should fail)
        fl::optional<double> valued = json.as_float();
        CHECK_FALSE(valued);
    }
    
    SUBCASE("Negative string number") {
        Json json("-5");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t
        fl::optional<int64_t> value64 = json.as_int();
        REQUIRE(value64);
        CHECK_EQ(*value64, -5);
        
        // Test conversion to double
        fl::optional<double> valued = json.as_float();
        REQUIRE(valued);
        // Use a simple comparison for floating-point values
        CHECK(*valued == -5.0);
    }
    
    SUBCASE("String with spaces") {
        Json json(" 5 ");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        
        // Test conversion to int64_t (should fail - spaces not allowed)
        fl::optional<int64_t> value64 = json.as_int();
        CHECK_FALSE(value64);
        
        // Test conversion to double (should fail - spaces not allowed)
        fl::optional<double> valued = json.as_float();
        CHECK_FALSE(valued);
    }
}
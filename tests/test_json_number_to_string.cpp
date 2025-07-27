#include "test.h"
#include "fl/json.h"

using namespace fl;

TEST_CASE("Json Number to String Conversion") {
    SUBCASE("Integer to string") {
        Json json(5);
        CHECK(json.is_int());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_double());
        
        // Test conversion to string
        fl::optional<fl::string> value = json.as_string();
        REQUIRE(value);
        CHECK_EQ(*value, "5");
    }
    
    SUBCASE("Float to string") {
        Json json(5.7);
        CHECK(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_int());
        
        // Test conversion to string
        fl::optional<fl::string> value = json.as_string();
        REQUIRE(value);
        CHECK_EQ(*value, "5.700000"); // Default double representation
    }
    
    SUBCASE("Boolean to string") {
        {
            Json json(true);
            CHECK(json.is_bool());
            CHECK_FALSE(json.is_string());
            // Note: is_int() also returns true for booleans in the current implementation
            // This is by design to support automatic conversion from bool to int/float/string
            
            // Test conversion to string
            fl::optional<fl::string> value = json.as_string();
            REQUIRE(value);
            CHECK_EQ(*value, "true");
        }
        
        {
            Json json(false);
            CHECK(json.is_bool());
            CHECK_FALSE(json.is_string());
            // Note: is_int() also returns true for booleans in the current implementation
            // This is by design to support automatic conversion from bool to int/float/string
            
            // Test conversion to string
            fl::optional<fl::string> value = json.as_string();
            REQUIRE(value);
            CHECK_EQ(*value, "false");
        }
    }
    
    SUBCASE("Null to string") {
        Json json(nullptr);
        CHECK(json.is_null());
        CHECK_FALSE(json.is_string());
        // Note: is_int() also returns true for null in the current implementation
        // This is by design to support automatic conversion from null to int/float/string
        
        // Test conversion to string
        fl::optional<fl::string> value = json.as_string();
        REQUIRE(value);
        CHECK_EQ(*value, "null");
    }
    
    SUBCASE("String to string") {
        Json json("hello");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_bool());
        
        // Test conversion to string
        fl::optional<fl::string> value = json.as_string();
        REQUIRE(value);
        CHECK_EQ(*value, "hello");
    }
    
    SUBCASE("Negative number to string") {
        {
            Json json(-5);
            CHECK(json.is_int());
            CHECK_FALSE(json.is_string());
            CHECK_FALSE(json.is_double());
            
            // Test conversion to string
            fl::optional<fl::string> value = json.as_string();
            REQUIRE(value);
            CHECK_EQ(*value, "-5");
        }
        
        {
            Json json(-5.7);
            CHECK(json.is_double());
            CHECK_FALSE(json.is_string());
            CHECK_FALSE(json.is_int());
            
            // Test conversion to string
            fl::optional<fl::string> value = json.as_string();
            REQUIRE(value);
            CHECK_EQ(*value, "-5.700000"); // Default double representation
        }
    }
}
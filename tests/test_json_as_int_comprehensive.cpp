#include "test.h"
#include "fl/json.h"

using namespace fl;

TEST_CASE("Json as_int Comprehensive Conversion") {
    SUBCASE("Boolean true to integer") {
        Json json(true);
        CHECK(json.is_bool());
        CHECK(json.is_int());
        
        fl::optional<int64_t> value = json.as_int();
        REQUIRE(value);
        CHECK_EQ(*value, 1);
    }
    
    SUBCASE("Boolean false to integer") {
        Json json(false);
        CHECK(json.is_bool());
        CHECK(json.is_int());
        
        fl::optional<int64_t> value = json.as_int();
        REQUIRE(value);
        CHECK_EQ(*value, 0);
    }
    
    SUBCASE("Integer to integer") {
        Json json(42);
        CHECK(json.is_int());
        CHECK_FALSE(json.is_bool());
        
        fl::optional<int64_t> value = json.as_int();
        REQUIRE(value);
        CHECK_EQ(*value, 42);
    }
    
    SUBCASE("String to integer (should fail)") {
        Json json("hello");
        CHECK(json.is_string());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_bool());
        
        fl::optional<int64_t> value = json.as_int();
        CHECK_FALSE(value);
    }
    
    SUBCASE("Double to integer (should succeed)") {
        // NEW INSTRUCTIONS: AUTO CONVERT FLOAT TO INT
        Json json(3.14);
        CHECK(json.is_double());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_bool());
        
        fl::optional<int64_t> value = json.as_int();
        CHECK(value);
        CHECK_EQ(*value, 3);
        
    }
    
    SUBCASE("Null to integer (should fail)") {
        Json json(nullptr);
        CHECK(json.is_null());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_bool());
        
        fl::optional<int64_t> value = json.as_int();
        CHECK_FALSE(value);
    }
}

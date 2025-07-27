#include "test.h"
#include "fl/json.h"

using namespace fl;

TEST_CASE("Json as_int Template Conversion") {
    SUBCASE("Double to various integer types") {
        Json json(3.14);
        CHECK(json.is_double());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_bool());
        
        // Test conversion to int64_t (default)
        fl::optional<int64_t> value64 = json.as_int();
        CHECK(value64);
        CHECK_EQ(*value64, 3);
        
        // Test conversion to int32_t
        fl::optional<int32_t> value32 = json.as_int<int32_t>();
        CHECK(value32);
        CHECK_EQ(*value32, 3);
        
        // Test conversion to int16_t
        fl::optional<int16_t> value16 = json.as_int<int16_t>();
        CHECK(value16);
        CHECK_EQ(*value16, 3);
        
        // Test conversion to int8_t
        fl::optional<int8_t> value8 = json.as_int<int8_t>();
        CHECK(value8);
        CHECK_EQ(*value8, 3);
        
        // Test conversion to uint64_t
        fl::optional<uint64_t> uvalue64 = json.as_int<uint64_t>();
        CHECK(uvalue64);
        CHECK_EQ(*uvalue64, 3);
        
        // Test conversion to uint32_t
        fl::optional<uint32_t> uvalue32 = json.as_int<uint32_t>();
        CHECK(uvalue32);
        CHECK_EQ(*uvalue32, 3);
        
        // Test conversion to uint16_t
        fl::optional<uint16_t> uvalue16 = json.as_int<uint16_t>();
        CHECK(uvalue16);
        CHECK_EQ(*uvalue16, 3);
        
        // Test conversion to uint8_t
        fl::optional<uint8_t> uvalue8 = json.as_int<uint8_t>();
        CHECK(uvalue8);
        CHECK_EQ(*uvalue8, 3);
    }
    
    SUBCASE("Negative double to signed integer types") {
        Json json(-3.14);
        CHECK(json.is_double());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_bool());
        
        // Test conversion to int64_t (default)
        fl::optional<int64_t> value64 = json.as_int();
        CHECK(value64);
        CHECK_EQ(*value64, -3);
        
        // Test conversion to int32_t
        fl::optional<int32_t> value32 = json.as_int<int32_t>();
        CHECK(value32);
        CHECK_EQ(*value32, -3);
        
        // Test conversion to int16_t
        fl::optional<int16_t> value16 = json.as_int<int16_t>();
        CHECK(value16);
        CHECK_EQ(*value16, -3);
        
        // Test conversion to int8_t
        fl::optional<int8_t> value8 = json.as_int<int8_t>();
        CHECK(value8);
        CHECK_EQ(*value8, -3);
    }
    
    SUBCASE("Large double to integer types") {
        Json json(123456.789);
        CHECK(json.is_double());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_bool());
        
        // Test conversion to int64_t (default)
        fl::optional<int64_t> value64 = json.as_int();
        CHECK(value64);
        CHECK_EQ(*value64, 123456);
        
        // Test conversion to int32_t
        fl::optional<int32_t> value32 = json.as_int<int32_t>();
        CHECK(value32);
        CHECK_EQ(*value32, 123456);
        
        // Test conversion to uint64_t
        fl::optional<uint64_t> uvalue64 = json.as_int<uint64_t>();
        CHECK(uvalue64);
        CHECK_EQ(*uvalue64, 123456);
    }
}
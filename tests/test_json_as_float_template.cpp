#include "test.h"
#include "fl/json.h"

using namespace fl;

TEST_CASE("Json as_float Template Conversion") {
    SUBCASE("Integer to various float types") {
        Json json(42);
        CHECK(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_bool());
        
        // Test conversion to double (default)
        fl::optional<double> valued = json.as_float();
        CHECK(valued);
        CHECK_EQ(*valued, 42.0);
        
        // Test conversion to float
        fl::optional<float> valuef = json.as_float<float>();
        CHECK(valuef);
        CHECK_EQ(*valuef, 42.0f);
    }
    
    SUBCASE("Boolean to various float types") {
        Json json(true);
        CHECK(json.is_bool());
        CHECK_FALSE(json.is_double());
        // Note: is_int() also returns true for booleans in the current implementation
        // This is by design to support automatic conversion from bool to int/float
        
        // Test conversion to double (default)
        fl::optional<double> valued = json.as_float();
        CHECK(valued);
        CHECK_EQ(*valued, 1.0);
        
        // Test conversion to float
        fl::optional<float> valuef = json.as_float<float>();
        CHECK(valuef);
        CHECK_EQ(*valuef, 1.0f);
    }
    
    SUBCASE("Negative integer to float types") {
        Json json(-42);
        CHECK(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_bool());
        
        // Test conversion to double (default)
        fl::optional<double> valued = json.as_float();
        CHECK(valued);
        CHECK_EQ(*valued, -42.0);
        
        // Test conversion to float
        fl::optional<float> valuef = json.as_float<float>();
        CHECK(valuef);
        CHECK_EQ(*valuef, -42.0f);
    }
    
    SUBCASE("Large integer to float types") {
        Json json(int64_t(123456789));
        CHECK(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_bool());
        
        // Test conversion to double (default)
        fl::optional<double> valued = json.as_float();
        CHECK(valued);
        CHECK_EQ(*valued, 123456789.0);
        
        // Test conversion to float
        fl::optional<float> valuef = json.as_float<float>();
        CHECK(valuef);
        CHECK_EQ(*valuef, 123456789.0f);
    }
}
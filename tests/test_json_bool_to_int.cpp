#include "test.h"
#include "fl/json.h"

using namespace fl;

TEST_CASE("Json Boolean to Integer Conversion") {
    SUBCASE("Create boolean and verify type checking") {
        // Create a JSON boolean value directly
        Json json(true);
        
        // Test that it's recognized as a boolean
        CHECK(json.is_bool());
        CHECK(json.is_int());

        fl::optional<int64_t> value = json.as_int();
        REQUIRE(value);
        CHECK_EQ(*value, 1);
    }
    
}

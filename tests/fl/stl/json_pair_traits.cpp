// Trait probe for #3588: how does fl::vector classify the json map pair?
#include "test.h"
#include "fl/stl/json.h"
#include "fl/stl/type_traits.h"

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("json_object pair classification in vector ops") {
    using Pair = fl::json_object::value_type;
    // Oracle checks: a FAILURE tells us the trait value.
    FL_CHECK_EQ((int)fl::is_trivially_copyable<Pair>::value, 0);
    FL_CHECK_EQ((int)fl::is_trivially_copyable<fl::string>::value, 0);
    FL_CHECK_EQ((int)fl::is_trivially_copyable<fl::shared_ptr<fl::json_value>>::value, 0);
    const auto* ops = fl::vector_element_ops_for<Pair>();
    FL_CHECK(ops != nullptr);
    if (ops) {
        FL_CHECK(ops->uninitialized_move_n != nullptr);
        FL_CHECK(ops->destroy_n != nullptr);
        FL_CHECK(ops->move_construct != nullptr);
    }
}

}

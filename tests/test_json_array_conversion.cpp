#include "test.h"
// Include our refactored JSON header
#include "fl/json_refactored.h"

namespace fl {

TEST_CASE("JsonArrayConversionTest") {
    // Test converting regular JsonArray
    JsonArray array;
    array.push_back(fl::make_shared<JsonValue>(1));
    array.push_back(fl::make_shared<JsonValue>(2));
    array.push_back(fl::make_shared<JsonValue>(3));

    ArrayConversionVisitor<JsonArray> arrayVisitor;
    arrayVisitor(array);
    CHECK_FALSE(arrayVisitor.has_error());
    CHECK(arrayVisitor.result.has_value());
    CHECK(arrayVisitor.result->size() == 3);

    // Test converting to integer (size of array)
    ArrayConversionVisitor<int> intVisitor;
    intVisitor(array);
    CHECK_FALSE(intVisitor.has_error());
    CHECK(intVisitor.result.has_value());
    CHECK(*intVisitor.result == 3);

    // Test converting to boolean (arrays are truthy)
    ArrayConversionVisitor<bool> boolVisitor;
    boolVisitor(array);
    CHECK_FALSE(boolVisitor.has_error());
    CHECK(boolVisitor.result.has_value());
    CHECK(*boolVisitor.result == true);

    // Test converting int16_t array
    fl::vector<int16_t> intArray = {1, 2, 3, 4, 5};

    ArrayConversionVisitor<JsonArray> arrayVisitor2;
    arrayVisitor2(intArray);
    CHECK_FALSE(arrayVisitor2.has_error());
    CHECK(arrayVisitor2.result.has_value());
    CHECK(arrayVisitor2.result->size() == 5);

    // Test converting uint8_t array
    fl::vector<uint8_t> uintArray = {10, 20, 30};

    ArrayConversionVisitor<JsonArray> uintVisitor;
    uintVisitor(uintArray);
    CHECK_FALSE(uintVisitor.has_error());
    CHECK(uintVisitor.result.has_value());
    CHECK(uintVisitor.result->size() == 3);

    // Test converting float array
    fl::vector<float> floatArray = {1.1f, 2.2f, 3.3f};

    ArrayConversionVisitor<JsonArray> floatVisitor;
    floatVisitor(floatArray);
    CHECK_FALSE(floatVisitor.has_error());
    CHECK(floatVisitor.result.has_value());
    CHECK(floatVisitor.result->size() == 3);

    // Test converting array to string (should fail)
    JsonArray array2;
    array2.push_back(fl::make_shared<JsonValue>(1));
    array2.push_back(fl::make_shared<JsonValue>(2));

    ArrayConversionVisitor<fl::string> stringVisitor;
    stringVisitor(array2);
    CHECK(stringVisitor.has_error());
    CHECK_FALSE(stringVisitor.result.has_value());
}

} // namespace fl
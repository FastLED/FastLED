#include "test.h"
#include "fl/json.h"
#include "fl/json2.h"




TEST_CASE("JSON roundtrip test fl::Json <-> fl::json2::Json") {
    const char* initialJson = "{\"map\":{\"strip1\":{\"x\":[0,1,2,3],\"y\":[0,1,2,3]}}}";

    // 1. Deserialize with fl::Json
    fl::Json json = fl::Json::parse(initialJson);
    CHECK(json.has_value());

    // 2. Serialize with fl::Json
    fl::string json_string = json.serialize();

    // 3. Deserialize with json2
    fl::json2::Json json2_obj = fl::json2::Json::parse(json_string);
    CHECK(json2_obj.has_value());

    // 4. Serialize with json2
    fl::string json2_string = json2_obj.to_string();

    // 5. Compare the results
    CHECK_EQ(fl::string(initialJson), json2_string);
}

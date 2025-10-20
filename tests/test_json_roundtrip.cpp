#include "test.h"
#include "fl/json.h"


TEST_CASE("JSON Round Trip Serialization Test") {
    // Create the initial JSON string
    const char* initialJson = "{\"name\":\"bob\",\"value\":21}";
    
    // 1. Parse the JSON string into a Json document
    fl::Json parsedJson = fl::Json::parse(initialJson);
    
    // Verify parsing was successful
    CHECK(parsedJson.has_value());
    CHECK(parsedJson.is_object());
    CHECK(parsedJson.contains("name"));
    CHECK(parsedJson.contains("value"));
    
    // Check values
    CHECK_EQ(parsedJson["name"].as_or(fl::string("")), "bob");
    CHECK_EQ(parsedJson["value"].as_or(0), 21);
    
    // 2. Serialize it back to a string
    fl::string serializedJson = parsedJson.to_string();
    
    // 3. Confirm that the string is the same (after normalization)
    // Since JSON serialization might reorder keys or add spaces, we'll normalize both strings
    fl::string normalizedInitial = fl::Json::normalizeJsonString(initialJson);
    fl::string normalizedSerialized = fl::Json::normalizeJsonString(serializedJson.c_str());
    
    CHECK_EQ(normalizedInitial, normalizedSerialized);
    
    // Also test with a more structured approach - parse the serialized string again
    fl::Json reparsedJson = fl::Json::parse(serializedJson);
    CHECK(reparsedJson.has_value());
    CHECK(reparsedJson.is_object());
    CHECK(reparsedJson.contains("name"));
    CHECK(reparsedJson.contains("value"));
    CHECK_EQ(reparsedJson["name"].as_or(fl::string("")), "bob");
    CHECK_EQ(reparsedJson["value"].as_or(0), 21);
}
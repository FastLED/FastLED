#include "test.h"

#include "fl/json.h"
#include "fl/namespace.h"
#include "fl/dbg.h"

FASTLED_USING_NAMESPACE

#if FASTLED_ENABLE_JSON

TEST_CASE("JSON serialization basic test") {
    // Test string provided by user: {"2": "3"}
    const char* jsonStr = R"({"2": "3"})";
    
    fl::JsonDocument testDoc;
    fl::string error;
    
    // Parse the JSON string into a document
    bool parseResult = fl::parseJson(jsonStr, &testDoc, &error);
    
    // Verify parsing was successful
    CHECK(parseResult);
    CHECK(error.empty());
    
    // Test that the document is a JsonObject as requested
    CHECK(testDoc.is<FLArduinoJson::JsonObject>());
    
    // Additional verification of the parsed content
    auto obj = testDoc.as<FLArduinoJson::JsonObjectConst>();
    CHECK(fl::string(obj["2"].as<const char*>()) == fl::string("3"));
}

TEST_CASE("JSON serialization round-trip test") {
    // Test creating a JSON object programmatically and serializing it
    fl::JsonDocument doc;
    auto jsonObj = doc.to<FLArduinoJson::JsonObject>();
    jsonObj["2"] = "3";
    
    // Verify it's recognized as a JsonObject
    CHECK(doc.is<FLArduinoJson::JsonObject>());
    
    // Test serialization back to string
    fl::string jsonBuffer;
    fl::toJson(doc, &jsonBuffer);
    
    CHECK(!jsonBuffer.empty());
    CHECK(jsonBuffer.find('2') >= 0);  // Contains key "2"
    CHECK(jsonBuffer.find('3') >= 0);  // Contains value "3"
    
    // Test round-trip: parse the serialized string
    fl::JsonDocument roundTripDoc;
    fl::string roundTripError;
    bool roundTripResult = fl::parseJson(jsonBuffer.c_str(), &roundTripDoc, &roundTripError);
    
    CHECK(roundTripResult);
    CHECK(roundTripError.empty());
    CHECK(roundTripDoc.is<FLArduinoJson::JsonObject>());
    
    // Verify content is preserved
    auto roundTripObj = roundTripDoc.as<FLArduinoJson::JsonObjectConst>();
    CHECK(fl::string(roundTripObj["2"].as<const char*>()) == fl::string("3"));
}

TEST_CASE("JSON serialization type verification tests") {
    // Test various JSON structures to ensure proper type detection
    
    // Test object type
    const char* objectJson = R"({"key": "value"})";
    fl::JsonDocument objectDoc;
    fl::string error;
    
    bool objectResult = fl::parseJson(objectJson, &objectDoc, &error);
    CHECK(objectResult);
    CHECK(objectDoc.is<FLArduinoJson::JsonObject>());
    CHECK_FALSE(objectDoc.is<FLArduinoJson::JsonArray>());
    
    // Test array type
    const char* arrayJson = R"([1, 2, 3])";
    fl::JsonDocument arrayDoc;
    
    bool arrayResult = fl::parseJson(arrayJson, &arrayDoc, &error);
    CHECK(arrayResult);
    CHECK(arrayDoc.is<FLArduinoJson::JsonArray>());
    CHECK_FALSE(arrayDoc.is<FLArduinoJson::JsonObject>());
    
    // Test primitive types
    const char* stringJson = R"("hello")";
    fl::JsonDocument stringDoc;
    
    bool stringResult = fl::parseJson(stringJson, &stringDoc, &error);
    CHECK(stringResult);
    CHECK_FALSE(stringDoc.is<FLArduinoJson::JsonObject>());
    CHECK_FALSE(stringDoc.is<FLArduinoJson::JsonArray>());
}

TEST_CASE("JSON serialization error handling") {
    // Test parsing invalid JSON
    const char* invalidJson = R"({"2": "3")";  // Missing closing brace
    
    fl::JsonDocument testDoc;
    fl::string error;
    
    bool parseResult = fl::parseJson(invalidJson, &testDoc, &error);
    
    // Should fail to parse
    CHECK_FALSE(parseResult);
    CHECK(!error.empty());
}

TEST_CASE("JSON type detection and printing") {
    // Test with the user's JSON string {"2": "3"}
    const char* jsonStr = R"({"2": "3"})";
    
    fl::JsonDocument testDoc;
    fl::string error;
    
    bool parseResult = fl::parseJson(jsonStr, &testDoc, &error);
    CHECK(parseResult);
    CHECK(error.empty());
    
    // Print the type of the root document
    FL_WARN("Root document type: ");
    if (testDoc.is<FLArduinoJson::JsonObject>()) {
        FL_WARN("object");
        CHECK(true); // Verify we detected object correctly
    } else if (testDoc.is<FLArduinoJson::JsonArray>()) {
        FL_WARN("array");
        CHECK(false); // Should not be array
    } else {
        FL_WARN("other type or null");
        CHECK(false); // Should be object
    }
    
    // Test individual value types within the object
    auto obj = testDoc.as<FLArduinoJson::JsonObjectConst>();
    auto valueVariant = obj["2"];
    
    FL_WARN("Value '2' type: ");
    if (valueVariant.is<FLArduinoJson::JsonObjectConst>()) {
        FL_WARN("object");
        CHECK(false); // Should not be object
    } else if (valueVariant.is<FLArduinoJson::JsonArrayConst>()) {
        FL_WARN("array");
        CHECK(false); // Should not be array
    } else if (valueVariant.is<int>()) {
        FL_WARN("integer");
        CHECK(false); // Should not be integer (it's a string "3")
    } else if (valueVariant.is<float>()) {
        FL_WARN("float");
        CHECK(false); // Should not be float
    } else if (valueVariant.is<bool>()) {
        FL_WARN("boolean");
        CHECK(false); // Should not be boolean
    } else if (valueVariant.is<const char*>()) {
        FL_WARN("string");
        CHECK(true); // Should be string "3"
    } else if (valueVariant.isNull()) {
        FL_WARN("null");
        CHECK(false); // Should not be null
    } else {
        FL_WARN("undefined type");
        CHECK(false); // Should not be undefined
    }
}

TEST_CASE("JSON type detection comprehensive") {
    // Test multiple JSON types in one document
    const char* complexJson = R"({
        "str": "hello",
        "num": 42,
        "float": 3.14,
        "bool": true,
        "null_val": null,
        "array": [1, 2, 3],
        "object": {"nested": "value"}
    })";
    
    fl::JsonDocument testDoc;
    fl::string error;
    
    bool parseResult = fl::parseJson(complexJson, &testDoc, &error);
    CHECK(parseResult);
    CHECK(error.empty());
    
    auto obj = testDoc.as<FLArduinoJson::JsonObjectConst>();
    
    // Test string type
    auto strVal = obj["str"];
    CHECK(strVal.is<const char*>());
    FL_WARN("str field type: string");
    
    // Test integer type  
    auto numVal = obj["num"];
    CHECK(numVal.is<int>());
    FL_WARN("num field type: integer");
    
    // Test float type
    auto floatVal = obj["float"];
    CHECK(floatVal.is<float>());
    FL_WARN("float field type: float");
    
    // Test boolean type
    auto boolVal = obj["bool"];
    CHECK(boolVal.is<bool>());
    FL_WARN("bool field type: boolean");
    
    // Test null type
    auto nullVal = obj["null_val"];
    CHECK(nullVal.isNull());
    FL_WARN("null_val field type: null");
    
    // Test array type
    auto arrayVal = obj["array"];
    CHECK(arrayVal.is<FLArduinoJson::JsonArrayConst>());
    FL_WARN("array field type: array");
    
    // Test nested object type
    auto objectVal = obj["object"];
    CHECK(objectVal.is<FLArduinoJson::JsonObjectConst>());
    FL_WARN("object field type: object");
}

#else

TEST_CASE("JSON serialization disabled") {
    // When JSON is disabled, we should still be able to compile
    // but functionality will be limited
    CHECK(true);
}

#endif // FASTLED_ENABLE_JSON

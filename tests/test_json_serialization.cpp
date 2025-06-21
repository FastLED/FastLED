#include "test.h"

#include "fl/json.h"
#include "fl/namespace.h"

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

#else

TEST_CASE("JSON serialization disabled") {
    // When JSON is disabled, we should still be able to compile
    // but functionality will be limited
    CHECK(true);
}

#endif // FASTLED_ENABLE_JSON

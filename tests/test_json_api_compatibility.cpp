#include "test.h"
#include "fl/json.h"
#include "fl/namespace.h"
#include "fl/dbg.h"

FASTLED_USING_NAMESPACE

#if FASTLED_ENABLE_JSON

TEST_CASE("JSON API Compatibility - Object Parsing") {
    const char* jsonStr = R"({"name": "test", "value": 42, "active": true, "temp": 25.5})";
    
    // Test legacy parseJson API
    fl::JsonDocument legacyDoc;
    fl::string legacyError;
    bool legacyResult = fl::parseJson(jsonStr, &legacyDoc, &legacyError);
    
    // Test new fl::Json API
    fl::Json newJson = fl::Json::parse(jsonStr);
    
    // Both should succeed
    CHECK(legacyResult);
    CHECK(legacyError.empty());
    CHECK(newJson.has_value());
    CHECK(newJson.is_object());
    
    // Legacy API access
    auto legacyObj = legacyDoc.as<FLArduinoJson::JsonObjectConst>();
    CHECK(fl::string(legacyObj["name"].as<const char*>()) == "test");
    CHECK(legacyObj["value"].as<int>() == 42);
    CHECK(legacyObj["active"].as<bool>() == true);
    CHECK_CLOSE(legacyObj["temp"].as<float>(), 25.5f, 0.001f);
    
    // New API access (would need implementation of value getters)
    // These are placeholders - actual implementation needed in JsonImpl
    // CHECK((newJson["name"] | fl::string("")) == "test");
    // CHECK((newJson["value"] | 0) == 42);
    // CHECK((newJson["active"] | false) == true);
    
    // Test serialization compatibility
    fl::string legacySerialized;
    fl::toJson(legacyDoc, &legacySerialized);
    fl::string newSerialized = newJson.serialize();
    
    // Parse both serialized outputs to compare structure
    fl::JsonDocument legacyReParsed;
    fl::JsonDocument newReParsed;
    fl::string reParseError;
    
    bool legacyReParseResult = fl::parseJson(legacySerialized.c_str(), &legacyReParsed, &reParseError);
    bool newReParseResult = fl::parseJson(newSerialized.c_str(), &newReParsed, &reParseError);
    
    CHECK(legacyReParseResult);
    CHECK(newReParseResult);
    
    // Verify both contain same data
    auto legacyReObj = legacyReParsed.as<FLArduinoJson::JsonObjectConst>();
    auto newReObj = newReParsed.as<FLArduinoJson::JsonObjectConst>();
    
    CHECK(fl::string(legacyReObj["name"].as<const char*>()) == fl::string(newReObj["name"].as<const char*>()));
    CHECK(legacyReObj["value"].as<int>() == newReObj["value"].as<int>());
    CHECK(legacyReObj["active"].as<bool>() == newReObj["active"].as<bool>());
}

TEST_CASE("JSON API Compatibility - Array Parsing") {
    const char* jsonStr = R"([{"id": 1, "name": "first"}, {"id": 2, "name": "second"}])";
    
    // Test legacy parseJson API
    fl::JsonDocument legacyDoc;
    fl::string legacyError;
    bool legacyResult = fl::parseJson(jsonStr, &legacyDoc, &legacyError);
    
    // Test new fl::Json API
    fl::Json newJson = fl::Json::parse(jsonStr);
    
    // Both should succeed
    CHECK(legacyResult);
    CHECK(legacyError.empty());
    CHECK(newJson.has_value());
    CHECK(newJson.is_array());
    
    // Legacy API access
    auto legacyArray = legacyDoc.as<FLArduinoJson::JsonArrayConst>();
    CHECK(legacyArray.size() == 2);
    CHECK(legacyArray[0]["id"].as<int>() == 1);
    CHECK(fl::string(legacyArray[0]["name"].as<const char*>()) == "first");
    CHECK(legacyArray[1]["id"].as<int>() == 2);
    CHECK(fl::string(legacyArray[1]["name"].as<const char*>()) == "second");
    
    // New API access
    CHECK(newJson.getSize() == 2);
    CHECK(newJson[0].is_object());
    CHECK(newJson[1].is_object());
    
    // Test serialization compatibility
    fl::string legacySerialized;
    fl::toJson(legacyDoc, &legacySerialized);
    fl::string newSerialized = newJson.serialize();
    
    // Parse both serialized outputs to compare structure
    fl::JsonDocument legacyReParsed;
    fl::JsonDocument newReParsed;
    fl::string reParseError;
    
    bool legacyReParseResult = fl::parseJson(legacySerialized.c_str(), &legacyReParsed, &reParseError);
    bool newReParseResult = fl::parseJson(newSerialized.c_str(), &newReParsed, &reParseError);
    
    CHECK(legacyReParseResult);
    CHECK(newReParseResult);
    
    // Verify both arrays have same structure
    auto legacyReArray = legacyReParsed.as<FLArduinoJson::JsonArrayConst>();
    auto newReArray = newReParsed.as<FLArduinoJson::JsonArrayConst>();
    
    CHECK(legacyReArray.size() == newReArray.size());
    CHECK(legacyReArray[0]["id"].as<int>() == newReArray[0]["id"].as<int>());
    CHECK(fl::string(legacyReArray[0]["name"].as<const char*>()) == fl::string(newReArray[0]["name"].as<const char*>()));
}

TEST_CASE("JSON API Compatibility - Type Detection") {
    const char* jsonStr = R"({
        "string_val": "hello",
        "int_val": 42,
        "float_val": 3.14,
        "bool_val": true,
        "null_val": null,
        "array_val": [1, 2, 3],
        "object_val": {"key": "value"}
    })";
    
    // Test legacy parseJson API
    fl::JsonDocument legacyDoc;
    fl::string legacyError;
    bool legacyResult = fl::parseJson(jsonStr, &legacyDoc, &legacyError);
    
    // Test new fl::Json API
    fl::Json newJson = fl::Json::parse(jsonStr);
    
    CHECK(legacyResult);
    CHECK(newJson.has_value());
    
    // Test type detection compatibility
    auto legacyObj = legacyDoc.as<FLArduinoJson::JsonObjectConst>();
    
    // String type
    CHECK(fl::getJsonType(legacyObj["string_val"]) == fl::JSON_STRING);
    CHECK(newJson["string_val"].getStringValue() != ""); // Basic check that it has a value
    
    // Integer type
    CHECK(fl::getJsonType(legacyObj["int_val"]) == fl::JSON_INTEGER);
    CHECK(newJson["int_val"].getIntValue() != 0);
    
    // Float type
    CHECK(fl::getJsonType(legacyObj["float_val"]) == fl::JSON_FLOAT);
    CHECK(newJson["float_val"].getFloatValue() != 0.0f);
    
    // Boolean type  
    CHECK(fl::getJsonType(legacyObj["bool_val"]) == fl::JSON_BOOLEAN);
    CHECK(newJson["bool_val"].getBoolValue() == true);
    
    // Null type
    CHECK(fl::getJsonType(legacyObj["null_val"]) == fl::JSON_NULL);
    CHECK(newJson["null_val"].isNull());
    
    // Array type
    CHECK(fl::getJsonType(legacyObj["array_val"]) == fl::JSON_ARRAY);
    CHECK(newJson["array_val"].is_array());
    
    // Object type
    CHECK(fl::getJsonType(legacyObj["object_val"]) == fl::JSON_OBJECT);
    CHECK(newJson["object_val"].is_object());
}

TEST_CASE("JSON API Compatibility - Error Handling") {
    const char* invalidJson = R"({"invalid": json syntax})";
    
    // Test legacy parseJson API error handling
    fl::JsonDocument legacyDoc;
    fl::string legacyError;
    bool legacyResult = fl::parseJson(invalidJson, &legacyDoc, &legacyError);
    
    // Test new fl::Json API error handling
    fl::Json newJson = fl::Json::parse(invalidJson);
    
    // Both should fail gracefully
    CHECK_FALSE(legacyResult);
    CHECK(!legacyError.empty());
    CHECK_FALSE(newJson.has_value());
}

TEST_CASE("JSON API Compatibility - Empty and Simple Values") {
    SUBCASE("Empty object") {
        const char* emptyObj = "{}";
        
        fl::JsonDocument legacyDoc;
        fl::string error;
        bool legacyResult = fl::parseJson(emptyObj, &legacyDoc, &error);
        fl::Json newJson = fl::Json::parse(emptyObj);
        
        CHECK(legacyResult);
        CHECK(newJson.has_value());
        CHECK(newJson.is_object());
        
        // Both should serialize to equivalent empty objects
        fl::string legacySerialized;
        fl::toJson(legacyDoc, &legacySerialized);
        fl::string newSerialized = newJson.serialize();
        
        // Both should be valid JSON objects (may have different formatting)
        CHECK(legacySerialized.find('{') != fl::string::npos);
        CHECK(legacySerialized.find('}') != fl::string::npos);
        CHECK(newSerialized.find('{') != fl::string::npos);
        CHECK(newSerialized.find('}') != fl::string::npos);
    }
    
    SUBCASE("Empty array") {
        const char* emptyArr = "[]";
        
        fl::JsonDocument legacyDoc;
        fl::string error;
        bool legacyResult = fl::parseJson(emptyArr, &legacyDoc, &error);
        fl::Json newJson = fl::Json::parse(emptyArr);
        
        CHECK(legacyResult);
        CHECK(newJson.has_value());
        CHECK(newJson.is_array());
        
        // Both should serialize to equivalent empty arrays
        fl::string legacySerialized;
        fl::toJson(legacyDoc, &legacySerialized);
        fl::string newSerialized = newJson.serialize();
        
        CHECK(legacySerialized.find('[') != fl::string::npos);
        CHECK(legacySerialized.find(']') != fl::string::npos);
        CHECK(newSerialized.find('[') != fl::string::npos);
        CHECK(newSerialized.find(']') != fl::string::npos);
    }
}

TEST_CASE("JSON API Compatibility - Nested Structures") {
    const char* nestedJson = R"({
        "config": {
            "settings": {
                "brightness": 128,
                "enabled": true
            },
            "modes": ["auto", "manual", "off"]
        },
        "data": [
            {"timestamp": 1000, "values": [1, 2, 3]},
            {"timestamp": 2000, "values": [4, 5, 6]}
        ]
    })";
    
    // Test legacy parseJson API
    fl::JsonDocument legacyDoc;
    fl::string legacyError;
    bool legacyResult = fl::parseJson(nestedJson, &legacyDoc, &legacyError);
    
    // Test new fl::Json API
    fl::Json newJson = fl::Json::parse(nestedJson);
    
    CHECK(legacyResult);
    CHECK(newJson.has_value());
    CHECK(newJson.is_object());
    
    // Test nested access - legacy API
    auto legacyObj = legacyDoc.as<FLArduinoJson::JsonObjectConst>();
    CHECK(legacyObj["config"]["settings"]["brightness"].as<int>() == 128);
    CHECK(legacyObj["config"]["settings"]["enabled"].as<bool>() == true);
    CHECK(legacyObj["config"]["modes"][0].as<const char*>() == fl::string("auto"));
    CHECK(legacyObj["data"][0]["timestamp"].as<int>() == 1000);
    
    // Test nested access - new API
    CHECK(newJson["config"].is_object());
    CHECK(newJson["config"]["settings"].is_object());
    CHECK(newJson["config"]["modes"].is_array());
    CHECK(newJson["data"].is_array());
    CHECK(newJson["data"][0].is_object());
    
    // Test serialization preserves nested structure
    fl::string legacySerialized;
    fl::toJson(legacyDoc, &legacySerialized);
    fl::string newSerialized = newJson.serialize();
    
    // Both should contain key structural elements
    CHECK(legacySerialized.find("config") != fl::string::npos);
    CHECK(legacySerialized.find("settings") != fl::string::npos);
    CHECK(legacySerialized.find("brightness") != fl::string::npos);
    CHECK(newSerialized.find("config") != fl::string::npos);
    CHECK(newSerialized.find("settings") != fl::string::npos);
    CHECK(newSerialized.find("brightness") != fl::string::npos);
}

#else

TEST_CASE("JSON API Compatibility - Disabled") {
    // When JSON is disabled, both APIs should be no-ops
    fl::JsonDocument legacyDoc;
    fl::string error;
    bool legacyResult = fl::parseJson("{}", &legacyDoc, &error);
    
    fl::Json newJson = fl::Json::parse("{}");
    
    CHECK_FALSE(legacyResult);
    CHECK_FALSE(newJson.has_value());
}

#endif // FASTLED_ENABLE_JSON 

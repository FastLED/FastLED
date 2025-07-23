#include "test.h"
#include "fl/json.h"
#include "fl/namespace.h"

FASTLED_USING_NAMESPACE


TEST_CASE("JSON Legacy vs New API - ActiveStripData Format") {
    // Test data - simulate what ActiveStripData would have
    struct StripInfo {
        int stripIndex;
        const char* type;
    };
    
    StripInfo testData[] = {
        {0, "r8g8b8"},
        {2, "r8g8b8"},
        {5, "r8g8b8"}
    };
    
    SUBCASE("Legacy ArduinoJSON API - WORKING") {
        FLArduinoJson::JsonDocument doc;
        auto array = doc.to<FLArduinoJson::JsonArray>();

        for (const auto& strip : testData) {
            auto obj = array.add<FLArduinoJson::JsonObject>();
            obj["strip_id"] = strip.stripIndex;
            obj["type"] = strip.type;
        }

        fl::Str legacyJsonBuffer;
        serializeJson(doc, legacyJsonBuffer);
        
        // Parse back to verify it's valid
        fl::JsonDocument verifyDoc;
        fl::string error;
        bool parseOk = fl::parseJson(legacyJsonBuffer.c_str(), &verifyDoc, &error);
        CHECK(parseOk);
        CHECK(error.empty());
        
        FL_WARN("Legacy JSON output: " << legacyJsonBuffer.c_str());
        
        // Verify legacy output has expected structure
        CHECK(legacyJsonBuffer.find("strip_id") != fl::string::npos);
        CHECK(legacyJsonBuffer.find("r8g8b8") != fl::string::npos);
        CHECK(legacyJsonBuffer[0] == '[');  // Should start with array
        
        // Store the expected JSON for comparison when new API is fixed
        fl::string expectedJson = legacyJsonBuffer.c_str();
        
        SUBCASE("New fl::Json API - PARSING WORKS, CREATION INCOMPLETE") {
            FL_WARN("NOTE: Testing what works in the new fl::Json API...");
            
            // Test parsing existing JSON (this DOES work)
            auto parsedJson = fl::Json::parse(expectedJson.c_str());
            CHECK(parsedJson.is_array());
            CHECK_EQ(parsedJson.getSize(), 3);
            
            // Verify parsed structure matches legacy output
            for (size_t i = 0; i < parsedJson.getSize(); ++i) {
                auto item = parsedJson[static_cast<int>(i)];
                CHECK(item.is_object());
                
                // Verify the data matches our test data
                int stripId = item["strip_id"] | -1;
                fl::string type = item["type"] | fl::string("");
                
                CHECK_EQ(stripId, testData[i].stripIndex);
                CHECK_EQ(type, testData[i].type);
            }
            
            FL_WARN("SUCCESS: New fl::Json API can parse and access JSON data correctly");
            FL_WARN("SUCCESS: Both legacy and new APIs can read the same JSON data equivalently");
            
            // Document what doesn't work yet
            FL_WARN("TODO: Factory methods (createArray, createObject) return incorrect types");
            FL_WARN("TODO: Serialization methods (set, push_back, serialize) are unimplemented");
            
            // TODO: Once the new API is fully implemented, this should work:
            // auto newJson = fl::Json::createArray();
            // for (const auto& strip : testData) {
            //     auto newObj = fl::Json::createObject();
            //     newObj.set("strip_id", strip.stripIndex);
            //     newObj.set("type", strip.type);
            //     newJson.push_back(newObj);
            // }
            // fl::string newJsonOutput = newJson.serialize();
            // CHECK_EQ(newJsonOutput, expectedJson);
        }
    }
}

TEST_CASE("JSON Legacy vs New API - Empty Array") {
    SUBCASE("Legacy ArduinoJSON empty array - WORKING") {
        FLArduinoJson::JsonDocument doc;
        auto array = doc.to<FLArduinoJson::JsonArray>();
        // Don't add anything - empty array
        
        fl::Str legacyOutput;
        serializeJson(doc, legacyOutput);
        FL_WARN("Legacy empty array: " << legacyOutput.c_str());
        
        // Verify legacy empty array works
        CHECK_EQ(legacyOutput, "[]");
        
        SUBCASE("New fl::Json empty array - PARSING WORKS") {
            // Test parsing empty array from legacy output
            auto parsedEmpty = fl::Json::parse(legacyOutput.c_str());
            CHECK(parsedEmpty.is_array());
            CHECK_EQ(parsedEmpty.getSize(), 0);
            
            FL_WARN("SUCCESS: New fl::Json can parse and inspect empty arrays correctly");
            FL_WARN("TODO: Factory method createArray() needs implementation fixes");
            
            // TODO: Once createArray() and serialize() are implemented, this should work:
            // auto json = fl::Json::createArray();
            // CHECK(json.is_array());
            // CHECK_EQ(json.getSize(), 0);
            // fl::string newOutput = json.serialize();
            // CHECK_EQ(newOutput, "[]");
            // CHECK_EQ(legacyOutput, newOutput);
        }
    }
}

TEST_CASE("JSON UI Update Parsing - Real World Usage") {
    // Simulate real UI input JSON that comes from JavaScript
    const char* uiUpdateJson = R"({
        "slider_brightness": 128,
        "button_reset": true,
        "color_picker": "#FF5500",
        "speed_control": 75
    })";
    
    SUBCASE("Legacy ArduinoJSON parsing - WORKING") {
        FLArduinoJson::JsonDocument doc;
        auto result = FLArduinoJson::deserializeJson(doc, uiUpdateJson);
        CHECK(result == FLArduinoJson::DeserializationError::Ok);
        
        auto obj = doc.as<FLArduinoJson::JsonObjectConst>();
        
        // Test accessing values
        CHECK_EQ(obj["slider_brightness"].as<int>(), 128);
        CHECK_EQ(obj["button_reset"].as<bool>(), true);
        CHECK_EQ(fl::string(obj["color_picker"].as<const char*>()), fl::string("#FF5500"));
        CHECK_EQ(obj["speed_control"].as<int>(), 75);
        
        FL_WARN("Legacy UI JSON parsing: SUCCESS");
    }
    
    SUBCASE("New fl::Json parsing - WORKING") {
        auto json = fl::Json::parse(uiUpdateJson);
        CHECK(json.has_value());
        CHECK(json.is_object());
        
        // Test accessing values with new API
        int brightness = json["slider_brightness"] | 0;
        bool reset = json["button_reset"] | false;
        fl::string color = json["color_picker"] | fl::string("");
        int speed = json["speed_control"] | 0;
        
        CHECK_EQ(brightness, 128);
        CHECK_EQ(reset, true);
        CHECK_EQ(color, "#FF5500");
        CHECK_EQ(speed, 75);
        
        // Test safe access to missing fields
        int missing = json["non_existent_field"] | 999;
        CHECK_EQ(missing, 999);
        
        FL_WARN("New fl::Json UI parsing: SUCCESS");
        FL_WARN("SUCCESS: Both APIs can parse UI JSON data equivalently");
    }
}

TEST_CASE("JSON Strip Data Parsing - Standalone Pattern") {
    // Test with the exact JSON format that ActiveStripData produces
    const char* stripDataJson = R"([
        {"strip_id":0,"type":"r8g8b8"},
        {"strip_id":2,"type":"r8g8b8"},
        {"strip_id":5,"type":"r8g8b8"}
    ])";
    
    SUBCASE("Legacy ArduinoJSON strip parsing - WORKING") {
        FLArduinoJson::JsonDocument doc;
        auto result = FLArduinoJson::deserializeJson(doc, stripDataJson);
        CHECK(result == FLArduinoJson::DeserializationError::Ok);
        
        CHECK(doc.is<FLArduinoJson::JsonArray>());
        auto array = doc.as<FLArduinoJson::JsonArrayConst>();
        
        CHECK_EQ(array.size(), 3);
        
        // Test accessing strip data
        auto firstStrip = array[0].as<FLArduinoJson::JsonObjectConst>();
        CHECK_EQ(firstStrip["strip_id"].as<int>(), 0);
        CHECK_EQ(fl::string(firstStrip["type"].as<const char*>()), fl::string("r8g8b8"));
        
        auto thirdStrip = array[2].as<FLArduinoJson::JsonObjectConst>();
        CHECK_EQ(thirdStrip["strip_id"].as<int>(), 5);
        
        FL_WARN("Legacy strip JSON parsing: SUCCESS");
    }
    
    SUBCASE("New fl::Json strip parsing - WORKING") {
        auto json = fl::Json::parse(stripDataJson);
        CHECK(json.has_value());
        CHECK(json.is_array());
        CHECK_EQ(json.getSize(), 3);
        
        // Test accessing strip data with new API
        auto firstStrip = json[0];
        CHECK(firstStrip.is_object());
        
        int stripId = firstStrip["strip_id"] | -1;
        fl::string type = firstStrip["type"] | fl::string("");
        
        CHECK_EQ(stripId, 0);
        CHECK_EQ(type, "r8g8b8");
        
        // Test the third strip
        auto thirdStrip = json[2];
        int thirdStripId = thirdStrip["strip_id"] | -1;
        CHECK_EQ(thirdStripId, 5);
        
        // Test safe access to missing strip
        auto missingStrip = json[10]; // Index out of bounds
        int missingId = missingStrip["strip_id"] | 999;
        CHECK_EQ(missingId, 999); // Should get default
        
        FL_WARN("New fl::Json strip parsing: SUCCESS");
        FL_WARN("SUCCESS: Both APIs can parse strip JSON data equivalently");
    }
}

TEST_CASE("JSON API Compatibility Summary") {
    FL_WARN("=== JSON API COMPATIBILITY STATUS ===");
    FL_WARN("LEGACY ArduinoJSON API: ✅ FULLY WORKING");
    FL_WARN("  ✅ Create arrays and objects");
    FL_WARN("  ✅ Set values and nested structures");
    FL_WARN("  ✅ Serialize to JSON strings");
    FL_WARN("  ✅ Parse JSON strings");
    FL_WARN("  ✅ Access and iterate data");
    FL_WARN("");
    FL_WARN("NEW fl::Json API: ⚠️ PARTIALLY WORKING");
    FL_WARN("  ✅ Parse JSON strings correctly");
    FL_WARN("  ✅ Access data with [] operator and defaults (|)");
    FL_WARN("  ✅ Type checking (is_array, is_object)");
    FL_WARN("  ✅ Size and iteration methods");
    FL_WARN("  ❌ Factory methods (createArray, createObject) return wrong types");
    FL_WARN("  ❌ Modification methods (set, push_back) are unimplemented stubs");
    FL_WARN("  ❌ Serialization (serialize) causes crashes");
    FL_WARN("");
    FL_WARN("CONCLUSION: Both APIs can READ the same data equivalently when complete");
    FL_WARN("The new API needs implementation work for creation and serialization");
    FL_WARN("");
    FL_WARN("🎯 STRIP JSON UPDATE: ActiveStripData now includes new fl::Json parsing");
    FL_WARN("  ✅ parseStripJsonInfo() method demonstrates new API usage");
    FL_WARN("  ✅ infoJsonString() documents future new API usage");
    FL_WARN("  ✅ Both APIs can handle strip data parsing equivalently");
}

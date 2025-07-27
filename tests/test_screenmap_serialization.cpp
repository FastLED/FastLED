#include "test.h"
#include "fl/json.h"
#include "fl/screenmap.h"
#include "fl/map.h"

using namespace fl;

TEST_CASE("Test ScreenMap serialization") {
    // Create test ScreenMaps
    ScreenMap strip1(3, 0.5f);
    strip1.set(0, {0.0f, 0.0f});
    strip1.set(1, {1.0f, 0.0f});
    strip1.set(2, {2.0f, 0.0f});
    
    ScreenMap strip2(3, 0.3f);
    strip2.set(0, {0.0f, 1.0f});
    strip2.set(1, {1.0f, 1.0f});
    strip2.set(2, {2.0f, 1.0f});
    
    fl::fl_map<fl::string, ScreenMap> segmentMaps;
    segmentMaps["strip1"] = strip1;
    segmentMaps["strip2"] = strip2;
    
    // Serialize to JSON using new json2 implementation
    fl::Json doc;
    ScreenMap::toJson(segmentMaps, &doc);
    
    // Also test with string serialization
    fl::string jsonBuffer = doc.to_string();
    FL_WARN("Generated JSON: " << jsonBuffer);
    
    // Try parsing with a simple test first
    fl::Json simpleJson = fl::Json::parse("{\"test\": 123}");
    CHECK(simpleJson.is_object());
    CHECK(simpleJson.contains("test"));
    
    // Try parsing our generated JSON
    fl::Json parsedJson = fl::Json::parse(jsonBuffer.c_str());
    CHECK(parsedJson.is_object());
    if (parsedJson.is_object()) {
        FL_WARN("Parsed JSON is object");
        CHECK(parsedJson.contains("map"));
        if (parsedJson.contains("map")) {
            FL_WARN("Contains map key");
        } else {
            FL_WARN("Does NOT contain map key");
        }
    } else {
        FL_WARN("Parsed JSON is NOT object");
    }
}
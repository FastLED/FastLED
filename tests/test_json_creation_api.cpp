#include "test.h"
#include "fl/json.h"
#include "fl/namespace.h"
#include "fl/warn.h"

FASTLED_USING_NAMESPACE

#if FASTLED_ENABLE_JSON

TEST_CASE("JSON Creation API - Array factory method") {
    auto json = fl::Json::createArray();
    CHECK(json.has_value());
    CHECK(json.is_array());
    CHECK_FALSE(json.is_object());
    CHECK_EQ(json.getSize(), 0);
    
    fl::string serialized = json.serialize();
    FL_WARN("Created array serializes to: " << serialized);
    CHECK(serialized == "[]");
}

TEST_CASE("JSON Creation API - Object factory method") {
    auto json = fl::Json::createObject();
    CHECK(json.has_value());
    CHECK(json.is_object());
    CHECK_FALSE(json.is_array());
    CHECK_EQ(json.getSize(), 0);
    
    fl::string serialized = json.serialize();
    FL_WARN("Created object serializes to: " << serialized);
    CHECK(serialized == "{}");
}

TEST_CASE("JSON Modification API - Object building") {
    auto json = fl::Json::createObject();
    
    // Test various value types
    json.set("name", "test");
    json.set("count", 42);
    json.set("enabled", true);
    json.set("value", 3.14f);
    
    fl::string output = json.serialize();
    FL_WARN("Built object JSON: " << output);
    
    // Verify the JSON contains expected values
    CHECK(output.find("\"name\"") != fl::string::npos);
    CHECK(output.find("\"test\"") != fl::string::npos);
    CHECK(output.find("\"count\"") != fl::string::npos);
    CHECK(output.find("42") != fl::string::npos);
    CHECK(output.find("\"enabled\"") != fl::string::npos);
    CHECK(output.find("true") != fl::string::npos);
    
    // Verify it can be parsed back
    auto reparsed = fl::Json::parse(output.c_str());
    CHECK(reparsed.has_value());
    CHECK(reparsed.is_object());
    CHECK_EQ(reparsed["name"] | fl::string(""), fl::string("test"));
    CHECK_EQ(reparsed["count"] | 0, 42);
    CHECK_EQ(reparsed["enabled"] | false, true);
}

TEST_CASE("JSON Modification API - Array building") {
    auto json = fl::Json::createArray();
    
    // Create elements to add
    auto obj1 = fl::Json::createObject();
    obj1.set("id", 1);
    obj1.set("name", "item1");
    
    auto obj2 = fl::Json::createObject();
    obj2.set("id", 2);
    obj2.set("name", "item2");
    
    // Add objects to array
    json.push_back(obj1);
    json.push_back(obj2);
    
    CHECK_EQ(json.getSize(), 2);
    
    fl::string output = json.serialize();
    FL_WARN("Built array JSON: " << output);
    
    // Verify array structure
    CHECK(output[0] == '[');
    CHECK(output.find("\"id\":1") != fl::string::npos);
    CHECK(output.find("\"name\":\"item1\"") != fl::string::npos);
    CHECK(output.find("\"id\":2") != fl::string::npos);
    CHECK(output.find("\"name\":\"item2\"") != fl::string::npos);
    
    // Verify it can be parsed back
    auto reparsed = fl::Json::parse(output.c_str());
    CHECK(reparsed.has_value());
    CHECK(reparsed.is_array());
    CHECK_EQ(reparsed.getSize(), 2);
    CHECK_EQ(reparsed[0]["id"] | -1, 1);
    CHECK_EQ(reparsed[1]["id"] | -1, 2);
}

TEST_CASE("JSON Strip Data Building - ActiveStripData Pattern") {
    // Test building the exact JSON that ActiveStripData needs to create
    auto json = fl::Json::createArray();
    
    for (int stripId : {0, 2, 5}) {
        auto stripObj = fl::Json::createObject();
        stripObj.set("strip_id", stripId);
        stripObj.set("type", "r8g8b8");
        json.push_back(stripObj);
    }
    
    fl::string output = json.serialize();
    FL_WARN("Built strip JSON: " << output);
    
    // Should produce: [{"strip_id":0,"type":"r8g8b8"},{"strip_id":2,"type":"r8g8b8"},{"strip_id":5,"type":"r8g8b8"}]
    
    // Verify structure
    CHECK(output[0] == '[');
    CHECK(output.find("\"strip_id\":0") != fl::string::npos);
    CHECK(output.find("\"strip_id\":2") != fl::string::npos);
    CHECK(output.find("\"strip_id\":5") != fl::string::npos);
    CHECK(output.find("\"type\":\"r8g8b8\"") != fl::string::npos);
    
    // Verify parsing works
    auto reparsed = fl::Json::parse(output.c_str());
    CHECK(reparsed.has_value());
    CHECK(reparsed.is_array());
    CHECK_EQ(reparsed.getSize(), 3);
    CHECK_EQ(reparsed[0]["strip_id"] | -1, 0);
    CHECK_EQ(reparsed[1]["strip_id"] | -1, 2);
    CHECK_EQ(reparsed[2]["strip_id"] | -1, 5);
    CHECK_EQ(reparsed[0]["type"] | fl::string(""), fl::string("r8g8b8"));
}

TEST_CASE("JSON Creation API - Error handling") {
    auto json = fl::Json::createObject();
    json.set(nullptr, "test");  // Should not crash
    
    fl::string output = json.serialize();
    CHECK(output == "{}");  // Should still be empty object
}

#endif  // FASTLED_ENABLE_JSON 

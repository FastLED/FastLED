
#include "test.h"
#include "fl/json.h"
#include "fl/namespace.h"

FASTLED_USING_NAMESPACE

TEST_CASE("JSON createArray() debug") {
    auto json = fl::Json::createArray();
    
    FL_WARN("createArray() result:");
    FL_WARN("  has_value(): " << (json.has_value() ? "true" : "false"));
    FL_WARN("  is_array(): " << (json.is_array() ? "true" : "false"));
    FL_WARN("  is_object(): " << (json.is_object() ? "true" : "false"));
    FL_WARN("  isNull(): " << (json.isNull() ? "true" : "false"));
    FL_WARN("  getSize(): " << json.getSize());
    
    fl::string serialized = json.serialize();
    FL_WARN("  serialize(): " << serialized);
    
    // These should pass but probably don't:
    CHECK(json.has_value());
    CHECK(json.is_array());
    CHECK_FALSE(json.is_object());
    CHECK_EQ(json.getSize(), 0);
    CHECK_EQ(serialized, "[]");
}

TEST_CASE("JSON createObject() debug") {
    auto json = fl::Json::createObject();
    
    FL_WARN("createObject() result:");
    FL_WARN("  has_value(): " << (json.has_value() ? "true" : "false"));
    FL_WARN("  is_array(): " << (json.is_array() ? "true" : "false"));
    FL_WARN("  is_object(): " << (json.is_object() ? "true" : "false"));
    FL_WARN("  isNull(): " << (json.isNull() ? "true" : "false"));
    FL_WARN("  getSize(): " << json.getSize());
    
    fl::string serialized = json.serialize();
    FL_WARN("  serialize(): " << serialized);
    
    // These should pass but probably don't:
    CHECK(json.has_value());
    CHECK(json.is_object());
    CHECK_FALSE(json.is_array());
    CHECK_EQ(json.getSize(), 0);
    CHECK_EQ(serialized, "{}");
}

TEST_CASE("JSON set() method debug") {
    auto json = fl::Json::createObject();
    
    FL_WARN("Testing set() method:");
    json.set("test_key", "test_value");
    json.set("number", 42);
    json.set("flag", true);
    
    FL_WARN("  After setting values:");
    FL_WARN("  getSize(): " << json.getSize());
    
    fl::string serialized = json.serialize();
    FL_WARN("  serialize(): " << serialized);
    
    // Test accessing the set values
    auto testValue = json["test_key"];
    FL_WARN("  json[\"test_key\"] has_value(): " << (testValue.has_value() ? "true" : "false"));
    
    fl::string retrievedValue = json["test_key"] | fl::string("MISSING");
    FL_WARN("  Retrieved value: " << retrievedValue);
    
    // These should work but probably don't:
    CHECK_EQ(json.getSize(), 3);
    CHECK_EQ(json["test_key"] | fl::string(""), "test_value");
    CHECK_EQ(json["number"] | 0, 42);
    CHECK_EQ(json["flag"] | false, true);
}

#include "test.h"
#include "fl/json.h"

using namespace fl;

TEST_CASE("Json Float Data Parsing") {
    SUBCASE("Array of float values should become float data") {
        // Create JSON with array of float values that can't fit in any integer type
        fl::string jsonStr = "[100000.5, 200000.7, 300000.14159, 400000.1, 500000.5]";
        Json json = Json::parse(jsonStr);
        
        // Print the type for debugging
        FASTLED_WARN("JSON type: " << (json.is_floats() ? "floats" : 
                      json.is_audio() ? "audio" : 
                      json.is_bytes() ? "bytes" : 
                      json.is_array() ? "array" : "other"));
        
        CHECK(json.is_floats());
        CHECK_FALSE(json.is_array()); // Should not be regular array anymore
        CHECK_FALSE(json.is_audio()); // Should not be audio data
        CHECK_FALSE(json.is_bytes()); // Should not be byte data
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of float data
        fl::optional<fl::vector<float>> floatData = json.as_floats();
        REQUIRE(floatData);
        CHECK_EQ(floatData->size(), 5);
        // Use approximate equality for floats
        CHECK((*floatData)[0] == 100000.5f);
        CHECK((*floatData)[1] == 200000.7f);
        CHECK((*floatData)[2] == 300000.14159f);
        CHECK((*floatData)[3] == 400000.1f);
        CHECK((*floatData)[4] == 500000.5f);
    }
    
    SUBCASE("Array with values that can't be represented as floats should remain regular array") {
        // Create JSON with array containing values that can't be exactly represented as floats
        fl::string jsonStr = "[16777217.0, -16777217.0]"; // Beyond float precision
        Json json = Json::parse(jsonStr);
        
        CHECK(json.is_array());
        CHECK_FALSE(json.is_floats());
        CHECK_FALSE(json.is_audio());
        CHECK_FALSE(json.is_bytes());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of regular array
        fl::optional<JsonArray> arrayData = json.as_array();
        REQUIRE(arrayData);
        CHECK_EQ(arrayData->size(), 2);
    }
    
    SUBCASE("Array with non-numeric values should remain regular array") {
        // Create JSON with array containing non-numeric values
        fl::string jsonStr = "[100000.5, 200000.7, \"hello\", 400000.1]";
        Json json = Json::parse(jsonStr);
        
        CHECK(json.is_array());
        CHECK_FALSE(json.is_floats());
        CHECK_FALSE(json.is_audio());
        CHECK_FALSE(json.is_bytes());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of regular array
        fl::optional<JsonArray> arrayData = json.as_array();
        REQUIRE(arrayData);
        CHECK_EQ(arrayData->size(), 4);
    }
    
    SUBCASE("Empty array should remain regular array") {
        // Create JSON with empty array
        fl::string jsonStr = "[]";
        Json json = Json::parse(jsonStr);
        
        CHECK(json.is_array());
        CHECK_FALSE(json.is_floats());
        CHECK_FALSE(json.is_audio());
        CHECK_FALSE(json.is_bytes());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of regular array
        fl::optional<JsonArray> arrayData = json.as_array();
        REQUIRE(arrayData);
        CHECK_EQ(arrayData->size(), 0);
    }
    
    SUBCASE("Array with integers that fit in float but not in int16 should become float data") {
        // Create JSON with array of integers that don't fit in int16_t
        fl::string jsonStr = "[40000, 50000, 60000, 70000]";
        Json json = Json::parse(jsonStr);
        
        // Print the type for debugging
        FASTLED_WARN("JSON type: " << (json.is_floats() ? "floats" : 
                      json.is_audio() ? "audio" : 
                      json.is_bytes() ? "bytes" : 
                      json.is_array() ? "array" : "other"));
        
        CHECK(json.is_floats());
        CHECK_FALSE(json.is_array()); // Should not be regular array anymore
        CHECK_FALSE(json.is_audio()); // Should not be audio data
        CHECK_FALSE(json.is_bytes()); // Should not be byte data
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of float data
        fl::optional<fl::vector<float>> floatData = json.as_floats();
        REQUIRE(floatData);
        CHECK_EQ(floatData->size(), 4);
        CHECK_EQ((*floatData)[0], 40000.0f);
        CHECK_EQ((*floatData)[1], 50000.0f);
        CHECK_EQ((*floatData)[2], 60000.0f);
        CHECK_EQ((*floatData)[3], 70000.0f);
    }
}
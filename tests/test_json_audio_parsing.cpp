#include "test.h"
#include "fl/json.h"

using namespace fl;

TEST_CASE("Json Audio Data Parsing") {
    SUBCASE("Array of int16 values should become audio data") {
        // Create JSON with array of values that fit in int16_t but not uint8_t
        fl::string jsonStr = "[100, -200, 32767, -32768, 0]";
        Json json = Json::parse(jsonStr);
        
        CHECK(json.is_audio());
        CHECK_FALSE(json.is_array()); // Should not be regular array anymore
        CHECK_FALSE(json.is_bytes()); // Should not be byte data
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of audio data
        fl::optional<fl::vector<int16_t>> audioData = json.as_audio();
        REQUIRE(audioData);
        CHECK_EQ(audioData->size(), 5);
        CHECK_EQ((*audioData)[0], 100);
        CHECK_EQ((*audioData)[1], -200);
        CHECK_EQ((*audioData)[2], 32767);
        CHECK_EQ((*audioData)[3], -32768);
        CHECK_EQ((*audioData)[4], 0);
    }
    
    SUBCASE("Array with boolean values should become byte data, not audio") {
        // Create JSON with array of boolean values (0s and 1s)
        fl::string jsonStr = "[1, 0, 1, 1, 0]";
        Json json = Json::parse(jsonStr);
        
        // Should become byte data, not audio data
        CHECK(json.is_bytes());
        CHECK_FALSE(json.is_audio());
        CHECK_FALSE(json.is_array()); // Should not be regular array anymore
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of byte data
        fl::optional<fl::vector<uint8_t>> byteData = json.as_bytes();
        REQUIRE(byteData);
        CHECK_EQ(byteData->size(), 5);
    }
    
    SUBCASE("Array with values outside int16 range should remain regular array") {
        // Create JSON with array containing values outside int16_t range
        fl::string jsonStr = "[100, -200, 32768, -32769, 0]"; // 32768 and -32769 exceed int16_t range
        Json json = Json::parse(jsonStr);
        
        CHECK(json.is_array());
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
        CHECK_EQ(arrayData->size(), 5);
    }
    
    SUBCASE("Array with non-integer values should remain regular array") {
        // Create JSON with array containing non-integer values
        fl::string jsonStr = "[100, -200, 3.14, 0]";
        Json json = Json::parse(jsonStr);
        
        CHECK(json.is_array());
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
    
    SUBCASE("Mixed array with int16 values should remain regular array") {
        // Create JSON with mixed array (mix of int16 and non-int16 values)
        fl::string jsonStr = "[100, \"hello\", 32767]";
        Json json = Json::parse(jsonStr);
        
        CHECK(json.is_array());
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
        CHECK_EQ(arrayData->size(), 3);
    }
}
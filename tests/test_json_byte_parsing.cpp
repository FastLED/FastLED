#include "test.h"
#include "fl/json.h"

using namespace fl;

TEST_CASE("Json Byte Data Parsing") {
    SUBCASE("Array of uint8 values should become byte data") {
        // Create JSON with array of values that fit in uint8_t
        fl::string jsonStr = "[100, 200, 255, 0, 128]";
        Json json = Json::parse(jsonStr);
        
        CHECK(json.is_bytes());
        CHECK_FALSE(json.is_array()); // Should not be regular array anymore
        CHECK_FALSE(json.is_audio()); // Should not be audio data
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of byte data
        fl::optional<fl::vector<uint8_t>> byteData = json.as_bytes();
        REQUIRE(byteData);
        CHECK_EQ(byteData->size(), 5);
        CHECK_EQ((*byteData)[0], 100);
        CHECK_EQ((*byteData)[1], 200);
        CHECK_EQ((*byteData)[2], 255);
        CHECK_EQ((*byteData)[3], 0);
        CHECK_EQ((*byteData)[4], 128);
    }
    
    SUBCASE("Array with boolean values should become byte data") {
        // Create JSON with array of boolean-like values (0s and 1s)
        fl::string jsonStr = "[1, 0, 1, 1, 0]";
        Json json = Json::parse(jsonStr);
        
        CHECK(json.is_bytes());
        CHECK_FALSE(json.is_array()); // Should not be regular array anymore
        CHECK_FALSE(json.is_audio()); // Should not be audio data
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of byte data
        fl::optional<fl::vector<uint8_t>> byteData = json.as_bytes();
        REQUIRE(byteData);
        CHECK_EQ(byteData->size(), 5);
        CHECK_EQ((*byteData)[0], 1);
        CHECK_EQ((*byteData)[1], 0);
        CHECK_EQ((*byteData)[2], 1);
        CHECK_EQ((*byteData)[3], 1);
        CHECK_EQ((*byteData)[4], 0);
    }
    
    SUBCASE("Array with float boolean values should become byte data") {
        // Create JSON with array of float boolean-like values (0.0s and 1.0s)
        fl::string jsonStr = "[1.0, 0.0, 1.0, 1.0, 0.0]";
        Json json = Json::parse(jsonStr);
        
        CHECK(json.is_bytes());
        CHECK_FALSE(json.is_array()); // Should not be regular array anymore
        CHECK_FALSE(json.is_audio()); // Should not be audio data
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of byte data
        fl::optional<fl::vector<uint8_t>> byteData = json.as_bytes();
        REQUIRE(byteData);
        CHECK_EQ(byteData->size(), 5);
        CHECK_EQ((*byteData)[0], 1);
        CHECK_EQ((*byteData)[1], 0);
        CHECK_EQ((*byteData)[2], 1);
        CHECK_EQ((*byteData)[3], 1);
        CHECK_EQ((*byteData)[4], 0);
    }
    
    SUBCASE("Array with values outside uint8 range should become audio data or regular array") {
        // Create JSON with array containing values outside uint8_t range
        fl::string jsonStr = "[100, 200, 256, 0, 128]"; // 256 exceeds uint8_t range
        Json json = Json::parse(jsonStr);
        
        // Should become audio data since values fit in int16_t
        CHECK(json.is_audio());
        CHECK_FALSE(json.is_bytes());
        CHECK_FALSE(json.is_array());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of audio data
        fl::optional<fl::vector<int16_t>> audioData = json.as_audio();
        REQUIRE(audioData);
        CHECK_EQ(audioData->size(), 5);
    }
    
    SUBCASE("Array with negative values should become audio data or regular array") {
        // Create JSON with array containing negative values
        fl::string jsonStr = "[100, -1, 255, 0, 128]";
        Json json = Json::parse(jsonStr);
        
        // Should become audio data since values fit in int16_t but not uint8_t
        CHECK(json.is_audio());
        CHECK_FALSE(json.is_bytes());
        CHECK_FALSE(json.is_array());
        CHECK_FALSE(json.is_int());
        CHECK_FALSE(json.is_double());
        CHECK_FALSE(json.is_string());
        CHECK_FALSE(json.is_bool());
        CHECK_FALSE(json.is_null());
        
        // Test extraction of audio data
        fl::optional<fl::vector<int16_t>> audioData = json.as_audio();
        REQUIRE(audioData);
        CHECK_EQ(audioData->size(), 5);
    }
    
    SUBCASE("Array with values outside int16 range should remain regular array") {
        // Create JSON with array containing values outside int16_t range
        fl::string jsonStr = "[100, 200, 32768, 0, 128]"; // 32768 exceeds int16_t range
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
        fl::string jsonStr = "[100, 200, 3.14, 0, 128]";
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
    
    SUBCASE("Array with non-integer boolean-like values should remain regular array") {
        // Create JSON with array containing non-integer boolean-like values
        fl::string jsonStr = "[100, 200, 1.5, 0, 128]";
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
    
    SUBCASE("Mixed array should remain regular array") {
        // Create JSON with mixed array (mix of uint8 and non-uint8 values)
        fl::string jsonStr = "[100, \"hello\", 255]";
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
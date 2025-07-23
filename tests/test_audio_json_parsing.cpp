#include <doctest.h>
#include <string>
#include "fl/json.h"
#include "fl/string.h"
#include "fl/vector.h"

#if FASTLED_ENABLE_JSON

// Copy the AudioBuffer struct and parsing function from audio.cpp for testing
struct AudioBuffer {
    fl::vector<int16_t> samples;
    uint32_t timestamp = 0;
};

// Copy the helper functions from audio.cpp
//static bool isdigit(char c) { return c >= '0' && c <= '9'; }
static bool isspace(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

// Fast manual parsing of PCM data from a samples array string
// Input: samples string like "[1,2,3,-4,5]"
// Output: fills the samples vector
static void parsePcmSamplesString(const fl::string &samplesStr, fl::vector<int16_t> *samples) {
    samples->clear();
    
    size_t i = 0, n = samplesStr.size();
    
    // Find opening '['
    while (i < n && samplesStr[i] != '[')
        ++i;
    if (i == n) return; // no array found
    ++i; // skip '['
    
    while (i < n && samplesStr[i] != ']') {
        // Skip whitespace
        while (i < n && isspace(samplesStr[i]))
            ++i;
        if (i >= n || samplesStr[i] == ']') break;
        
        // Parse number
        bool negative = false;
        if (samplesStr[i] == '-') {
            negative = true;
            ++i;
        } else if (samplesStr[i] == '+') {
            ++i;
        }
        
        int value = 0;
        bool hasDigits = false;
        while (i < n && isdigit(static_cast<unsigned char>(samplesStr[i]))) {
            hasDigits = true;
            value = value * 10 + (samplesStr[i] - '0');
            ++i;
        }
        
        if (hasDigits) {
            if (negative) value = -value;
            samples->push_back(static_cast<int16_t>(value));
        }
        
        // Skip whitespace and comma
        while (i < n && (isspace(samplesStr[i]) || samplesStr[i] == ','))
            ++i;
    }
}

// Hybrid JSON/manual parsing function for testing
static void parseJsonToAudioBuffers(const fl::string &jsonStr,
                                    fl::vector<AudioBuffer> *audioBuffers) {
    audioBuffers->clear();
    
    // Parse JSON string using ideal API
    fl::Json json = fl::Json::parse(jsonStr.c_str());
    
    if (!json.is_array()) {
        return;
    }
    
    for (size_t i = 0; i < json.size(); ++i) {
        fl::Json item = json[i];
        if (!item.is_object()) {
            continue;
        }
        
        AudioBuffer buffer;
        
        // Extract timestamp - use get<uint32_t> for large values
        auto timestampOpt = item["timestamp"].get<uint32_t>();
        if (timestampOpt.has_value()) {
            buffer.timestamp = *timestampOpt;
        } else {
            buffer.timestamp = 0;
        }
        
        // Extract samples array
        fl::Json samplesJson = item["samples"];
        if (samplesJson.is_array()) {
            for (size_t j = 0; j < samplesJson.size(); ++j) {
                int16_t sample = samplesJson[j] | 0;
                buffer.samples.push_back(sample);
            }
        }
        
        // Only add buffer if it has samples (matches original behavior)
        if (!buffer.samples.empty()) {
            audioBuffers->push_back(buffer);
        }
    }
}

TEST_CASE("AudioJsonParsing - Single buffer with samples and timestamp") {
    fl::string jsonStr = R"([{"samples": [100, -200, 300], "timestamp": 1234567890}])";
    fl::vector<AudioBuffer> buffers;
    
    parseJsonToAudioBuffers(jsonStr, &buffers);
    
    REQUIRE(buffers.size() == 1);
    REQUIRE(buffers[0].samples.size() == 3);
    CHECK(buffers[0].samples[0] == 100);
    CHECK(buffers[0].samples[1] == -200);
    CHECK(buffers[0].samples[2] == 300);
    CHECK(buffers[0].timestamp == 1234567890);
}

TEST_CASE("AudioJsonParsing - Multiple buffers with different timestamps") {
    fl::string jsonStr = R"([
        {"samples": [1, 2, 3], "timestamp": 1000},
        {"samples": [4, 5, 6], "timestamp": 2000}
    ])";
    fl::vector<AudioBuffer> buffers;
    
    parseJsonToAudioBuffers(jsonStr, &buffers);
    
    REQUIRE(buffers.size() == 2);
    
    // First buffer
    REQUIRE(buffers[0].samples.size() == 3);
    CHECK(buffers[0].samples[0] == 1);
    CHECK(buffers[0].samples[1] == 2);
    CHECK(buffers[0].samples[2] == 3);
    CHECK(buffers[0].timestamp == 1000);
    
    // Second buffer
    REQUIRE(buffers[1].samples.size() == 3);
    CHECK(buffers[1].samples[0] == 4);
    CHECK(buffers[1].samples[1] == 5);
    CHECK(buffers[1].samples[2] == 6);
    CHECK(buffers[1].timestamp == 2000);
}

TEST_CASE("AudioJsonParsing - Empty samples array") {
    fl::string jsonStr = R"([{"samples": [], "timestamp": 1234567890}])";
    fl::vector<AudioBuffer> buffers;
    
    parseJsonToAudioBuffers(jsonStr, &buffers);
    
    // Should not add buffers with empty samples
    CHECK(buffers.size() == 0);
}

TEST_CASE("AudioJsonParsing - Large 16-bit values") {
    fl::string jsonStr = R"([{"samples": [32767, -32768, 0], "timestamp": 1234567890}])";
    fl::vector<AudioBuffer> buffers;
    
    parseJsonToAudioBuffers(jsonStr, &buffers);
    
    REQUIRE(buffers.size() == 1);
    REQUIRE(buffers[0].samples.size() == 3);
    CHECK(buffers[0].samples[0] == 32767);   // Max int16_t
    CHECK(buffers[0].samples[1] == -32768);  // Min int16_t
    CHECK(buffers[0].samples[2] == 0);
    CHECK(buffers[0].timestamp == 1234567890UL);
}

TEST_CASE("AudioJsonParsing - Whitespace tolerance") {
    fl::string jsonStr = R"([  {  "samples"  :  [  1  ,  2  ,  3  ]  ,  "timestamp"  :  1234  }  ])";
    fl::vector<AudioBuffer> buffers;
    
    parseJsonToAudioBuffers(jsonStr, &buffers);
    
    REQUIRE(buffers.size() == 1);
    REQUIRE(buffers[0].samples.size() == 3);
    CHECK(buffers[0].samples[0] == 1);
    CHECK(buffers[0].samples[1] == 2);
    CHECK(buffers[0].samples[2] == 3);
    CHECK(buffers[0].timestamp == 1234);
}

TEST_CASE("AudioJsonParsing - Properties in different order") {
    fl::string jsonStr = R"([{"timestamp": 5678, "samples": [10, 20]}])";
    fl::vector<AudioBuffer> buffers;
    
    parseJsonToAudioBuffers(jsonStr, &buffers);
    
    REQUIRE(buffers.size() == 1);
    REQUIRE(buffers[0].samples.size() == 2);
    CHECK(buffers[0].samples[0] == 10);
    CHECK(buffers[0].samples[1] == 20);
    CHECK(buffers[0].timestamp == 5678);
}

TEST_CASE("AudioJsonParsing - Invalid JSON formats") {
    fl::vector<AudioBuffer> buffers;
    
    SUBCASE("Empty string") {
        parseJsonToAudioBuffers("", &buffers);
        CHECK(buffers.size() == 0);
    }
    
    SUBCASE("No array") {
        parseJsonToAudioBuffers("not an array", &buffers);
        CHECK(buffers.size() == 0);
    }
    
    SUBCASE("Malformed array") {
        parseJsonToAudioBuffers("[{incomplete", &buffers);
        CHECK(buffers.size() == 0);
    }
    
    SUBCASE("Missing samples") {
        parseJsonToAudioBuffers(R"([{"timestamp": 1234}])", &buffers);
        CHECK(buffers.size() == 0);
    }
}

TEST_CASE("AudioJsonParsing - Realistic audio data") {
    // Simulate real audio data with typical PCM values
    fl::string jsonStr = R"([
        {"samples": [512, -1024, 2048, -512, 0, 1536, -2048], "timestamp": 1000000},
        {"samples": [256, -768, 1024, -256, 128, -384, 512], "timestamp": 1000010}
    ])";
    fl::vector<AudioBuffer> buffers;
    
    parseJsonToAudioBuffers(jsonStr, &buffers);
    
    REQUIRE(buffers.size() == 2);
    
    // First buffer
    REQUIRE(buffers[0].samples.size() == 7);
    CHECK(buffers[0].timestamp == 1000000UL);
    
    // Second buffer  
    REQUIRE(buffers[1].samples.size() == 7);
    CHECK(buffers[1].timestamp == 1000010UL);
    
    // Verify some sample values
    CHECK(buffers[0].samples[0] == 512);
    CHECK(buffers[0].samples[1] == -1024);
    CHECK(buffers[1].samples[0] == 256);
    CHECK(buffers[1].samples[1] == -768);
}

TEST_CASE("AudioJsonParsing - Edge case with many samples") {
    // Test with a larger number of samples (typical audio block size)
    fl::string jsonStr = R"([{"samples": [)";
    
    // Generate 512 sample values (typical audio block size)
    for (int i = 0; i < 512; ++i) {
        if (i > 0) jsonStr += ",";
        jsonStr.append(i % 2 == 0 ? i : -i);
    }
    
    jsonStr += R"(], "timestamp": 1234567890}])";
    
    fl::vector<AudioBuffer> buffers;
    parseJsonToAudioBuffers(jsonStr, &buffers);
    
    REQUIRE(buffers.size() == 1);
    REQUIRE(buffers[0].samples.size() == 512);
    CHECK(buffers[0].timestamp == 1234567890);
    
    // Verify pattern: positive/negative alternating
    CHECK(buffers[0].samples[0] == 0);
    CHECK(buffers[0].samples[1] == -1);
    CHECK(buffers[0].samples[2] == 2);
    CHECK(buffers[0].samples[3] == -3);
    CHECK(buffers[0].samples[511] == -511);
}

TEST_CASE("AudioJsonParsing - Large buffer preserved without chunking") {
    // Test with more than 512 samples to verify no chunking occurs
    // Previously, buffers were chunked into 512-sample pieces, now they should remain intact
    fl::string jsonStr = R"([{"samples": [)";
    
    // Generate 1024 sample values (larger than old chunk size of 512)
    for (int i = 0; i < 1024; ++i) {
        if (i > 0) jsonStr += ",";
        jsonStr.append(i);
    }
    
    jsonStr += R"(], "timestamp": 4000000000}])";
    
    fl::vector<AudioBuffer> buffers;
    parseJsonToAudioBuffers(jsonStr, &buffers);
    
    // Should have exactly ONE buffer (not chunked into multiple 512-sample pieces)
    REQUIRE(buffers.size() == 1);
    
    // The single buffer should contain all 1024 samples
    REQUIRE(buffers[0].samples.size() == 1024);
    CHECK(buffers[0].timestamp == 4000000000UL);
    
    // Verify first, middle, and last samples
    CHECK(buffers[0].samples[0] == 0);
    CHECK(buffers[0].samples[511] == 511);  // This would be in a separate chunk with old behavior
    CHECK(buffers[0].samples[1023] == 1023);
}

TEST_CASE("JSON Audio: serialize JSON audio samples to string") {
    fl::vector<int16_t> samples = {1000, -1500, 2000, -2500, 3000};
    uint32_t timestamp = 12345;
    
    // Manual JSON construction since JsonBuilder doesn't support vector<int16_t> yet
    fl::JsonDocument doc;
    doc["timestamp"] = timestamp;
    auto samplesArray = doc["samples"].to<FLArduinoJson::JsonArray>();
    for (auto sample : samples) {
        samplesArray.add(sample);
    }
    
    fl::string jsonStr = doc.serialize();
    
    CHECK(!jsonStr.empty());
    CHECK(jsonStr.find("1000") != fl::string::npos);
    CHECK(jsonStr.find("-1500") != fl::string::npos);
}

#endif // FASTLED_ENABLE_JSON 

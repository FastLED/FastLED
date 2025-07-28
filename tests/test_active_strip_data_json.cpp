#include "test.h"
#include "fl/json.h"
#include "fl/namespace.h"
#include "platforms/shared/active_strip_data/active_strip_data.h"
#include "FastLED.h"
#include "cpixel_ledcontroller.h"
#include "pixel_controller.h"
#include "eorder.h"

FASTLED_USING_NAMESPACE

TEST_CASE("ActiveStripData JSON Round-Trip Test") {
    FL_WARN("Testing ActiveStripData JSON round-trip...");
    
    // Create a simple stub controller for testing
    class StubController : public CPixelLEDController<RGB> {
    private:
        int mStripId;
        
    public:
        StubController(int stripId) : CPixelLEDController<RGB>(), mStripId(stripId) {}
        
        void init() override {}
        
        void showPixels(PixelController<RGB>& pixels) override {
            fl::ActiveStripData& data = fl::ActiveStripData::Instance();
            
            // Create RGB buffer from pixels
            fl::vector<uint8_t> rgbBuffer;
            rgbBuffer.resize(pixels.size() * 3);
            
            auto iterator = pixels.as_iterator(RgbwInvalid());
            size_t idx = 0;
            while (iterator.has(1)) {
                uint8_t r, g, b;
                iterator.loadAndScaleRGB(&r, &g, &b);
                rgbBuffer[idx++] = r;
                rgbBuffer[idx++] = g;
                rgbBuffer[idx++] = b;
                iterator.advanceData();
            }
            
            data.update(mStripId, 1000, rgbBuffer.data(), rgbBuffer.size());
        }
        
        uint16_t getMaxRefreshRate() const override { return 60; }
    };
    
    // Set up test data
    CRGB leds1[2] = {CRGB::Red, CRGB::Green};
    CRGB leds2[3] = {CRGB::Blue, CRGB::Yellow, CRGB::Magenta};
    
    static StubController controller10(10);
    static StubController controller20(20);
    
    FastLED.addLeds(&controller10, leds1, 2);
    FastLED.addLeds(&controller20, leds2, 3);
    
    // Trigger data population
    FastLED.show();
    
    FL_WARN("Populated ActiveStripData with 2 strips (IDs: 10, 20)");
    
    // Test only the legacy method first
    fl::ActiveStripData& data = fl::ActiveStripData::Instance();
    fl::string legacyJson = data.infoJsonString();
    
    FL_WARN("Legacy JSON: " << legacyJson);
    
    // Parse back to verify data
    auto parsed = fl::Json::parse(legacyJson.c_str());
    
    CHECK(parsed.has_value());
    CHECK(parsed.is_array());
    CHECK_EQ(parsed.getSize(), 2);
    
    // Verify strip data
    bool found10 = false, found20 = false;
    for (size_t i = 0; i < parsed.getSize(); ++i) {
        auto strip = parsed[static_cast<int>(i)];
        int id = strip["strip_id"] | -1;
        fl::string type = strip["type"] | fl::string("missing");
        
        if (id == 10) {
            found10 = true;
            CHECK_EQ(type, "r8g8b8");
        } else if (id == 20) {
            found20 = true;
            CHECK_EQ(type, "r8g8b8");
        }
    }
    
    CHECK(found10);
    CHECK(found20);
    
    FL_WARN("SUCCESS: Legacy JSON round-trip works correctly!");
    
    // Test the new API now that it's working
    fl::string newJson = data.infoJsonStringNew();
    FL_WARN("New JSON:    " << newJson);
    
    // Both should produce semantically identical output (field order may differ)
    fl::Json legacyParsed = fl::Json::parse(legacyJson.c_str());
    fl::Json newParsed = fl::Json::parse(newJson.c_str());
    
    // Verify both are arrays with same length
    CHECK(legacyParsed.is_array());
    CHECK(newParsed.is_array());
    CHECK_EQ(legacyParsed.size(), newParsed.size());
    
    // Verify each element has the same content (regardless of field order)
    for (fl::size_t i = 0; i < legacyParsed.size(); i++) {
        fl::Json legacyItem = legacyParsed[i];
        fl::Json newItem = newParsed[i];
        
        CHECK_EQ(legacyItem["strip_id"].as<int>(), newItem["strip_id"].as<int>());
        CHECK_EQ(legacyItem["type"].as<fl::string>(), newItem["type"].as<fl::string>());
    }
    
    FL_WARN("SUCCESS: Both serializers produce identical output!");


    
} 

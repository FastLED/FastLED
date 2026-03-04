// Tests that UIAudio with a URL properly serializes the URL field
// in the JSON UI output on the stub platform.

#include "fl/ui.h"
#include "fl/json.h"
#include "fl/warn.h"
#include "fl/stl/url.h"
#include "platforms/shared/ui/json/ui.h"
#include "platforms/shared/ui/json/audio.h"
#include "platforms/shared/ui/json/audio_internal.h"
#include "test.h"

using namespace fl;

FL_TEST_CASE("UIAudio URL constructor serializes url field") {
    // Set up JSON UI handler to capture serialized output
    fl::string capturedJson;
    auto updateEngineState = fl::setJsonUiHandlers(
        [&](const char* jsonStr) {
            capturedJson = jsonStr;
        }
    );
    FL_REQUIRE(updateEngineState);

    {
        // Create audio element with URL (like the AudioUrl example sketch)
        fl::JsonAudioImpl audio("Audio",
            fl::url("https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3"));

        // Trigger serialization
        fl::processJsonUiPendingUpdates();

        // Must have captured something
        FL_REQUIRE(!capturedJson.empty());
        FASTLED_WARN("Captured JSON: " << capturedJson.c_str());

        // Parse the JSON array
        fl::Json parsed = fl::Json::parse(capturedJson.c_str());
        FL_REQUIRE(parsed.is_array());
        FL_REQUIRE(parsed.size() >= 1);

        // Find the audio element
        bool foundAudio = false;
        for (size_t i = 0; i < parsed.size(); ++i) {
            fl::Json component = parsed[i];
            fl::string type = component["type"].as_or(fl::string(""));
            if (type == "audio") {
                foundAudio = true;

                // Verify name
                fl::string name = component["name"].as_or(fl::string(""));
                FL_CHECK_EQ(name, fl::string("Audio"));

                // Verify url field is present and correct
                FL_CHECK(component.contains("url"));
                fl::string url = component["url"].as_or(fl::string(""));
                FL_CHECK_EQ(url, fl::string(
                    "https://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3"));

                FASTLED_WARN("Audio URL field present: " << url.c_str());
                break;
            }
        }
        FL_CHECK(foundAudio);
    }
}

FL_TEST_CASE("UIAudio without URL does not emit url field") {
    fl::string capturedJson;
    auto updateEngineState = fl::setJsonUiHandlers(
        [&](const char* jsonStr) {
            capturedJson = jsonStr;
        }
    );
    FL_REQUIRE(updateEngineState);

    {
        // Create audio element WITHOUT URL (original constructor)
        fl::JsonAudioImpl audio("Audio");

        fl::processJsonUiPendingUpdates();
        FL_REQUIRE(!capturedJson.empty());

        fl::Json parsed = fl::Json::parse(capturedJson.c_str());
        FL_REQUIRE(parsed.is_array());
        FL_REQUIRE(parsed.size() >= 1);

        for (size_t i = 0; i < parsed.size(); ++i) {
            fl::Json component = parsed[i];
            fl::string type = component["type"].as_or(fl::string(""));
            if (type == "audio") {
                // Verify url field is NOT present when no URL given
                FL_CHECK_FALSE(component.contains("url"));
                FASTLED_WARN("Audio element without URL - no url field (correct)");
                break;
            }
        }
    }
}

FL_TEST_CASE("JsonUiAudioInternal URL constructor stores url") {
    // Direct test of the internal class
    fl::JsonUiAudioInternal internal("TestAudio",
        fl::url("https://example.com/test.mp3"));

    FL_CHECK_EQ(internal.url(), fl::url("https://example.com/test.mp3"));

    // Serialize and verify
    fl::Json json = fl::Json::createObject();
    internal.toJson(json);

    FL_CHECK(json.contains("url"));
    fl::string url = json["url"].as_or(fl::string(""));
    FL_CHECK_EQ(url, fl::string("https://example.com/test.mp3"));

    fl::string type = json["type"].as_or(fl::string(""));
    FL_CHECK_EQ(type, fl::string("audio"));

    fl::string name = json["name"].as_or(fl::string(""));
    FL_CHECK_EQ(name, fl::string("TestAudio"));

    FASTLED_WARN("JsonUiAudioInternal toJson: " << json.to_string().c_str());
}

FL_TEST_CASE("JsonUiAudioInternal without URL has empty url") {
    fl::JsonUiAudioInternal internal("TestAudio");

    FL_CHECK_FALSE(internal.url().isValid());

    fl::Json json = fl::Json::createObject();
    internal.toJson(json);

    // url field should NOT be present when URL is empty
    FL_CHECK_FALSE(json.contains("url"));
}

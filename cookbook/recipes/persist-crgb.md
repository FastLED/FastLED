# Save and restore a CRGB array

FastLED can convert a pixel buffer to versioned JSON and validate it when
loading. The storage operation itself is platform-specific: use LittleFS on an
ESP32, an SD card, EEPROM, or another store appropriate for your board.

```cpp
#include <FastLED.h>
#include <LittleFS.h>
#include "fl/gfx/crgb_json.h"

constexpr int NUM_LEDS = 60;
CRGB leds[NUM_LEDS];

bool savePixels() {
    const fl::optional<fl::json> document = fl::crgbToJson(leds);
    if (!document) {
        return false;
    }

    const fl::string text = document->to_string();
    CRGB verified[NUM_LEDS];
    if (!fl::crgbFromJson(fl::json::parse(text), verified)) {
        return false;
    }

    File file = LittleFS.open("/leds.json.tmp", "w");
    if (!file) {
        return false;
    }

    const size_t written = file.print(text.c_str());
    file.close();
    if (written != text.size()) {
        LittleFS.remove("/leds.json.tmp");
        return false;
    }
    if (!LittleFS.rename("/leds.json.tmp", "/leds.json")) {
        LittleFS.remove("/leds.json.tmp");
        return false;
    }
    return true;
}

bool loadPixels() {
    File file = LittleFS.open("/leds.json", "r");
    if (!file) {
        return false;
    }

    fl::string text;
    text.reserve(file.size());
    while (file.available()) {
        text.push_back(static_cast<char>(file.read()));
    }
    file.close();

    return fl::crgbFromJson(fl::json::parse(text), leds);
}
```

Mount LittleFS once during `setup()` before calling either function. The loader
requires the saved pixel count to match `NUM_LEDS`, checks every RGB channel,
and leaves `leds` unchanged if the file is malformed or incompatible. Saving
also validates the serialized text before it writes a temporary file, then
renames that complete file over the previous save.

JSON is convenient and human-readable, but it is larger than the three raw
bytes stored by each `CRGB`. For large installations, store the raw bytes with
a small version/checksum header instead. Avoid writing from `loop()` on every
frame; flash and EEPROM have finite write endurance, so save only when the
pattern actually changes.

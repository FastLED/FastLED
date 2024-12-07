/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED Web
 * to map the 1D strip to a 2D grid. Note that the strip can have arbitrary
 * size. this was first motivated during the (attempted? Oct. 19th 2024) port of
 * the Chromancer project to FastLED Web.
 */

#include "fl/screenmap.h"

#include "fl/str.h"
#include "fl/map.h"
#include "fl/json.h"
#include "namespace.h"
#include "fl/vector.h"
#include "fl/warn.h"
#include "fl/math_macros.h"
#include "math.h"

using namespace fl;



FASTLED_NAMESPACE_BEGIN

ScreenMap ScreenMap::Circle(int numLeds, float cm_between_leds, float cm_led_diameter) {
    ScreenMap screenMap = ScreenMap(numLeds);
    float circumference = numLeds * cm_between_leds;
    float radius = circumference / (2 * PI);

    for (int i = 0; i < numLeds; i++) {
        float angle = i * 2 * PI / numLeds;
        float x = radius * cos(angle) * 2;
        float y = radius * sin(angle) * 2;
        screenMap[i] = {x, y};
    }
    screenMap.setDiameter(cm_led_diameter);
    return screenMap;
}


bool ScreenMap::ParseJson(const char *jsonStrScreenMap,
                          fl::FixedMap<Str, ScreenMap, 16> *segmentMaps,
                          fl::Str *err) {
    fl::JsonDocument doc;
    fl::Str _err;
    if (!err) {
        err = &_err;
    }

    bool ok = fl::parseJson(jsonStrScreenMap, &doc, err);
    if (!ok) {
        FASTLED_WARN("Failed to parse json: " << err->c_str());
        return false;
    }
    auto map = doc["map"];
    for (auto kv : map.as<FLArduinoJson::JsonObject>()) {
        auto segment = kv.value();
        auto x = segment["x"];
        auto y = segment["y"];
        auto obj = segment["diameter"];
        float diameter = -1.0f;
        if (obj.is<float>()) {
            float d = obj.as<float>();
            if (d > 0.0f) {
                diameter = d;
            }
        }
        auto n = x.size();
        ScreenMap segment_map(n, diameter);
        for (uint16_t j = 0; j < n; j++) {
            segment_map.set(j, pair_xy_float{x[j], y[j]});
        }
        segmentMaps->insert(kv.key().c_str(), segment_map);
    }
    return true;
}

bool ScreenMap::ParseJson(const char *jsonStrScreenMap,
                          const char* screenMapName,
                          ScreenMap *screenmap,
                          fl::Str *err) {
    fl::FixedMap<fl::Str, ScreenMap, 16> segmentMaps;
    bool ok = ParseJson(jsonStrScreenMap, &segmentMaps, err);
    if (!ok) {
        return false;
    }
    if (segmentMaps.size() == 0) {
        return false;
    }
    if (segmentMaps.has(screenMapName)) {
        *screenmap = segmentMaps[screenMapName];
        return true;
    }
    fl::Str _err = "ScreenMap not found: ";
    _err.append(screenMapName);
    if (err) {
        *err = _err;
    }
    FASTLED_WARN(_err.c_str());
    return false;
}

void ScreenMap::toJson(const fl::FixedMap<Str, ScreenMap, 16>& segmentMaps, FLArduinoJson::JsonDocument* _doc) {
    auto& doc = *_doc;
    auto map = doc["map"].to<FLArduinoJson::JsonObject>();
    for (auto kv : segmentMaps) {
        auto segment = map[kv.first].to<FLArduinoJson::JsonObject>();
        auto x_array = segment["x"].to<FLArduinoJson::JsonArray>();
        auto y_array = segment["y"].to<FLArduinoJson::JsonArray>();
        for (uint16_t i = 0; i < kv.second.getLength(); i++) {
            const pair_xy_float& xy = kv.second[i];
            x_array.add(xy.x);
            y_array.add(xy.y);
        }
        float diameter = kv.second.getDiameter();
        if (diameter < 0.0f) {
            diameter = .5f; // 5mm.
        }
        if (diameter > 0.0f) {
            segment["diameter"] = diameter;
        }
    }
}

void ScreenMap::toJsonStr(const fl::FixedMap<Str, ScreenMap, 16>& segmentMaps, Str* jsonBuffer) {
    FLArduinoJson::JsonDocument doc;
    toJson(segmentMaps, &doc);
    FLArduinoJson::serializeJson(doc, *jsonBuffer);
}



FASTLED_NAMESPACE_END

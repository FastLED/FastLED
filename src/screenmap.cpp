/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED.js
 * to map the 1D strip to a 2D grid. Note that the strip can have arbitrary
 * size. this was first motivated during the (attempted? Oct. 19th 2024) port of
 * the Chromancer project to FastLED.js.
 */

#include "screenmap.h"

#include "fixed_map.h"
#include "third_party/arduinojson/json.h"
#include "namespace.h"
#include "str.h"
#include "fixed_vector.h"

FASTLED_NAMESPACE_BEGIN

void ScreenMap::ParseJson(const char *jsonStrOfMapFile,
                          FixedMap<String, ScreenMap, 16> *segmentMaps) {
    ArduinoJson::JsonDocument doc;
    ArduinoJson::deserializeJson(doc, jsonStrOfMapFile);
    auto map = doc["map"];
    for (auto kv : map.as<ArduinoJson::JsonObject>()) {
        auto segment = kv.value();
        auto x = segment["x"];
        auto y = segment["y"];
        auto n = x.size();
        ScreenMap segment_map(n);
        for (uint16_t j = 0; j < n; j++) {
            segment_map.set(j, pair_xy_float{x[j], y[j]});
        }
        segmentMaps->insert(kv.key().c_str(), segment_map);
    }
}

FASTLED_NAMESPACE_END
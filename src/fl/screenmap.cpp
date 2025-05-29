/* Screenmap maps strip indexes to x,y coordinates. This is used for FastLED Web
 * to map the 1D strip to a 2D grid. Note that the strip can have arbitrary
 * size. this was first motivated during the (attempted? Oct. 19th 2024) port of
 * the Chromancer project to FastLED Web.
 */

#include "fl/screenmap.h"

#include "fl/json.h"
#include "fl/map.h"
#include "fl/math.h"
#include "fl/math_macros.h"
#include "fl/namespace.h"
#include "fl/screenmap.h"
#include "fl/str.h"
#include "fl/vector.h"
#include "fl/warn.h"

namespace fl {

ScreenMap ScreenMap::Circle(int numLeds, float cm_between_leds,
                            float cm_led_diameter, float completion) {
    ScreenMap screenMap(numLeds);

    // radius from LED spacing
    float circumference = numLeds * cm_between_leds;
    float radius = circumference / (2 * PI);

    // how big an arc we light vs leave dark
    float totalAngle = completion * 2 * PI;
    float gapAngle = 2 * PI - totalAngle;

    // shift so the dark gap is centered at the bottom (–π/2)
    float startAngle = -PI / 2 + gapAngle / 2.0f;

    // if partial, land last LED exactly at startAngle+totalAngle
    float divisor =
        (completion < 1.0f && numLeds > 1) ? (numLeds - 1) : numLeds;

    for (int i = 0; i < numLeds; ++i) {
        float angle = startAngle + (i * totalAngle) / divisor;
        float x = radius * cos(angle) * 2;
        float y = radius * sin(angle) * 2;
        screenMap[i] = {x, y};
    }

    screenMap.setDiameter(cm_led_diameter);
    return screenMap;
}

bool ScreenMap::ParseJson(const char *jsonStrScreenMap,
                          FixedMap<Str, ScreenMap, 16> *segmentMaps, Str *err) {
#if !FASTLED_ENABLE_JSON
    if (err) {
        *err = "JSON not enabled";
    }
    return false;
#else
    JsonDocument doc;
    Str _err;
    if (!err) {
        err = &_err;
    }

    bool ok = parseJson(jsonStrScreenMap, &doc, err);
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
            segment_map.set(j, vec2f{x[j], y[j]});
        }
        segmentMaps->insert(kv.key().c_str(), segment_map);
    }
    return true;
#endif
}

bool ScreenMap::ParseJson(const char *jsonStrScreenMap,
                          const char *screenMapName, ScreenMap *screenmap,
                          Str *err) {
#if !FASTLED_ENABLE_JSON
    if (err) {
        *err = "JSON not enabled";
    }
    return false;
#else
    FixedMap<Str, ScreenMap, 16> segmentMaps;
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
    Str _err = "ScreenMap not found: ";
    _err.append(screenMapName);
    if (err) {
        *err = _err;
    }
    FASTLED_WARN(_err.c_str());
    return false;
#endif
}

void ScreenMap::toJson(const FixedMap<Str, ScreenMap, 16> &segmentMaps,
                       JsonDocument *_doc) {

#if !FASTLED_ENABLE_JSON
    return;
#else
    if (!_doc) {
        FASTLED_WARN("ScreenMap::toJson called with nullptr _doc");
        return;
    }
    auto &doc = *_doc;
    auto map = doc["map"].to<FLArduinoJson::JsonObject>();
    if (!segmentMaps.empty()) {
        for (auto kv : segmentMaps) {
            auto segment = map[kv.first].to<FLArduinoJson::JsonObject>();
            auto x_array = segment["x"].to<FLArduinoJson::JsonArray>();
            auto y_array = segment["y"].to<FLArduinoJson::JsonArray>();
            for (uint16_t i = 0; i < kv.second.getLength(); i++) {
                const vec2f &xy = kv.second[i];
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

#endif
}

void ScreenMap::toJsonStr(const FixedMap<Str, ScreenMap, 16> &segmentMaps,
                          Str *jsonBuffer) {
#if !FASTLED_ENABLE_JSON
    return;
#else
    JsonDocument doc;
    toJson(segmentMaps, &doc);
    fl::toJson(doc, jsonBuffer);
#endif
}

ScreenMap::ScreenMap(uint32_t length, float mDiameter)
    : length(length), mDiameter(mDiameter) {
    mLookUpTable = LUTXYFLOATPtr::New(length);
    LUTXYFLOAT &lut = *mLookUpTable.get();
    vec2f *data = lut.getDataMutable();
    for (uint32_t x = 0; x < length; x++) {
        data[x] = {0, 0};
    }
}

ScreenMap::ScreenMap(const vec2f *lut, uint32_t length, float diameter)
    : length(length), mDiameter(diameter) {
    mLookUpTable = LUTXYFLOATPtr::New(length);
    LUTXYFLOAT &lut16xy = *mLookUpTable.get();
    vec2f *data = lut16xy.getDataMutable();
    for (uint32_t x = 0; x < length; x++) {
        data[x] = lut[x];
    }
}

ScreenMap::ScreenMap(const ScreenMap &other) {
    mDiameter = other.mDiameter;
    length = other.length;
    mLookUpTable = other.mLookUpTable;
}

void ScreenMap::set(uint16_t index, const vec2f &p) {
    if (mLookUpTable) {
        LUTXYFLOAT &lut = *mLookUpTable.get();
        auto *data = lut.getDataMutable();
        data[index] = p;
    }
}

void ScreenMap::setDiameter(float diameter) { mDiameter = diameter; }

vec2f ScreenMap::mapToIndex(uint32_t x) const {
    if (x >= length || !mLookUpTable) {
        return {0, 0};
    }
    LUTXYFLOAT &lut = *mLookUpTable.get();
    vec2f screen_coords = lut[x];
    return screen_coords;
}

uint32_t ScreenMap::getLength() const { return length; }

float ScreenMap::getDiameter() const { return mDiameter; }

vec2f ScreenMap::getBounds() const {

    if (length == 0 || !mLookUpTable) {
        return {0, 0};
    }

    LUTXYFLOAT &lut = *mLookUpTable.get();

    fl::vec2f *data = lut.getDataMutable();
    // float minX = lut[0].x;
    // float maxX = lut[0].x;
    // float minY = lut[0].y;
    // float maxY = lut[0].y;
    float minX = data[0].x;
    float maxX = data[0].x;
    float minY = data[0].y;
    float maxY = data[0].y;

    for (uint32_t i = 1; i < length; i++) {
        const vec2f &p = lut[i];
        minX = MIN(minX, p.x);
        maxX = MAX(maxX, p.x);
        minY = MIN(minY, p.y);
        maxY = MAX(maxY, p.y);
    }

    return {maxX - minX, maxY - minY};
}

const vec2f &ScreenMap::empty() {
    static const vec2f s_empty = vec2f(0, 0);
    return s_empty;
}

const vec2f &ScreenMap::operator[](uint32_t x) const {
    if (x >= length || !mLookUpTable) {
        return empty(); // better than crashing.
    }
    LUTXYFLOAT &lut = *mLookUpTable.get();
    return lut[x];
}

vec2f &ScreenMap::operator[](uint32_t x) {
    if (x >= length || !mLookUpTable) {
        return const_cast<vec2f &>(empty()); // better than crashing.
    }
    LUTXYFLOAT &lut = *mLookUpTable.get();
    auto *data = lut.getDataMutable();
    return data[x];
}

ScreenMap &ScreenMap::operator=(const ScreenMap &other) {
    if (this != &other) {
        mDiameter = other.mDiameter;
        length = other.length;
        mLookUpTable = other.mLookUpTable;
    }
    return *this;
}

void ScreenMap::addOffset(const vec2f &p) {
    vec2f *data = mLookUpTable->getDataMutable();
    for (uint32_t i = 0; i < length; i++) {
        vec2f &curr = data[i];
        curr.x += p.x;
        curr.y += p.y;
    }
}

void ScreenMap::addOffsetX(float x) { addOffset({x, 0}); }
void ScreenMap::addOffsetY(float y) { addOffset({0, y}); }

} // namespace fl

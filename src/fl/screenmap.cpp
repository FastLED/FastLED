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

// Helper function to extract a vector of floats from a JSON array
fl::vector<float> jsonArrayToFloatVector(const fl::json2::Json& jsonArray) {
    fl::vector<float> result;
    
    if (!jsonArray.has_value() || !jsonArray.is_array()) {
        return result;
    }
    
    auto arrayOpt = jsonArray.as_array();
    if (!arrayOpt) {
        return result;
    }
    
    for (const auto& item : *arrayOpt) {
        // Check that item is not null before creating Json object
        if (!item) {
            continue;
        }
        
        // Create Json object from shared_ptr
        fl::json2::Json itemJson(item);
        if (!itemJson.has_value()) {
            continue;
        }
        
        // Try to get as double first (more common for JSON numbers)
        auto doubleVal = itemJson.as_double();
        if (doubleVal) {
            result.push_back(static_cast<float>(*doubleVal));
            continue;
        }
        
        // If that fails, try as int
        auto intVal = itemJson.as_int();
        if (intVal) {
            result.push_back(static_cast<float>(*intVal));
            continue;
        }
    }
    
    return result;
}

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
                          fl::fl_map<string, ScreenMap> *segmentMaps, string *err) {

    //FL_WARN_SCREENMAP("ParseJson called with JSON: " << jsonStrScreenMap);
    
    string _err;
    if (!err) {
        err = &_err;
    }

    auto jsonDoc = fl::json2::Json::parse(jsonStrScreenMap);
    if (!jsonDoc.has_value()) {
        *err = "Failed to parse JSON";
        FL_WARN("Failed to parse JSON");
        return false;
    }
    
    if (!jsonDoc.is_object()) {
        *err = "JSON root is not an object";
        FL_WARN("JSON root is not an object");
        return false;
    }
    
    // Check if "map" key exists and is an object
    if (!jsonDoc.contains("map")) {
        *err = "Missing 'map' key in JSON";
        FL_WARN("Missing 'map' key in JSON");
        return false;
    }
    
    // Make sure we're working with a valid object
    if (!jsonDoc["map"].has_value() || !jsonDoc["map"].is_object()) {
        *err = "Invalid 'map' object in JSON";
        FL_WARN("Invalid 'map' object in JSON");
        return false;
    }
    
    auto jsonMapOpt = jsonDoc["map"].as_object();
    if (!jsonMapOpt || jsonMapOpt->empty()) {
        *err = "Failed to parse map from JSON or map is empty";
        FL_WARN("Failed to parse map from JSON or map is empty");
        return false;
    }
    
    auto& jsonMap = *jsonMapOpt;

    
    for (const auto& kv : jsonMap) {
        auto name = kv.first;

        
        // Check that the value is not null before creating Json object
        if (!kv.second) {
            *err = "Null value for segment " + name;
            return false;
        }
        
        // Create Json object directly from shared_ptr
        fl::json2::Json val(kv.second);
        if (!val.has_value()) {
            *err = "Invalid value for segment " + name;
            return false;
        }
        
        if (!val.is_object()) {
            *err = "Segment value for " + name + " is not an object";
            return false;
        }
        
        // Check if x array exists and is actually an array
        if (!val.contains("x")) {
            *err = "Missing x array for " + name;
            return false;
        }
        
        if (!val["x"].has_value() || !val["x"].is_array()) {
            *err = "Invalid x array for " + name;
            return false;
        }
        
        // Extract x array using our helper function
        fl::vector<float> x_array = jsonArrayToFloatVector(val["x"]);
        
        // Check if y array exists and is actually an array
        if (!val.contains("y")) {
            *err = "Missing y array for " + name;
            return false;
        }
        
        if (!val["y"].has_value() || !val["y"].is_array()) {
            *err = "Invalid y array for " + name;
            return false;
        }
        
        // Extract y array using our helper function
        fl::vector<float> y_array = jsonArrayToFloatVector(val["y"]);
        
        // Get diameter (optional) with default value
        float diameter = -1.0f; // default value
        if (val.contains("diameter") && val["diameter"].has_value()) {
            auto diameterOpt = val["diameter"].as_double();
            if (diameterOpt) {
                diameter = static_cast<float>(*diameterOpt);
            }
        }

        auto n = MIN(x_array.size(), y_array.size());
        if (n != x_array.size() || n != y_array.size()) {
            if (n != x_array.size()) {
            }
            if (n != y_array.size()) {
            }
        }

        ScreenMap segment_map(n, diameter);
        for (size_t i = 0; i < n; i++) {
            segment_map.set(i, vec2f{x_array[i], y_array[i]});
        }
        (*segmentMaps)[name] = segment_map;
    }
    return true;
}

bool ScreenMap::ParseJson(const char *jsonStrScreenMap,
                          const char *screenMapName, ScreenMap *screenmap,
                          string *err) {

    fl::fl_map<string, ScreenMap> segmentMaps;
    bool ok = ParseJson(jsonStrScreenMap, &segmentMaps, err);
    if (!ok) {
        return false;
    }
    if (segmentMaps.size() == 0) {
        return false;
    }
    if (segmentMaps.contains(screenMapName)) {
        *screenmap = segmentMaps[screenMapName];
        return true;
    }
    string _err = "ScreenMap not found: ";
    _err.append(screenMapName);
    if (err) {
        *err = _err;
    }
    
    return false;
}

void ScreenMap::toJson(const fl::fl_map<string, ScreenMap> &segmentMaps,
                       fl::json2::Json *doc) {

    if (!doc) {
        FL_WARN("ScreenMap::toJson called with nullptr doc");
        return;
    }

    // Build the JSON string manually to avoid shared_ptr issues
    fl::string jsonStr = "{\"map\":{";
    
    bool firstSegment = true;
    for (const auto& kv : segmentMaps) {
        if (kv.second.getLength() == 0) {
            FL_WARN("ScreenMap::toJson called with empty segment: " << fl::string(kv.first));   
            continue;
        }
        
        if (!firstSegment) {
            jsonStr += ",";
        }
        
        auto& name = kv.first;
        auto& segment = kv.second;
        float diameter = segment.getDiameter();
        
        // Escape the name for JSON
        jsonStr += "\"" + name + "\":{";
        
        // Add x array
        jsonStr += "\"x\":[";
        bool firstX = true;
        for (u16 i = 0; i < segment.getLength(); i++) {
            if (!firstX) {
                jsonStr += ",";
            }
            jsonStr += fl::to_string(segment[i].x);
            firstX = false;
        }
        jsonStr += "],";
        
        // Add y array
        jsonStr += "\"y\":[";
        bool firstY = true;
        for (u16 i = 0; i < segment.getLength(); i++) {
            if (!firstY) {
                jsonStr += ",";
            }
            jsonStr += fl::to_string(segment[i].y);
            firstY = false;
        }
        jsonStr += "],";
        
        // Add diameter
        jsonStr += "\"diameter\":" + fl::to_string(diameter);
        
        jsonStr += "}";
        firstSegment = false;
    }
    
    jsonStr += "}}";

    


    FL_WARN("ScreenMap::toJson: " << jsonStr);
    FASTLED_ASSERT(false, "force debug");
    
    // Parse the manually built string
    *doc = fl::json2::Json::parse(jsonStr);
}

void ScreenMap::toJsonStr(const fl::fl_map<string, ScreenMap> &segmentMaps,
                          string *jsonBuffer) {
    fl::json2::Json doc;
    toJson(segmentMaps, &doc);
    *jsonBuffer = doc.to_string();
}

ScreenMap::ScreenMap(u32 length, float mDiameter)
    : length(length), mDiameter(mDiameter) {
    mLookUpTable = fl::make_shared<LUTXYFLOAT>(length);
    LUTXYFLOAT &lut = *mLookUpTable.get();
    vec2f *data = lut.getDataMutable();
    for (u32 x = 0; x < length; x++) {
        data[x] = {0, 0};
    }
}

ScreenMap::ScreenMap(const vec2f *lut, u32 length, float diameter)
    : length(length), mDiameter(diameter) {
    mLookUpTable = fl::make_shared<LUTXYFLOAT>(length);
    LUTXYFLOAT &lut16xy = *mLookUpTable.get();
    vec2f *data = lut16xy.getDataMutable();
    for (u32 x = 0; x < length; x++) {
        data[x] = lut[x];
    }
}

ScreenMap::ScreenMap(const ScreenMap &other) {
    mDiameter = other.mDiameter;
    length = other.length;
    mLookUpTable = other.mLookUpTable;
}

ScreenMap::ScreenMap(ScreenMap&& other) {
    mDiameter = other.mDiameter;
    length = other.length;
    fl::swap(mLookUpTable, other.mLookUpTable);
    other.mLookUpTable.reset();
}

void ScreenMap::set(u16 index, const vec2f &p) {
    if (mLookUpTable) {
        LUTXYFLOAT &lut = *mLookUpTable.get();
        auto *data = lut.getDataMutable();
        data[index] = p;
    }
}

void ScreenMap::setDiameter(float diameter) { mDiameter = diameter; }

vec2f ScreenMap::mapToIndex(u32 x) const {
    if (x >= length || !mLookUpTable) {
        return {0, 0};
    }
    LUTXYFLOAT &lut = *mLookUpTable.get();
    vec2f screen_coords = lut[x];
    return screen_coords;
}

u32 ScreenMap::getLength() const { return length; }

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

    for (u32 i = 1; i < length; i++) {
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

const vec2f &ScreenMap::operator[](u32 x) const {
    if (x >= length || !mLookUpTable) {
        return empty(); // better than crashing.
    }
    LUTXYFLOAT &lut = *mLookUpTable.get();
    return lut[x];
}

vec2f &ScreenMap::operator[](u32 x) {
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
    for (u32 i = 0; i < length; i++) {
        vec2f &curr = data[i];
        curr.x += p.x;
        curr.y += p.y;
    }
}

void ScreenMap::addOffsetX(float x) { addOffset({x, 0}); }
void ScreenMap::addOffsetY(float y) { addOffset({0, y}); }

} // namespace fl

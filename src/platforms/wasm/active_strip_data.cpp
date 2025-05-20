#include <memory>
#include <stdio.h>

#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/slice.h"

#include "active_strip_data.h"
#include "fl/map.h"
#include "fl/namespace.h"
#include "fl/str.h"
#include "js.h"
#include "platforms/wasm/engine_listener.h"

using namespace fl;

FASTLED_NAMESPACE_BEGIN

ActiveStripData &ActiveStripData::Instance() {
    return fl::Singleton<ActiveStripData>::instance();
}

void ActiveStripData::update(int id, uint32_t now, const uint8_t *pixel_data,
                             size_t size) {
    mStripMap.update(id, SliceUint8(pixel_data, size));
}

void ActiveStripData::updateScreenMap(int id, const ScreenMap &screenmap) {
    mScreenMap.update(id, screenmap);
}

const uint8_t* ActiveStripData::getPixelData_Uint8_C(int stripIndex) {
    SliceUint8 stripData;
    if (mStripMap.get(stripIndex, &stripData)) {
        return stripData.data();
    }
    return nullptr;
}

size_t ActiveStripData::getPixelDataSize_C(int stripIndex) {
    SliceUint8 stripData;
    if (mStripMap.get(stripIndex, &stripData)) {
        return stripData.size();
    }
    return 0;
}

Str ActiveStripData::infoJsonString() {
    FLArduinoJson::JsonDocument doc;
    auto array = doc.to<FLArduinoJson::JsonArray>();

    for (const auto &[stripIndex, stripData] : mStripMap) {
        auto obj = array.add<FLArduinoJson::JsonObject>();
        obj["strip_id"] = stripIndex;
        obj["type"] = "r8g8b8";
    }

    Str jsonBuffer;
    serializeJson(doc, jsonBuffer);
    return jsonBuffer;
}

// gcc constructor to get the
// ActiveStripData instance created.
__attribute__((constructor)) void __init_ActiveStripData() {
    ActiveStripData::Instance();
}

FASTLED_NAMESPACE_END

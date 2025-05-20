#ifdef __EMSCRIPTEN__

#include <memory>
#include <stdio.h>

#include "fl/map.h"
#include "fl/singleton.h"
#include "fl/slice.h"

#include "fl/map.h"
#include "fl/namespace.h"
#include "fl/str.h"

#include "active_strip_data2.h"
#include "js.h"
#include "platforms/wasm/engine_listener.h"

using namespace fl;

namespace fl {

ActiveStripData2 &ActiveStripData2::Instance() {
    return fl::Singleton<ActiveStripData2>::instance();
}

void ActiveStripData2::update(int id, uint32_t now, const uint8_t *pixel_data,
                              size_t size) {
    mStripMap.update(id, SliceUint8(pixel_data, size));
}

void ActiveStripData2::updateScreenMap(int id, const ScreenMap &screenmap) {
    mScreenMap.update(id, screenmap);
}

const uint8_t *ActiveStripData2::getPixelData_Uint8_C(int stripIndex) {
    SliceUint8 stripData;
    if (mStripMap.get(stripIndex, &stripData)) {
        return stripData.data();
    }
    return nullptr;
}

size_t ActiveStripData2::getPixelDataSize_C(int stripIndex) {
    SliceUint8 stripData;
    if (mStripMap.get(stripIndex, &stripData)) {
        return stripData.size();
    }
    return 0;
}

Str ActiveStripData2::infoJsonString() {
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

// // gcc constructor to get the
// // ActiveStripData2 instance created.
// __attribute__((constructor)) void __init_ActiveStripData2() {
//     ActiveStripData2::Instance();
// }

} // namespace fl

#endif // __EMSCRIPTEN__

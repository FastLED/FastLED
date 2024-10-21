#pragma once

#include <memory>

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/val.h>

#include "fixed_map.h"
#include "singleton.h"
#include "slice.h"

#include "engine_events.h"
#include "fixed_map.h"
#include "js.h"
#include "namespace.h"

FASTLED_NAMESPACE_BEGIN

emscripten::val ActiveStripData::getPixelData_Uint8(int stripIndex) {
    // Efficient, zero copy conversion from internal data to JavaScript.
    SliceUint8 stripData;
    if (mStripMap.get(stripIndex, &stripData)) {
        const uint8_t *data = stripData.data();
        uint8_t *data_mutable = const_cast<uint8_t *>(data);
        size_t size = stripData.size();
        return emscripten::val(
            emscripten::typed_memory_view(size, data_mutable));
    }
    return emscripten::val::undefined();
}

std::string ActiveStripData::infoJson() {
    ArduinoJson::JsonDocument doc;
    auto array = doc.to<ArduinoJson::JsonArray>();

    for (const auto &[stripIndex, stripData] : mStripMap) {
        auto nestedObject = array.createNestedObject();
        nestedObject["strip_id"] = stripIndex;
    }

    std::string jsonBuffer;
    serializeJson(doc, jsonBuffer);
    return jsonBuffer;
}

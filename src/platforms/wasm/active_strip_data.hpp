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

ActiveStripData& ActiveStripData::Instance() {
    return Singleton<ActiveStripData>::instance();
}

void ActiveStripData::update(int id, uint32_t now, const uint8_t* data, size_t size) {
    mStripMap.update(id, SliceUint8(data, size));
}

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

emscripten::val ActiveStripData::getFirstPixelData_Uint8() {
    // Efficient, zero copy conversion from internal data to JavaScript.
    if (!mStripMap.empty()) {
        SliceUint8 stripData = mStripMap.begin()->second;
        const uint8_t* data = stripData.data();
        uint8_t* data_mutable = const_cast<uint8_t*>(data);
        size_t size = stripData.size();
        return emscripten::val(emscripten::typed_memory_view(size, data_mutable));
    }
    return emscripten::val::undefined();
}

emscripten::val ActiveStripData::getNthPixelStripData_Uint8(int n) {
    // Efficient, zero copy conversion from internal data to JavaScript.
    if (mStripMap.size() > n) {
        auto it = mStripMap.begin();
        for (int i = 0; i < n && it != mStripMap.end(); ++i, ++it) {}
        if (it != mStripMap.end()) {
            SliceUint8 stripData = it->second;
            const uint8_t* data = stripData.data();
            uint8_t* data_mutable = const_cast<uint8_t*>(data);
            size_t size = stripData.size();
            return emscripten::val(emscripten::typed_memory_view(size, data_mutable));
        }
    }
    return emscripten::val::undefined();
}

std::string ActiveStripData::infoJsonString() {
    ArduinoJson::JsonDocument doc;
    auto array = doc.to<ArduinoJson::JsonArray>();

    for (const auto &[stripIndex, stripData] : mStripMap) {
        auto obj = array.add<ArduinoJson::JsonObject>();
        obj["strip_id"] = stripIndex;
    }

    std::string jsonBuffer;
    serializeJson(doc, jsonBuffer);
    return jsonBuffer;
}

static ActiveStripData* getActiveStripDataPtr() {
    ActiveStripData* instance = &Singleton<ActiveStripData>::instance();
    return instance;
}

EMSCRIPTEN_BINDINGS(engine_events_constructors) {
    emscripten::class_<ActiveStripData>("ActiveStripData")
        .constructor(&getActiveStripDataPtr, emscripten::allow_raw_pointers())
        .function("getPixelData_Uint8", &ActiveStripData::getPixelData_Uint8)
        .function("getFirstPixelData_Uint8", &ActiveStripData::getFirstPixelData_Uint8)
        .function("getNthPixelStripData_Uint8", &ActiveStripData::getNthPixelStripData_Uint8);
}


#pragma once



#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

#include <sstream>

// emscripten headers
#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>



#include "xymap.h"
#include "json.h"

void jsSetCanvasSize(int cledcontoller_id, const XYMap& xymap) {
    int width = xymap.getWidth();
    int height = xymap.getHeight();
    std::ostringstream oss;
    JsonDictEncoder encoder;
    oss << "[";
    encoder.begin(&oss);
    encoder.addField("id", cledcontoller_id);
    encoder.addField("width", width);
    encoder.addField("height", height);
    encoder.end();
    oss << "]";
    std::string jsonStr = oss.str();
    EM_ASM_({
        globalThis.onFastLedSetCanvasSize = globalThis.onFastLedSetCanvasSize || function(jsonStr) {
            console.log("Missing globalThis.onFastLedSetCanvasSize(jsonStr) function");
        };
        var jsonStr = UTF8ToString($0);  // Convert C string to JavaScript string
        globalThis.onFastLedSetCanvasSize(jsonStr);
    }, jsonStr.c_str());
}

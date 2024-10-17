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
    JsonDictEncoder encoder;
    encoder.begin();
    encoder.addField("strip_id", cledcontoller_id);
    encoder.addField("width", width);
    encoder.addField("height", height);
    encoder.end();
    EM_ASM_({
        globalThis.onFastLedSetCanvasSize = globalThis.onFastLedSetCanvasSize || function(jsonStr) {
            console.log("Missing globalThis.onFastLedSetCanvasSize(jsonStr) function");
        };
        var jsonStr = UTF8ToString($0);  // Convert C string to JavaScript string
        var jsonData = JSON.parse(jsonStr);
        globalThis.onFastLedSetCanvasSize(jsonData);
    }, encoder.c_str());
}

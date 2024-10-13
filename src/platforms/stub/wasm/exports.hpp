#pragma once



#ifndef __EMSCRIPTEN__
#error "This file should only be included in an Emscripten build"
#endif

/// Begin compatibility layer for FastLED platform. WebAssembly edition.

// emscripten headers
#include <emscripten.h>
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>


#include <iostream> // ok include
#include <deque>  // ok include
#include <string> // ok include
#include <stdio.h>
#include <thread>

#include "exports.h"
#include "channel_data.h"
#include "exports/timer.hpp"
#include "exports/endframe.hpp"
#include "exports/setup_and_loop.hpp"
#include "exports/message_queue.hpp"


void jsSetCanvasSize(int width, int height) {
    char jsonStr[1024];
    snprintf(jsonStr, sizeof(jsonStr), "[{\"width\":%d,\"height\":%d}]", width, height);
    EM_ASM_({
        globalThis.onFastLedSetCanvasSize = globalThis.onFastLedSetCanvasSize || function(jsonStr) {
            console.log("Missing globalThis.onFastLedSetCanvasSize(jsonStr) function");
        };
        var jsonStr = UTF8ToString($0);  // Convert C string to JavaScript string
        globalThis.onFastLedSetCanvasSize(jsonStr);
    }, jsonStr);
}

EMSCRIPTEN_KEEPALIVE extern "C" bool postMessage(const char* jstStr) {
    // post message to the message queue.
    MessageQueue& js_message_queue = MessageQueue::Instance();
    return js_message_queue.pushBack(jstStr);
}

void exports_init() {
    OnEndFrameListener::Init();
}
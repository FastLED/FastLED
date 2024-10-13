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

#include "message_queue.h"

EMSCRIPTEN_KEEPALIVE extern "C" bool postMessage(const char* jstStr) {
    // post message to the message queue.
    MessageQueue& js_message_queue = MessageQueue::Instance();
    return js_message_queue.pushBack(jstStr);
}

void exports_init() {
    OnEndFrameListener::Init();
}
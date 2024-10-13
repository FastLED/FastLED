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
#include "exports/canvas_size.hpp"


EMSCRIPTEN_KEEPALIVE extern "C" bool postMessage(const char* jstStr) {
    // post message to the message queue.
    MessageQueue& js_message_queue = MessageQueue::Instance();
    return js_message_queue.pushBack(jstStr);
}

void exports_init() {
    OnEndFrameListener::Init();
}
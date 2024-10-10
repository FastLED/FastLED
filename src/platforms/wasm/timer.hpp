#pragma once

// clib headers
#include <stdio.h>
#include <thread>

// emscripten headers
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>



// Global variables
bool g_animation_running = true;
uint32_t g_time_offset = 0;
uint32_t g_pause_time = 0;

// Provide missing functions for WebAssembly build.
extern "C" {
    uint32_t real_millis() {
        return emscripten_get_now();
/// Begin compatibility layer for FastLED platform. WebAssembly edition.

// emscripten headers
#include <emscripten/emscripten.h> // Include Emscripten headers
#include <emscripten/html5.h>

#include "platforms/wasm/timer.hpp"

extern void setup();
extern void loop();

// Frame time for 60 fps.
#define SIXTY_FPS 16
static bool g_setup_called = false;

void setup_once() {
    if (g_setup_called) {
        return;
    }
    g_setup_called = true;
    setup();
}



//////////////////////////////////////////////////////////////////////////
// BEGIN EMSCRIPTEN EXPORTS

EMSCRIPTEN_KEEPALIVE extern "C" int extern_setup() {
    setup_once();
    return 0;
}

EMSCRIPTEN_KEEPALIVE extern "C" int extern_loop() {

    setup_once();
    //fastled_resume_timer();
    loop();
    //fastled_pause_timer();
    return 0;
}

#if 0

EMSCRIPTEN_KEEPALIVE extern "C" EM_BOOL on_request_animation_frame_loop(double time, void *userData);

EMSCRIPTEN_KEEPALIVE extern "C" EM_BOOL on_request_animation_frame_loop(double time, void *userData) {
    extern_loop();
    emscripten_request_animation_frame_loop(on_request_animation_frame_loop, 0);
    return true;
}
#endif

void interval_loop(void* userData) {
    extern_loop();
}



EMSCRIPTEN_KEEPALIVE extern "C" void async_start_loop() {
  // Receives a function to call and some user data to provide it.
  //emscripten_request_animation_frame_loop(on_request_animation_frame_loop, 0);
  emscripten_set_interval(interval_loop, SIXTY_FPS, nullptr);
}


EMSCRIPTEN_KEEPALIVE extern "C" int main() {
    printf("Hello from FastLED\r\n");
    async_start_loop();
    return 0;
}

    }

    // Replacement for 'millis' in WebAssembly context
    uint32_t millis() {
        #if 0
        if (g_animation_running) {
            return real_millis() - g_time_offset;
        } else {
            return g_pause_time;
        }
        #else
        real_millis();
        #endif
    }

    // Replacement for 'micros' in WebAssembly context
    uint32_t micros() {
        return millis() * 1000;
    }

    // Replacement for 'delay' in WebAssembly context
    void delay(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }


    // Function to pause the animation and set the pause time
    void fastled_pause_timer() {
        //g_animation_running = false;
        //g_pause_time = millis();
    }

    // Function to resume the animation
    void fastled_resume_timer() {
        //if (!g_animation_running) {
        //    uint32_t current_time = real_millis();
        //    uint32_t elapsed_pause_time = current_time - g_pause_time;
        //    g_time_offset += elapsed_pause_time;
        //    g_animation_running = true;
        //}
    }

    // Function to check if animation is running
    bool is_animation_running() {
        return g_animation_running;
    }
}

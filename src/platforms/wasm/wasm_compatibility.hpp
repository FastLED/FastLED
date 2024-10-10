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

// Standalone sketch runner demonstration
// This shows how external applications can use the FastLED sketch runner interface

#include "fl/stl/string.h" // ok include

// Demo sketch implementation
static int setup_call_count = 0;
static int loop_call_count = 0;

// Arduino-style functions that would be provided by user sketch
void setup() {
    setup_call_count++;
    fl::printf("SKETCH: setup() called (count: %d)\n", setup_call_count);
    fl::printf("SKETCH: Initializing FastLED configuration...\n");
}

void loop() {
    loop_call_count++;
    fl::printf("SKETCH: loop() called (count: %d)\n", loop_call_count);
    fl::printf("SKETCH: Running LED animation frame %d\n", loop_call_count);
}

// Direct declarations for demo (avoiding DLL export complexity)
extern "C" {
    void sketch_setup();
    void sketch_loop();
}

// Simple implementations that call the Arduino functions
void sketch_setup() {
    setup();
}

void sketch_loop() {
    loop();
}

int main() {
    fl::printf("RUNNER: FastLED Sketch Runner Demo\n");
    fl::printf("RUNNER: ================================\n");

    // Initialize sketch (call setup once)
    fl::printf("RUNNER: Initializing sketch...\n");
    sketch_setup();
    fl::printf("RUNNER: Sketch initialization complete\n");
    fl::printf("RUNNER: ================================\n");

    // Run sketch loop five times
    fl::printf("RUNNER: Running sketch loop 5 times...\n");
    for (int i = 1; i <= 5; i++) {
        fl::printf("RUNNER: --- Loop iteration %d ---\n", i);
        sketch_loop();
    }
    
    fl::printf("RUNNER: ================================\n");
    fl::printf("RUNNER: Execution complete\n");
    fl::printf("RUNNER: Final state:\n");
    fl::printf("RUNNER:   setup() called: %d times\n", setup_call_count);
    fl::printf("RUNNER:   loop() called: %d times\n", loop_call_count);
    fl::printf("RUNNER: ================================\n");
    
    return 0;
}

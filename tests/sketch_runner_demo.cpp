// Standalone sketch runner demonstration
// This shows how external applications can use the FastLED sketch runner interface

#include <stdio.h>

// Demo sketch implementation
static int setup_call_count = 0;
static int loop_call_count = 0;

// Arduino-style functions that would be provided by user sketch
void setup() {
    setup_call_count++;
    printf("SKETCH: setup() called (count: %d)\n", setup_call_count);
    printf("SKETCH: Initializing FastLED configuration...\n");
}

void loop() {
    loop_call_count++;
    printf("SKETCH: loop() called (count: %d)\n", loop_call_count);
    printf("SKETCH: Running LED animation frame %d\n", loop_call_count);
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
    printf("RUNNER: FastLED Sketch Runner Demo\n");
    printf("RUNNER: ================================\n");
    
    // Initialize sketch (call setup once)
    printf("RUNNER: Initializing sketch...\n");
    sketch_setup();
    printf("RUNNER: Sketch initialization complete\n");
    printf("RUNNER: ================================\n");
    
    // Run sketch loop five times
    printf("RUNNER: Running sketch loop 5 times...\n");
    for (int i = 1; i <= 5; i++) {
        printf("RUNNER: --- Loop iteration %d ---\n", i);
        sketch_loop();
    }
    
    printf("RUNNER: ================================\n");
    printf("RUNNER: Execution complete\n");
    printf("RUNNER: Final state:\n");
    printf("RUNNER:   setup() called: %d times\n", setup_call_count);
    printf("RUNNER:   loop() called: %d times\n", loop_call_count);
    printf("RUNNER: ================================\n");
    
    return 0;
}

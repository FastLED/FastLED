// Unit tests for sketch runner functionality

#include "test.h"

using namespace fl;

// Test sketch implementation
static int setup_call_count = 0;
static int loop_call_count = 0;
static bool test_mode = false;

// Mock Arduino functions for testing
void setup() {
    if (test_mode) {
        setup_call_count++;
        printf("SKETCH: setup() called (count: %d)\n", setup_call_count);
    }
}

void loop() {
    if (test_mode) {
        loop_call_count++;
        printf("SKETCH: loop() called (count: %d)\n", loop_call_count);
    }
}

// Direct declarations for testing (avoiding DLL export complexity in test context)
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

TEST_CASE("Sketch Runner - Basic Functionality") {
    // Reset counters and enable test mode
    setup_call_count = 0;
    loop_call_count = 0;
    test_mode = true;
    
    printf("RUNNER: Starting sketch runner test\n");
    
    // Call sketch_setup() once
    printf("RUNNER: Calling sketch_setup()\n");
    sketch_setup();
    
    CHECK_EQ(setup_call_count, 1);
    
    // Call sketch_loop() five times
    for (int i = 1; i <= 5; i++) {
        printf("RUNNER: Calling sketch_loop() - iteration %d\n", i);
        sketch_loop();
        CHECK_EQ(loop_call_count, i);
    }
    
    printf("RUNNER: Test completed successfully\n");
    printf("RUNNER: Final state - setup called %d times, loop called %d times\n", 
           setup_call_count, loop_call_count);
    
    // Verify final state
    CHECK_EQ(setup_call_count, 1);
    CHECK_EQ(loop_call_count, 5);
    
    // Disable test mode
    test_mode = false;
}

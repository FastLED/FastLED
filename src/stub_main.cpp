/*
 * This is a stub implementation of main that can be used to include an *.ino
 * file which is so close to C++ that many of them can be compiled as C++. The
 * notable difference between a *.ino file and a *.cpp file is that the *.ino
 * file does not need to include function prototypes, and are instead
 * auto-generated.
*/


#ifdef FASTLED_STUB_MAIN_INCLUDE_INO

#ifndef _FASTLED_STRINGIFY
#define _FASTLED_STRINGIFY_HELPER(x) #x
#define _FASTLED_STRINGIFY(x) _FASTLED_STRINGIFY_HELPER(x)
#endif

// Correctly include the file by expanding and stringifying the macro value
#include _FASTLED_STRINGIFY(FASTLED_STUB_MAIN_INCLUDE_INO)

#include <iostream>

// XY does mapping to a 1D array. Make it weak so that
// the user can override it if they have supplied an example
// in their sketch.
#pragma weak XY
uint16_t XY(uint8_t x, uint8_t y) {
    std::cout << "Warning: XY function not defined. Using stub implementation." << std::endl;
    return 0;
}

int main() {
    // Super simple main function that just calls the setup and loop functions.
    setup();
    while(1) {
        loop();
    }
}
#endif  // FASTLED_STUB_MAIN_INCLUDE_INO
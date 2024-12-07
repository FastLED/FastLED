/*
 * This is a stub implementation of main that can be used to include an *.ino
 * file which is so close to C++ that many of them can be compiled as C++. The
 * notable difference between a *.ino file and a *.cpp file is that the *.ino
 * file does not need to include function prototypes, and are instead
 * auto-generated.
*/

// This can't be in the namespace fl. It needs to be in the global namespace.


#if defined(FASTLED_STUB_MAIN) || defined(FASTLED_STUB_MAIN_INCLUDE_INO)

#ifndef _FASTLED_STRINGIFY
#define _FASTLED_STRINGIFY_HELPER(x) #x
#define _FASTLED_STRINGIFY(x) _FASTLED_STRINGIFY_HELPER(x)
#endif

#ifdef FASTLED_STUB_MAIN_INCLUDE_INO
// Correctly include the file by expanding and stringifying the macro value
#include _FASTLED_STRINGIFY(FASTLED_STUB_MAIN_INCLUDE_INO)
#else
void setup() {}
void loop() {}
#endif  // FASTLED_STUB_MAIN_INCLUDE_INO

#include <iostream>

int main() {
    // Super simple main function that just calls the setup and loop functions.
    setup();
    while(1) {
        loop();
    }
}
#endif  // FASTLED_STUB_MAIN_INCLUDE_INO
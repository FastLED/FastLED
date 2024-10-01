// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "lib8tion/scale_and_saturate.h" // Ensure this includes the correct scale_and_saturate definition
#include <iostream>

using namespace std;

void do_it(uint8_t a, uint8_t b) {
    uint8_t bprime = scale_and_saturate_u8(a, b);

    cout << "#############################################" << endl;
    cout << "a: " << (int)a << " b: " << (int)b << " bprime: " << (int)bprime
         << endl;
    // now multiply by b
    cout << "a * b: " << (int)(a * b) << " bprime * a: " << (int)(bprime * 255)
         << endl;

}

TEST_CASE("scale_and_saturate_u8") {

    SUBCASE("Test 1: mid values") {
        // Test 1: Normal case with a mid-range value for a and b
        uint8_t a = 128; // Mid-range value for a
        uint8_t b = 128; // Mid-range value for b
        do_it(a, b);
    }

    SUBCASE("Test 2: small values") {
        // Test 2: Normal case with a small value for a and b
        uint8_t a = 9; // Small value for a
        uint8_t b = 21; // Small value for b
        do_it(a, b);
    }
}
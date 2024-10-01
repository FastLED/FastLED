// g++ --std=c++11 test.cpp

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "doctest.h"
#include "lib8tion/scale_and_saturate.h"  // Ensure this includes the correct scale_and_saturate definition
#include <iostream>
#include <algorithm>

using namespace std;

TEST_CASE("scale_and_saturate_u8") {

    // Test 1: Normal case with a mid-range value for a and b
    uint8_t a = 128;  // Mid-range value for a
    uint8_t b = 128;  // Mid-range value for b

    uint8_t a_prime = 0;
    uint8_t b_prime = 0;

    // Call the function to saturate a and calculate b_prime
    scale_and_saturate_u8(a, b, &a_prime, &b_prime);

    std::cout << "#############################################" << std::endl;
    std::cout << "Original a: " << (int)a << " Original b: " << (int)b << std::endl;
    std::cout << "Saturated a_prime: " << (int)a_prime << " b_prime: " << (int)b_prime << std::endl;
    
    // Compare the original product with the new one
    uint16_t original_product = a * b;
    uint16_t new_product = a_prime * b_prime;
    std::cout << "a * b: " << (int)original_product << " a_prime * b_prime: " << (int)new_product << std::endl;

    // Tolerance value for the product comparison (e.g., 64)
    uint16_t tolerance = 64;

    CHECK(a_prime == 255);  // Ensure a_prime is saturated
    CHECK(abs(original_product - new_product) <= tolerance);  // Check that the product is as close as possible
}
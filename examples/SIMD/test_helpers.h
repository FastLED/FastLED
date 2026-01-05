// examples/SIMD/test_helpers.h
//
// Helper functions for SIMD testing

#pragma once

#include <FastLED.h>
#include "test_result.h"

namespace simd_test {

// ============================================================================
// Array Comparison Functions
// ============================================================================

/// @brief Compare two uint8_t arrays element-wise
/// @param a First array
/// @param b Second array
/// @param count Number of elements to compare
/// @return true if arrays match, false otherwise
bool compare_u8_arrays(const uint8_t* a, const uint8_t* b, size_t count);

/// @brief Compare two float arrays element-wise with epsilon tolerance
/// @param a First array
/// @param b Second array
/// @param count Number of elements to compare
/// @param epsilon Maximum allowed difference between elements
/// @return true if arrays match within epsilon, false otherwise
bool compare_f32_arrays(const float* a, const float* b, size_t count, float epsilon = 0.001f);

// ============================================================================
// Test Execution Functions
// ============================================================================

/// @brief Print test result to serial
/// @param result Test result to print
void print_test_result(const TestResult& result);

/// @brief Run a single test and record result
/// @param test_name Name of the test
/// @param test_func Test function to execute (takes TestResult& parameter)
/// @param results Vector to store test results
/// @param total_tests Total test counter (incremented)
/// @param passed_tests Passed test counter (incremented on pass)
/// @param failed_tests Failed test counter (incremented on fail)
template<typename TestFunc>
void run_test(const char* test_name, TestFunc test_func,
              fl::vector<TestResult>& results,
              int& total_tests, int& passed_tests, int& failed_tests) {
    total_tests++;
    TestResult result(test_name);

    // Execute test function
    test_func(result);

    // Update statistics
    if (result.passed) {
        passed_tests++;
    } else {
        failed_tests++;
    }

    // Print result
    print_test_result(result);

    // Store result
    results.push_back(result);
}

// ============================================================================
// Summary and Reporting Functions
// ============================================================================

/// @brief Print summary table of all test results
/// @param test_results All test results
/// @param total_tests Total number of tests
/// @param passed_tests Number of passed tests
/// @param failed_tests Number of failed tests
void print_summary(const fl::vector<TestResult>& test_results,
                   int total_tests, int passed_tests, int failed_tests);

/// @brief Print final result banner (PASS/FAIL)
/// @param failed_tests Number of failed tests (0 = all passed)
void print_final_banner(int failed_tests);

} // namespace simd_test

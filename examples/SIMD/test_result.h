// examples/SIMD/test_result.h
//
// Test result tracking structure for SIMD tests

#pragma once

namespace simd_test {

/// @brief Stores the result of a single SIMD test
struct TestResult {
    const char* test_name;
    bool passed;
    const char* error_msg;

    TestResult(const char* name) : test_name(name), passed(true), error_msg(nullptr) {}

    void fail(const char* msg) {
        passed = false;
        error_msg = msg;
    }
};

} // namespace simd_test

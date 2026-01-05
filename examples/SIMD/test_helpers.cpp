// examples/SIMD/test_helpers.cpp
//
// Helper function implementations for SIMD testing

#include "test_helpers.h"
#include "fl/dbg.h"

namespace simd_test {

// ============================================================================
// Array Comparison Functions
// ============================================================================

bool compare_u8_arrays(const uint8_t* a, const uint8_t* b, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

bool compare_f32_arrays(const float* a, const float* b, size_t count, float epsilon) {
    for (size_t i = 0; i < count; i++) {
        float diff = a[i] - b[i];
        if (diff < 0.0f) diff = -diff;
        if (diff > epsilon) {
            return false;
        }
    }
    return true;
}

// ============================================================================
// Test Execution Functions
// ============================================================================

void print_test_result(const TestResult& result) {
    fl::sstream ss;
    if (result.passed) {
        ss << "  ✓ PASS: " << result.test_name;
        FL_PRINT(ss.str());
    } else {
        ss << "  ✗ FAIL: " << result.test_name;
        FL_PRINT(ss.str());
        if (result.error_msg) {
            fl::sstream err;
            err << "    ERROR: " << result.error_msg;
            FL_ERROR(err.str().c_str());
        }
    }
}

// ============================================================================
// Summary and Reporting Functions
// ============================================================================

void print_summary(const fl::vector<TestResult>& test_results,
                   int total_tests, int passed_tests, int failed_tests) {
    fl::sstream ss;

    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ SIMD TEST SUMMARY                                              ║\n";
    ss << "╚════════════════════════════════════════════════════════════════╝\n";
    FL_PRINT(ss.str());

    ss.clear();
    ss << "Total Tests:  " << total_tests << "\n";
    ss << "Passed:       " << passed_tests << " ("
       << (total_tests > 0 ? (passed_tests * 100 / total_tests) : 0) << "%)\n";
    ss << "Failed:       " << failed_tests << " ("
       << (total_tests > 0 ? (failed_tests * 100 / total_tests) : 0) << "%)";
    FL_PRINT(ss.str());

    if (failed_tests > 0) {
        FL_PRINT("\nFailed Tests:");
        for (size_t i = 0; i < test_results.size(); i++) {
            if (!test_results[i].passed) {
                fl::sstream fail_ss;
                fail_ss << "  - " << test_results[i].test_name;
                if (test_results[i].error_msg) {
                    fail_ss << ": " << test_results[i].error_msg;
                }
                FL_PRINT(fail_ss.str());
            }
        }
    }
}

void print_final_banner(int failed_tests) {
    fl::sstream ss;
    ss << "\n";
    if (failed_tests == 0) {
        ss << "╔════════════════════════════════════════════════════════════════╗\n";
        ss << "║                      ✓ ALL TESTS PASSED                        ║\n";
        ss << "╚════════════════════════════════════════════════════════════════╝";
    } else {
        ss << "╔════════════════════════════════════════════════════════════════╗\n";
        ss << "║                      ✗ TESTS FAILED                            ║\n";
        ss << "╚════════════════════════════════════════════════════════════════╝";
    }
    FL_PRINT(ss.str());
}

} // namespace simd_test

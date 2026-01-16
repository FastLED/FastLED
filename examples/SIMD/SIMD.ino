// @filter: (memory is high)
//
// examples/SIMD/SIMD.ino
//
// FastLED SIMD Operations Test Suite
//
// This sketch validates all SIMD operations provided by fl::simd namespace.
// Tests are executed sequentially with pass/fail tracking and a summary report.
//
// DEMONSTRATES:
// 1. SIMD load/store operations (u8x16, u32x4, f32x4)
// 2. SIMD arithmetic operations (add, sub, scale, blend)
// 3. SIMD comparison operations (min, max, avg)
// 4. SIMD bitwise operations (and, or, xor, andnot)
// 5. SIMD floating-point operations (add, sub, mul, div, sqrt, min, max)
//
// Platform Support:
//   - All platforms (uses platform-specific SIMD when available, scalar fallback otherwise)
//
// Expected Output:
//   Serial monitor will show:
//   - Individual test results (PASS/FAIL) for each SIMD operation
//   - Error details for any failed tests
//   - Final summary table with pass/fail counts
//   - Overall PASS/FAIL banner
//
// ============================================================================
// AGENT INSTRUCTIONS
// ============================================================================
// This sketch is a validation test that uses the "ERROR" keyword in FL_ERROR
// statements to signal test failures. The `bash debug` command monitors for
// the "ERROR" keyword and fails the test when detected (exit code 1).
//
// 🚫 DO NOT "CHEAT" THE TEST:
//    - DO NOT change "ERROR" to "FAIL", "WARNING", "FAILURE", or any other
//      keyword to avoid test detection
//    - DO NOT modify FL_ERROR statements unless explicitly requested by the user
//    - The "ERROR" keyword is INTENTIONAL and part of the validation contract
//
// ✅ VALID MODIFICATIONS (only if explicitly requested):
//    - User asks: "Change the error message format"
//    - User asks: "Use a different warning macro"
//    - User asks: "Remove the ERROR prefix"
//
// ❌ INVALID MODIFICATIONS (considered "cheating"):
//    - Changing "ERROR" to avoid `bash debug --fail-on ERROR` detection
//    - Renaming to bypass automated test failure detection
//    - Any modification that circumvents the validation contract
//
// If the test is failing, investigate and FIX THE ACTUAL BUG that causes
// the "ERROR" message to be printed. Do not hide the symptom by changing
// the error message keyword.
// ============================================================================

#include <FastLED.h>
#include "fl/simd.h"
#include "fl/dbg.h"
#include "SketchHalt.h"

// Test framework headers
#include "test_result.h"
#include "test_helpers.h"
#include "simd_tests.h"

using namespace fl;
using namespace simd_test;

// ============================================================================
// Global Test State
// ============================================================================

// Halt mechanism
SketchHalt halt;

// Test results storage
fl::vector<TestResult> test_results;

// Test statistics
int total_tests = 0;
int passed_tests = 0;
int failed_tests = 0;

// ============================================================================
// Main Setup and Loop
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(2000);

    fl::sstream ss;
    ss << "\n╔════════════════════════════════════════════════════════════════╗\n";
    ss << "║ FastLED SIMD Operations Test Suite                            ║\n";
    ss << "╚════════════════════════════════════════════════════════════════╝\n";
    FL_PRINT(ss.str());

    FL_PRINT("\nStarting SIMD operation tests...\n");

    // ========================================================================
    // Load/Store Tests
    // ========================================================================
    FL_PRINT("\n[CATEGORY] Load/Store Operations");
    FL_PRINT("────────────────────────────────────────────────────────────────");

    run_test("load/store u8x16", test_load_store_u8_16, test_results, total_tests, passed_tests, failed_tests);
    run_test("load/store u32x4", test_load_store_u32_4, test_results, total_tests, passed_tests, failed_tests);
    run_test("load/store f32x4", test_load_store_f32_4, test_results, total_tests, passed_tests, failed_tests);

    // ========================================================================
    // Arithmetic Tests
    // ========================================================================
    FL_PRINT("\n[CATEGORY] Arithmetic Operations");
    FL_PRINT("────────────────────────────────────────────────────────────────");

    run_test("add_sat u8x16", test_add_sat_u8_16, test_results, total_tests, passed_tests, failed_tests);
    run_test("sub_sat u8x16", test_sub_sat_u8_16, test_results, total_tests, passed_tests, failed_tests);
    run_test("scale u8x16", test_scale_u8_16, test_results, total_tests, passed_tests, failed_tests);
    run_test("blend u8x16", test_blend_u8_16, test_results, total_tests, passed_tests, failed_tests);

    // ========================================================================
    // Comparison Tests
    // ========================================================================
    FL_PRINT("\n[CATEGORY] Comparison Operations");
    FL_PRINT("────────────────────────────────────────────────────────────────");

    run_test("min u8x16", test_min_u8_16, test_results, total_tests, passed_tests, failed_tests);
    run_test("max u8x16", test_max_u8_16, test_results, total_tests, passed_tests, failed_tests);
    run_test("avg u8x16", test_avg_u8_16, test_results, total_tests, passed_tests, failed_tests);
    run_test("avg_round u8x16", test_avg_round_u8_16, test_results, total_tests, passed_tests, failed_tests);

    // ========================================================================
    // Bitwise Tests
    // ========================================================================
    FL_PRINT("\n[CATEGORY] Bitwise Operations");
    FL_PRINT("────────────────────────────────────────────────────────────────");

    run_test("and u8x16", test_and_u8_16, test_results, total_tests, passed_tests, failed_tests);
    run_test("or u8x16", test_or_u8_16, test_results, total_tests, passed_tests, failed_tests);
    run_test("xor u8x16", test_xor_u8_16, test_results, total_tests, passed_tests, failed_tests);
    run_test("andnot u8x16", test_andnot_u8_16, test_results, total_tests, passed_tests, failed_tests);

    // ========================================================================
    // Broadcast Tests
    // ========================================================================
    FL_PRINT("\n[CATEGORY] Broadcast Operations");
    FL_PRINT("────────────────────────────────────────────────────────────────");

    run_test("set1 u32x4", test_set1_u32_4, test_results, total_tests, passed_tests, failed_tests);
    run_test("set1 f32x4", test_set1_f32_4, test_results, total_tests, passed_tests, failed_tests);

    // ========================================================================
    // Float Tests
    // ========================================================================
    FL_PRINT("\n[CATEGORY] Floating Point Operations");
    FL_PRINT("────────────────────────────────────────────────────────────────");

    run_test("add f32x4", test_add_f32_4, test_results, total_tests, passed_tests, failed_tests);
    run_test("sub f32x4", test_sub_f32_4, test_results, total_tests, passed_tests, failed_tests);
    run_test("mul f32x4", test_mul_f32_4, test_results, total_tests, passed_tests, failed_tests);
    run_test("div f32x4", test_div_f32_4, test_results, total_tests, passed_tests, failed_tests);
    run_test("sqrt f32x4", test_sqrt_f32_4, test_results, total_tests, passed_tests, failed_tests);
    run_test("min f32x4", test_min_f32_4, test_results, total_tests, passed_tests, failed_tests);
    run_test("max f32x4", test_max_f32_4, test_results, total_tests, passed_tests, failed_tests);

    // ========================================================================
    // Print Summary
    // ========================================================================
    print_summary(test_results, total_tests, passed_tests, failed_tests);
    print_final_banner(failed_tests);

    // Signal completion
    if (failed_tests == 0) {
        FL_PRINT("\nSIMD_TEST_SUITE_COMPLETE");
    } else {
        fl::sstream err;
        err << "\nSIMD test suite failed with " << failed_tests << " error(s)";
        FL_ERROR(err.str().c_str());
    }
}

void loop() {
    // Check if halted and return if so
    if (halt.check()) return;

    // Halt after running tests once
    halt.error("SIMD test suite complete - halting");
}

#!/bin/bash
# Script to demonstrate error propagation in animartrix2 test
# Toggles between GOOD and BAD states to show compilation error handling

set -e  # Exit on error (except where we expect failure)

echo "=========================================="
echo "Error Propagation Test for animartrix2"
echo "=========================================="
echo ""

TEST_FILE="tests/fl/fx/2d/animartrix2.cpp"

# Function to set the test state
set_test_state() {
    local state=$1
    echo "Setting test state to: $state"
    sed -i "s/#define ANIMARTRIX2_TEST_GOOD_STATE.*/#define ANIMARTRIX2_TEST_GOOD_STATE $state/" "$TEST_FILE"
}

# Function to check current state
check_current_state() {
    grep "#define ANIMARTRIX2_TEST_GOOD_STATE" "$TEST_FILE" | head -1
}

echo "Current state:"
check_current_state
echo ""

# Test 1: GOOD STATE (should compile successfully)
echo "=========================================="
echo "TEST 1: GOOD STATE (define = 1)"
echo "=========================================="
set_test_state 1
echo "Compiling..."
echo ""

if bash test animartrix2 --cpp --clean 2>&1 | tee /tmp/good_state.log | grep -q "Linking target tests/fl_fx_2d_animartrix2.dll"; then
    echo ""
    echo "✅ GOOD STATE: Compilation SUCCEEDED (as expected)"
    echo "   DLL was successfully built"
else
    echo ""
    echo "❌ GOOD STATE: Compilation FAILED (unexpected!)"
    echo "   Check /tmp/good_state.log for details"
    exit 1
fi

# Test 2: BAD STATE (should fail with clear error)
echo ""
echo "=========================================="
echo "TEST 2: BAD STATE (define = 0)"
echo "=========================================="
set_test_state 0
echo "Compiling..."
echo ""

# We expect this to fail, so don't exit on error
set +e
bash test animartrix2 --cpp --clean 2>&1 | tee /tmp/bad_state.log
COMPILE_RESULT=$?
set -e

if grep -q "error: expected '}'" /tmp/bad_state.log && grep -q "FAILED:" /tmp/bad_state.log; then
    echo ""
    echo "✅ BAD STATE: Compilation FAILED with clear error (as expected)"
    echo "   Error message:"
    grep "error: expected '}'" /tmp/bad_state.log | head -1
    echo ""
    echo "✅ ERROR PROPAGATION WORKING CORRECTLY!"
else
    echo ""
    echo "❌ BAD STATE: Did not see expected compilation error"
    echo "   This means error propagation is broken!"
    echo "   Check /tmp/bad_state.log for details"
    exit 1
fi

# Restore to GOOD state
echo ""
echo "=========================================="
echo "Restoring GOOD STATE"
echo "=========================================="
set_test_state 1
echo "Test file restored to good state"

echo ""
echo "=========================================="
echo "SUMMARY"
echo "=========================================="
echo "✅ GOOD STATE (define=1): Compilation succeeded"
echo "✅ BAD STATE (define=0): Compilation failed with visible error"
echo "✅ Error propagation is working correctly!"
echo ""
echo "Logs saved to:"
echo "  - /tmp/good_state.log (successful compilation)"
echo "  - /tmp/bad_state.log (failed compilation with errors)"

# Eliminate CMake Test System

## Overview
This feature will completely remove the legacy CMake-based test system from the FastLED project. The project now uses a custom Python-based builder and unit test runner that provides better performance, reduced memory usage, and more flexibility.

## Benefits
- **Performance**: The Python API system is 8x faster than the CMake system (15-30s → 2-4s)
- **Memory Efficiency**: 80% reduction in memory usage (2-4GB → 200-500MB)
- **Simplified Codebase**: Removal of duplicate build systems reduces maintenance burden
- **Faster CI**: Continuous integration will run faster with only one build system
- **Reduced Complexity**: Developers only need to learn one test system

## Implementation Plan

### 1. Remove CMake Configuration Files
- Delete the entire `tests/cmake/` directory containing CMake modules:
  - BuildOptions.cmake
  - CompilerDetection.cmake
  - CompilerFlags.cmake
  - LinkerCompatibility.cmake
  - DebugSettings.cmake
  - OptimizationSettings.cmake
  - DependencyManagement.cmake
  - ParallelBuild.cmake
  - TargetCreation.cmake
  - TestConfiguration.cmake
  - TestSourceDiscovery.cmake
  - ExampleCompileTest.cmake
  - StaticAnalysis.cmake

### 2. Remove CMake Build Files
- Delete `tests/CMakeLists.txt`
- Delete `tests/CMakeLists.txt.previous`
- Delete `tests/cmake_install.cmake`
- Delete `tests/CTestTestfile.cmake`
- Remove any other CMake-generated files in the tests directory

### 3. Update Test Runner Script
- Modify `test.py`:
  - Remove the `legacy` flag and related code paths
  - Remove environment variable checks for `USE_CMAKE`
  - Remove all code related to the legacy CMake system
  - Update documentation to reflect the single build system

### 4. Update C++ Test Runner
- Modify `ci/compiler/cpp_test_run.py`:
  - Remove `_compile_tests_cmake()` function
  - Remove `_run_tests_cmake()` function
  - Remove the legacy system checks and branching logic
  - Update the `compile_tests()` and `run_tests()` functions to only use the Python API

### 5. Clean Up Related Files
- Update any documentation referring to the CMake test system
- Remove any CMake-specific test configuration files
- Clean up any scripts that reference the CMake test system

### 6. Testing
- Verify all tests run correctly with the Python API system
- Ensure no regressions in test coverage or functionality
- Validate CI pipeline works correctly without the CMake system

### 7. Update Documentation
- Update README and other documentation to reflect the single test system
- Add notes about the performance improvements
- Update any developer guides that referenced the CMake system

## Migration Guide for Developers
Developers previously using the CMake system should:
1. Use `bash test` to run all tests (unchanged)
2. Use `bash test <test_name>` to run a specific test (unchanged)
3. Use `uv run python -m ci.compiler.cpp_test_run --test <test_name>` for direct test execution

## Timeline
1. Remove CMake files (1 day)
2. Update test scripts (1 day)
3. Testing and validation (1 day)
4. Documentation updates (0.5 day)

Total estimated time: 3.5 days

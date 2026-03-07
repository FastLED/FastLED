#include "test.h"
#include "fl/stl/iostream.h"
#include "fl/stl/cstdio.h"

// Diagnostic test to verify TestRegistry is working correctly
// This test queries the registry and prints statistics about registered tests

FL_TEST_FILE(FL_FILEPATH) {

FL_TEST_CASE("TestRegistry - Diagnostic") {
    // Get the singleton registry
    auto& registry = ::fl::Singleton<::TestRegistry>::instance();

    // Get all test groups
    const auto& groups = registry.getTestGroups();

    // Print summary
    fl::printf("\n=== TestRegistry Diagnostic ===\n");
    fl::printf("Total files with tests: %zu\n", groups.size());

    int total_tests = 0;
    for (const auto& group : groups) {
        const auto& file_path = group.first;
        const auto& tests = group.second;
        fl::printf("  %s: %zu tests\n", file_path.c_str(), tests.size());
        total_tests += (int)tests.size();
    }

    fl::printf("Total tests registered: %d\n", total_tests);
    fl::printf("==============================\n\n");

    // Verify we actually registered something
    FL_CHECK_GT(groups.size(), 0);
    FL_CHECK_GT(total_tests, 0);
}

FL_TEST_CASE("TestRegistry - Verify registry is working per-DLL") {
    // Note: In DLL mode, each test DLL has its own Singleton instance
    // This is expected behavior - audio_reactive.cpp would have its own registry
    // when compiled as a separate DLL.

    auto& registry = ::fl::Singleton<::TestRegistry>::instance();
    const auto& groups = registry.getTestGroups();

    // Verify that the registry in this DLL has the tests from this file
    bool found_diagnostic_file = false;
    for (const auto& group : groups) {
        const auto& file_path = group.first;
        if (file_path.find("test_registry_diagnostic") != fl::string::npos) {
            found_diagnostic_file = true;
            fl::printf("\nDiagnostic file tests in this DLL: %zu\n", group.second.size());
            break;
        }
    }

    FL_CHECK(found_diagnostic_file);
}

FL_TEST_CASE("TestRegistry - Query by file path") {
    auto& registry = ::fl::Singleton<::TestRegistry>::instance();

    // Get current file path (this file)
    const char* this_file = __FILE__;
    size_t this_file_count = registry.getTestCount(this_file);

    // This file should have at least the tests defined in it (3 so far)
    fl::printf("Tests in %s: %zu\n", this_file, this_file_count);
    FL_CHECK_GE(this_file_count, 2);  // At least the first 2 tests
}

} // FL_TEST_FILE

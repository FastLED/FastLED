#pragma once

#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/map.h"
#include "fl/stl/shared_ptr.h"
#include "fl/singleton.h"

/**
 * TestCase represents metadata about a single test case.
 *
 * Used for organizing tests by file path in unity builds,
 * allowing test filtering and grouping.
 *
 * Stores both the file path (for file-based filtering in unity builds)
 * and the test name (for exact test identification and filtering).
 */
class TestCase {
public:
    /**
     * Create a test case with file path and name.
     *
     * Registration happens at static initialization time during FL_TEST_CASE.
     * The order of registration within a file determines the test index.
     *
     * @param file_path  Source file path (__FILE__)
     * @param test_name  Test case name (from FL_TEST_CASE macro first arg)
     */
    TestCase(const char* file_path, const char* test_name)
        : file_path_(file_path)
        , test_name_(test_name)
    {
    }

    /**
     * Get the source file path where this test was declared.
     */
    const fl::string& get_file_path() const { return file_path_; }

    /**
     * Get the test case name.
     */
    const fl::string& get_test_name() const { return test_name_; }

private:
    fl::string file_path_;
    fl::string test_name_;
};

/**
 * TestRegistry is a singleton that groups test cases by file path.
 *
 * In unity builds (multiple .cpp files concatenated), this preserves
 * the association between tests and their original source files,
 * enabling file-based test filtering and selection.
 *
 * Structure: map<file_path, vector<TestCase>>
 *
 * Uses fl::Singleton for safe singleton management across DLL boundaries.
 */
class TestRegistry {
public:
    /**
     * Register a test case with its file path and name.
     *
     * Called by FL_TEST_CASE_IMPL macro during static initialization.
     * Stores both file path (for unity build filtering) and test name
     * (for exact test identification).
     *
     * @param file_path  Source file path (__FILE__)
     * @param test_name  Test case name (from FL_TEST_CASE first argument)
     */
    void registerTest(const char* file_path, const char* test_name) {
        auto test_case = fl::make_shared<TestCase>(file_path, test_name);
        test_groups_[file_path].push_back(test_case);
    }

    /**
     * Get all test groups organized by file path.
     *
     * @return Const reference to the test groups map
     */
    const fl::map<fl::string, fl::vector<fl::shared_ptr<TestCase>>>&
    getTestGroups() const {
        return test_groups_;
    }

    /**
     * Get test count for a specific file path.
     *
     * Useful for debugging and verifying test registration.
     *
     * @param file_path  File path to query
     * @return Number of tests from this file (0 if not found)
     */
    size_t getTestCount(const char* file_path) const {
        auto it = test_groups_.find(file_path);
        if (it != test_groups_.end()) {
            return it->second.size();
        }
        return 0;
    }

    /**
     * Get total test count across all files.
     */
    size_t getTotalTestCount() const {
        size_t total = 0;
        for (const auto& group : test_groups_) {
            total += group.second.size();
        }
        return total;
    }

private:
    friend class fl::Singleton<TestRegistry>;

    // Private constructor - singleton
    TestRegistry() = default;

    // Prevent copying/moving
    TestRegistry(const TestRegistry&) = delete;
    TestRegistry& operator=(const TestRegistry&) = delete;

    // Test groups: file_path -> vector of test cases
    fl::map<fl::string, fl::vector<fl::shared_ptr<TestCase>>> test_groups_;
};

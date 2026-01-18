#pragma once

/// @file fl/fltest.h
/// @brief Portable test framework for FastLED
///
/// This is a lightweight test framework that can run on both host computers
/// and embedded devices. It provides macros similar to doctest but with
/// minimal dependencies on the standard library.
///
/// Features:
/// - TEST_CASE / SUBCASE hierarchy with proper re-entry
/// - CHECK / REQUIRE assertion macros
/// - Support for embedded devices (serial output)
///
/// Usage:
///   #include "fl/fltest.h"
///
///   FL_TEST_CASE("MyTest") {
///       FL_CHECK(1 + 1 == 2);
///       FL_SUBCASE("nested") {
///           FL_CHECK_EQ(2, 2);
///       }
///   }

#include "fl/stl/stdint.h"
#include "fl/stl/vector.h"
#include "fl/stl/strstream.h"
#include "fl/stl/cstring.h"
#include "fl/stl/type_traits.h"

// Conditionally include <exception> for platforms with exception support
// This must be at the top of the file (before any namespace declarations)
#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
#include <exception>  // For std::exception in CHECK_THROWS_WITH
#endif

// Configure maximum test depth and counts
#ifndef FLTEST_MAX_SUBCASE_DEPTH
#define FLTEST_MAX_SUBCASE_DEPTH 8
#endif

#ifndef FLTEST_MAX_TEST_CASES
#define FLTEST_MAX_TEST_CASES 512
#endif

namespace fl {
namespace test {

// Forward declarations
class TestCase;
class Subcase;
class TestContext;

// Source location info
struct SourceLocation {
    const char* mFile;
    int mLine;

    SourceLocation(const char* file = "", int line = 0)
        : mFile(file), mLine(line) {}
};

// Result of an assertion
struct AssertResult {
    bool mPassed;
    fl::string mExpression;
    fl::string mExpanded;  // Expanded values
    SourceLocation mLocation;

    AssertResult(bool passed = true)
        : mPassed(passed) {}
};

// Test statistics
struct TestStats {
    fl::u32 mTestCasesRun = 0;
    fl::u32 mTestCasesPassed = 0;
    fl::u32 mTestCasesFailed = 0;
    fl::u32 mTestCasesSkipped = 0;  // Tracks tests skipped via FL_SKIP
    fl::u32 mAssertsPassed = 0;
    fl::u32 mAssertsFailed = 0;
    fl::u32 mTotalDurationMs = 0;   // Total duration of all tests in milliseconds

    void reset() {
        mTestCasesRun = 0;
        mTestCasesPassed = 0;
        mTestCasesFailed = 0;
        mTestCasesSkipped = 0;
        mAssertsPassed = 0;
        mAssertsFailed = 0;
        mTotalDurationMs = 0;
    }

    bool allPassed() const {
        return mAssertsFailed == 0 && mTestCasesFailed == 0;
    }
};

// Reporter interface for outputting results
class IReporter {
public:
    virtual ~IReporter() = default;

    virtual void testRunStart() = 0;
    virtual void testRunEnd(const TestStats& stats) = 0;
    virtual void testCaseStart(const char* name) = 0;
    /// Called when a test case ends
    /// @param passed Whether the test passed
    /// @param durationMs Duration of the test in milliseconds (0 if no timer available)
    virtual void testCaseEnd(bool passed, fl::u32 durationMs = 0) = 0;
    virtual void subcaseStart(const char* name) = 0;
    virtual void subcaseEnd() = 0;
    virtual void assertResult(const AssertResult& result) = 0;
};

// Default reporter that uses FL_DBG/printf
class DefaultReporter : public IReporter {
public:
    void testRunStart() override;
    void testRunEnd(const TestStats& stats) override;
    void testCaseStart(const char* name) override;
    void testCaseEnd(bool passed, fl::u32 durationMs = 0) override;
    void subcaseStart(const char* name) override;
    void subcaseEnd() override;
    void assertResult(const AssertResult& result) override;
};

// Subcase signature for tracking which subcases have been run
struct SubcaseSignature {
    const char* mName;
    const char* mFile;
    int mLine;

    bool operator==(const SubcaseSignature& other) const {
        return mLine == other.mLine &&
               fl::strcmp(mFile, other.mFile) == 0 &&
               fl::strcmp(mName, other.mName) == 0;
    }

    bool operator!=(const SubcaseSignature& other) const {
        return !(*this == other);
    }
};

// Hash function for SubcaseSignature
inline fl::u32 hashSubcaseSignature(const SubcaseSignature& sig) {
    fl::u32 hash = 0;
    for (const char* p = sig.mName; *p; ++p) {
        hash = hash * 31 + static_cast<fl::u32>(*p);
    }
    for (const char* p = sig.mFile; *p; ++p) {
        hash = hash * 31 + static_cast<fl::u32>(*p);
    }
    hash = hash * 31 + static_cast<fl::u32>(sig.mLine);
    return hash;
}

// =============================================================================
// Timeout support types
// =============================================================================
// For embedded devices, we use a callback-based timeout mechanism.
// The user provides a function to get current time (in milliseconds)
// and optionally a timeout duration per test.

/// Callback type for getting current time in milliseconds
/// Example: uint32_t getMillis() { return millis(); }  // Arduino
typedef fl::u32 (*GetMillisFunc)();

/// Callback type for timeout handler
/// Called when a test times out. Return true to abort, false to continue.
typedef bool (*TimeoutHandlerFunc)(const char* testName, fl::u32 elapsedMs);

// Global test context - manages test execution state
class TestContext {
public:
    static TestContext& instance();

    // Test registration
    typedef void (*TestFunc)();
    struct TestCaseInfo {
        TestFunc mFunc;
        const char* mName;
        const char* mFile;
        int mLine;
    };

    int registerTest(TestFunc func, const char* name, const char* file, int line);

    // Run all tests
    // argc/argv: command line args (filter pattern from first arg)
    // filter: explicit filter pattern (if provided, overrides argv)
    int run(int argc = 0, const char* const* argv = nullptr);

    // Run tests matching filter pattern
    // Filter supports:
    //   - "*" matches any sequence of characters
    //   - "?" matches any single character
    //   - Exact substring match if no wildcards
    int run(const char* filter);

    // List all registered test names without running them
    // Returns the number of tests listed
    fl::size listTests(const char* filter = nullptr) const;

    // Subcase management
    bool enterSubcase(const SubcaseSignature& sig);
    void exitSubcase(const SubcaseSignature& sig);
    bool needsReentry() const { return !mNextSubcaseStack.empty(); }

    // Assertion handling
    void reportAssert(const AssertResult& result);
    void checkFailed(const char* expr, const char* file, int line);
    void requireFailed(const char* expr, const char* file, int line);

    // Reporter
    void setReporter(IReporter* reporter) { mReporter = reporter; }
    IReporter* reporter() { return mReporter; }

    // Stats
    TestStats& stats() { return mStats; }
    const TestStats& stats() const { return mStats; }

    // Current test state
    bool hasFailure() const { return mCurrentTestFailed; }
    void setCurrentTestFailed(bool failed) { mCurrentTestFailed = failed; }

    // Timeout support
    /// Set the function to get current time in milliseconds
    void setGetMillis(GetMillisFunc func) { mGetMillis = func; }

    /// Set the timeout handler callback
    void setTimeoutHandler(TimeoutHandlerFunc func) { mTimeoutHandler = func; }

    /// Set default timeout for all tests (0 = no timeout)
    void setDefaultTimeoutMs(fl::u32 timeoutMs) { mDefaultTimeoutMs = timeoutMs; }

    /// Check if current test has timed out (call periodically in long tests)
    /// Returns true if timed out
    bool checkTimeout();

    /// Get elapsed time for current test (in ms)
    fl::u32 getElapsedMs() const;

private:
    TestContext();

    fl::vector<TestCaseInfo> mTestCases;
    fl::vector<SubcaseSignature> mSubcaseStack;       // Current path being followed
    fl::vector<SubcaseSignature> mNextSubcaseStack;   // Path for next iteration
    fl::vector<fl::u32> mFullyTraversedHashes;        // Fully explored paths

    IReporter* mReporter = nullptr;
    DefaultReporter mDefaultReporter;
    TestStats mStats;

    bool mCurrentTestFailed = false;
    bool mShouldReenter = false;                      // Need another iteration?
    fl::size mCurrentSubcaseDepth = 0;                // How deep in subcases
    fl::size mSubcaseDiscoveryDepth = 0;              // Position in discovery

    // Timeout support
    GetMillisFunc mGetMillis = nullptr;
    TimeoutHandlerFunc mTimeoutHandler = nullptr;
    fl::u32 mDefaultTimeoutMs = 0;                    // 0 = no timeout
    fl::u32 mCurrentTestStartMs = 0;
    const char* mCurrentTestName = nullptr;
    bool mCurrentTestTimedOut = false;

    // Run a single test case
    void runTestCase(const TestCaseInfo& info);

    // Hash the path to a subcase
    fl::u32 hashCurrentPath(const SubcaseSignature& sig) const;

    // Check if a path has been fully traversed
    bool isFullyTraversed(fl::u32 hash) const;
    void markFullyTraversed(fl::u32 hash);

    // Pattern matching helper for test filtering
    bool matchesFilter(const char* name, const char* filter) const;
};

// RAII subcase class
class Subcase {
public:
    Subcase(const char* name, const char* file, int line);
    ~Subcase();

    explicit operator bool() const { return mEntered; }

private:
    SubcaseSignature mSignature;
    bool mEntered = false;
};

// Test registration helper
struct TestRegistrar {
    TestRegistrar(TestContext::TestFunc func, const char* name, const char* file, int line) {
        TestContext::instance().registerTest(func, name, file, line);
    }
};

// Expression decomposition for better error messages
template<typename T>
struct ExpressionValue {
    T mValue;
    fl::string mStringified;

    ExpressionValue(const T& value) : mValue(value) {
        fl::StrStream ss;
        ss << value;
        mStringified = ss.str();
    }

    operator T() const { return mValue; }
    const char* str() const { return mStringified.c_str(); }
};

// Comparison helpers
// Use decay to remove references and cv qualifiers for comparison function types
template<typename L, typename R>
struct CompareEq {
    bool operator()(const L& lhs, const R& rhs) const { return lhs == rhs; }
};

template<typename L, typename R>
struct CompareNe {
    bool operator()(const L& lhs, const R& rhs) const { return lhs != rhs; }
};

template<typename L, typename R>
struct CompareLt {
    bool operator()(const L& lhs, const R& rhs) const { return lhs < rhs; }
};

template<typename L, typename R>
struct CompareGt {
    bool operator()(const L& lhs, const R& rhs) const { return lhs > rhs; }
};

template<typename L, typename R>
struct CompareLe {
    bool operator()(const L& lhs, const R& rhs) const { return lhs <= rhs; }
};

template<typename L, typename R>
struct CompareGe {
    bool operator()(const L& lhs, const R& rhs) const { return lhs >= rhs; }
};

// Binary assertion helper
template<typename L, typename R, typename Cmp>
bool binaryAssert(const L& lhs, const R& rhs, Cmp cmp,
                  const char* lhsExpr, const char* op, const char* rhsExpr,
                  const char* file, int line) {
    bool result = cmp(lhs, rhs);

    AssertResult ar(result);
    ar.mLocation = SourceLocation(file, line);

    fl::StrStream exprSS;
    exprSS << lhsExpr << " " << op << " " << rhsExpr;
    ar.mExpression = exprSS.str();

    if (!result) {
        fl::StrStream expandedSS;
        expandedSS << lhs << " " << op << " " << rhs;
        ar.mExpanded = expandedSS.str();
    }

    TestContext::instance().reportAssert(ar);
    return result;
}

// =============================================================================
// Approx class for floating point comparison
// =============================================================================

/// Helper class for approximate floating-point comparisons.
/// Supports both relative (epsilon) and absolute (margin) tolerance.
///
/// Usage:
///   FL_CHECK(value == fl::test::Approx(expected));
///   FL_CHECK(value == fl::test::Approx(expected).epsilon(0.01));  // 1% relative
///   FL_CHECK(value == fl::test::Approx(expected).margin(0.001));  // absolute margin
///
/// Comparison formula:
///   |actual - expected| <= margin OR
///   |actual - expected| <= epsilon * (scale + max(|actual|, |expected|))
///
/// By default: epsilon=1e-5, margin=0.0 (only epsilon comparison)
class Approx {
public:
    explicit Approx(double value)
        : mValue(value)
        , mEpsilon(1e-5)  // Default relative epsilon
        , mMargin(0.0)    // Default no absolute margin
        , mScale(1.0)
    {}

    /// Set custom relative epsilon for comparison
    /// Comparison uses: epsilon * (scale + max(|a|, |b|))
    Approx& epsilon(double newEpsilon) {
        mEpsilon = newEpsilon;
        return *this;
    }

    /// Set absolute margin for comparison
    /// When margin > 0, passes if |a - b| <= margin
    Approx& margin(double newMargin) {
        mMargin = newMargin < 0 ? 0 : newMargin;
        return *this;
    }

    /// Set custom scale for comparison
    Approx& scale(double newScale) {
        mScale = newScale;
        return *this;
    }

    double value() const { return mValue; }
    double getEpsilon() const { return mEpsilon; }
    double getMargin() const { return mMargin; }
    double getScale() const { return mScale; }

    // Comparison operators
    friend bool operator==(double lhs, const Approx& rhs) {
        double diff = lhs - rhs.mValue;
        if (diff < 0) diff = -diff;

        // Check absolute margin first (if set)
        if (rhs.mMargin > 0.0 && diff <= rhs.mMargin) {
            return true;
        }

        // Check relative epsilon
        double lhsAbs = lhs < 0 ? -lhs : lhs;
        double rhsAbs = rhs.mValue < 0 ? -rhs.mValue : rhs.mValue;
        double maxAbs = lhsAbs > rhsAbs ? lhsAbs : rhsAbs;
        double relativeMargin = rhs.mEpsilon * (rhs.mScale + maxAbs);
        return diff <= relativeMargin;
    }

    friend bool operator==(const Approx& lhs, double rhs) {
        return rhs == lhs;
    }

    friend bool operator!=(double lhs, const Approx& rhs) {
        return !(lhs == rhs);
    }

    friend bool operator!=(const Approx& lhs, double rhs) {
        return !(lhs == rhs);
    }

    friend bool operator<=(double lhs, const Approx& rhs) {
        return lhs < rhs.mValue || lhs == rhs;
    }

    friend bool operator>=(double lhs, const Approx& rhs) {
        return lhs > rhs.mValue || lhs == rhs;
    }

    friend bool operator<(double lhs, const Approx& rhs) {
        return lhs < rhs.mValue && !(lhs == rhs);
    }

    friend bool operator>(double lhs, const Approx& rhs) {
        return lhs > rhs.mValue && !(lhs == rhs);
    }

private:
    double mValue;
    double mEpsilon;
    double mMargin;
    double mScale;
};

// Stringify Approx for output
inline fl::StrStream& operator<<(fl::StrStream& os, const Approx& approx) {
    os << "Approx(" << approx.value() << ")";
    return os;
}

// =============================================================================
// Message output helper
// =============================================================================

/// Helper to output INFO/MESSAGE during test execution
void outputMessage(const char* msg, const char* file, int line);

/// Helper to output CAPTURE variable
void outputCapture(const char* name, const char* value, const char* file, int line);

/// Helper for FAIL macros
void fail(const char* msg, const char* file, int line, bool isFatal);

// =============================================================================
// SerialReporter for embedded device output
// =============================================================================
// This reporter outputs to a serial port interface. On embedded devices like
// ESP32 or Teensy, include this header and set:
//   fl::test::TestContext::instance().setReporter(&mySerialReporter);
//
// The reporter uses a configurable print function (default: fl::printf)
// which can be overridden to use Serial.print, etc.

/// Type for the serial print function callback
typedef void (*SerialPrintFunc)(const char* msg);

/// Serial reporter for embedded devices
/// Usage:
///   SerialReporter reporter(mySerialPrint);
///   fl::test::TestContext::instance().setReporter(&reporter);
class SerialReporter : public IReporter {
public:
    /// Create a serial reporter with custom print function
    explicit SerialReporter(SerialPrintFunc printFunc = nullptr)
        : mPrintFunc(printFunc) {}

    void testRunStart() override;
    void testRunEnd(const TestStats& stats) override;
    void testCaseStart(const char* name) override;
    void testCaseEnd(bool passed, fl::u32 durationMs = 0) override;
    void subcaseStart(const char* name) override;
    void subcaseEnd() override;
    void assertResult(const AssertResult& result) override;

    /// Set the print function
    void setPrintFunc(SerialPrintFunc func) { mPrintFunc = func; }

private:
    SerialPrintFunc mPrintFunc;
    void print(const char* msg);
};

// =============================================================================
// XMLReporter for CI/CD integration (JUnit format)
// =============================================================================
// This reporter outputs test results in JUnit XML format, which is widely
// supported by CI systems like Jenkins, GitHub Actions, GitLab CI, etc.
//
// Usage:
//   fl::string xmlOutput;
//   fl::test::XMLReporter reporter(&xmlOutput);
//   fl::test::TestContext::instance().setReporter(&reporter);
//   fl::test::TestContext::instance().run();
//   // xmlOutput now contains the JUnit XML

/// XML reporter that outputs JUnit-compatible XML format
/// The output is accumulated in a string buffer provided by the user.
class XMLReporter : public IReporter {
public:
    /// Create an XML reporter that writes to the given string buffer
    explicit XMLReporter(fl::string* outputBuffer, const char* suiteName = "fltest")
        : mOutput(outputBuffer), mSuiteName(suiteName) {}

    void testRunStart() override;
    void testRunEnd(const TestStats& stats) override;
    void testCaseStart(const char* name) override;
    void testCaseEnd(bool passed, fl::u32 durationMs = 0) override;
    void subcaseStart(const char* name) override;
    void subcaseEnd() override;
    void assertResult(const AssertResult& result) override;

    /// Set the test suite name (used in XML output)
    void setSuiteName(const char* name) { mSuiteName = name; }

private:
    fl::string* mOutput;
    const char* mSuiteName;

    // Current test case state
    fl::string mCurrentTestName;
    fl::string mCurrentTestFailures;
    bool mCurrentTestPassed = true;
    fl::vector<fl::string> mTestCaseResults;

    // Helper to escape XML special characters
    static fl::string escapeXml(const char* text);
};

// =============================================================================
// JSONReporter for modern CI/CD integration
// =============================================================================
// This reporter outputs test results in JSON format, useful for custom
// dashboards, APIs, and modern CI systems.
//
// Usage:
//   fl::string jsonOutput;
//   fl::test::JSONReporter reporter(&jsonOutput);
//   fl::test::TestContext::instance().setReporter(&reporter);
//   fl::test::TestContext::instance().run();
//   // jsonOutput now contains the JSON

/// JSON reporter that outputs test results in JSON format
class JSONReporter : public IReporter {
public:
    /// Create a JSON reporter that writes to the given string buffer
    explicit JSONReporter(fl::string* outputBuffer)
        : mOutput(outputBuffer) {}

    void testRunStart() override;
    void testRunEnd(const TestStats& stats) override;
    void testCaseStart(const char* name) override;
    void testCaseEnd(bool passed, fl::u32 durationMs = 0) override;
    void subcaseStart(const char* name) override;
    void subcaseEnd() override;
    void assertResult(const AssertResult& result) override;

private:
    fl::string* mOutput;

    // Current test case state
    fl::string mCurrentTestName;
    fl::vector<fl::string> mCurrentTestFailures;
    bool mCurrentTestPassed = true;

    // All test results as JSON objects
    fl::vector<fl::string> mTestResults;

    // Helper to escape JSON special characters
    static fl::string escapeJson(const char* text);
};

// =============================================================================
// TAPReporter for TAP (Test Anything Protocol) output
// =============================================================================
// TAP is a simple text-based protocol for test results, widely supported by
// CI systems and test harnesses. See https://testanything.org/
//
// TAP output looks like:
//   TAP version 13
//   1..5
//   ok 1 - test name
//   not ok 2 - failing test
//   # diagnostic message
//   ok 3 - another test
//   ...

/// TAP reporter that outputs TAP-compatible test results
/// Can output to a string buffer or use a print function for streaming output
class TAPReporter : public IReporter {
public:
    /// Create a TAP reporter that writes to the given string buffer
    explicit TAPReporter(fl::string* outputBuffer)
        : mOutput(outputBuffer), mPrintFunc(nullptr), mTestNumber(0), mTotalTests(0) {}

    /// Create a TAP reporter that uses a print function for streaming output
    explicit TAPReporter(SerialPrintFunc printFunc)
        : mOutput(nullptr), mPrintFunc(printFunc), mTestNumber(0), mTotalTests(0) {}

    void testRunStart() override;
    void testRunEnd(const TestStats& stats) override;
    void testCaseStart(const char* name) override;
    void testCaseEnd(bool passed, fl::u32 durationMs = 0) override;
    void subcaseStart(const char* name) override;
    void subcaseEnd() override;
    void assertResult(const AssertResult& result) override;

    /// Set the total number of tests (for TAP plan line)
    /// If not set, plan is output at the end instead
    void setTotalTests(fl::u32 total) { mTotalTests = total; }

private:
    fl::string* mOutput;
    SerialPrintFunc mPrintFunc;
    fl::u32 mTestNumber;
    fl::u32 mTotalTests;

    // Current test state
    fl::string mCurrentTestName;
    bool mCurrentTestPassed;
    fl::vector<fl::string> mDiagnostics;
    fl::string mStreamingOutput;  // Used for streaming mode

    // Helper to output a line
    void output(const char* line);
};

// =============================================================================
// Test skip support
// =============================================================================
// Allows tests to be skipped with a message

/// Record that the current test should be skipped
void skipTest(const char* reason, const char* file, int line);

/// Check if current test has been marked as skipped
bool isTestSkipped();

} // namespace test
} // namespace fl

// =============================================================================
// Macros
// =============================================================================

// Unique identifier generation
#define FLTEST_CAT_IMPL(a, b) a##b
#define FLTEST_CAT(a, b) FLTEST_CAT_IMPL(a, b)

// Use __LINE__ for unique identifiers - more reliable than __COUNTER__
// which increments on each expansion
#define FLTEST_UNIQUE(x) FLTEST_CAT(x, __LINE__)

// Test case registration
// Uses __LINE__ to ensure all three usages get the same unique suffix
#define FL_TEST_CASE(name)                                                      \
    static void FLTEST_UNIQUE(FLTEST_FUNC_)();                                  \
    static fl::test::TestRegistrar FLTEST_UNIQUE(FLTEST_REG_)(                  \
        FLTEST_UNIQUE(FLTEST_FUNC_), name, __FILE__, __LINE__);                 \
    static void FLTEST_UNIQUE(FLTEST_FUNC_)()

// Subcase
#define FL_SUBCASE(name)                                                        \
    if (const fl::test::Subcase& FLTEST_UNIQUE(FLTEST_SUB_) =                   \
            fl::test::Subcase(name, __FILE__, __LINE__))

// Basic assertions
#define FL_CHECK(expr)                                                          \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fl::test::TestContext::instance().checkFailed(#expr, __FILE__, __LINE__); \
        } else {                                                                 \
            fl::test::AssertResult ar(true);                                    \
            ar.mExpression = #expr;                                              \
            ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);        \
            fl::test::TestContext::instance().reportAssert(ar);                 \
        }                                                                        \
    } while (0)

#define FL_CHECK_FALSE(expr)                                                    \
    do {                                                                         \
        if (expr) {                                                              \
            fl::test::TestContext::instance().checkFailed("!(" #expr ")", __FILE__, __LINE__); \
        } else {                                                                 \
            fl::test::AssertResult ar(true);                                    \
            ar.mExpression = "!(" #expr ")";                                     \
            ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);        \
            fl::test::TestContext::instance().reportAssert(ar);                 \
        }                                                                        \
    } while (0)

#define FL_REQUIRE(expr)                                                        \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fl::test::TestContext::instance().requireFailed(#expr, __FILE__, __LINE__); \
            return;                                                              \
        } else {                                                                 \
            fl::test::AssertResult ar(true);                                    \
            ar.mExpression = #expr;                                              \
            ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);        \
            fl::test::TestContext::instance().reportAssert(ar);                 \
        }                                                                        \
    } while (0)

#define FL_REQUIRE_FALSE(expr)                                                  \
    do {                                                                         \
        if (expr) {                                                              \
            fl::test::TestContext::instance().requireFailed("!(" #expr ")", __FILE__, __LINE__); \
            return;                                                              \
        } else {                                                                 \
            fl::test::AssertResult ar(true);                                    \
            ar.mExpression = "!(" #expr ")";                                     \
            ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);        \
            fl::test::TestContext::instance().reportAssert(ar);                 \
        }                                                                        \
    } while (0)

// Binary comparison assertions - use decay_t to strip cv-qualifiers and references
#define FL_CHECK_EQ(lhs, rhs)                                                   \
    fl::test::binaryAssert(lhs, rhs, fl::test::CompareEq<fl::decay_t<decltype(lhs)>, fl::decay_t<decltype(rhs)>>{}, \
                           #lhs, "==", #rhs, __FILE__, __LINE__)

#define FL_CHECK_NE(lhs, rhs)                                                   \
    fl::test::binaryAssert(lhs, rhs, fl::test::CompareNe<fl::decay_t<decltype(lhs)>, fl::decay_t<decltype(rhs)>>{}, \
                           #lhs, "!=", #rhs, __FILE__, __LINE__)

#define FL_CHECK_LT(lhs, rhs)                                                   \
    fl::test::binaryAssert(lhs, rhs, fl::test::CompareLt<fl::decay_t<decltype(lhs)>, fl::decay_t<decltype(rhs)>>{}, \
                           #lhs, "<", #rhs, __FILE__, __LINE__)

#define FL_CHECK_GT(lhs, rhs)                                                   \
    fl::test::binaryAssert(lhs, rhs, fl::test::CompareGt<fl::decay_t<decltype(lhs)>, fl::decay_t<decltype(rhs)>>{}, \
                           #lhs, ">", #rhs, __FILE__, __LINE__)

#define FL_CHECK_LE(lhs, rhs)                                                   \
    fl::test::binaryAssert(lhs, rhs, fl::test::CompareLe<fl::decay_t<decltype(lhs)>, fl::decay_t<decltype(rhs)>>{}, \
                           #lhs, "<=", #rhs, __FILE__, __LINE__)

#define FL_CHECK_GE(lhs, rhs)                                                   \
    fl::test::binaryAssert(lhs, rhs, fl::test::CompareGe<fl::decay_t<decltype(lhs)>, fl::decay_t<decltype(rhs)>>{}, \
                           #lhs, ">=", #rhs, __FILE__, __LINE__)

#define FL_REQUIRE_EQ(lhs, rhs)                                                 \
    do {                                                                         \
        if (!FL_CHECK_EQ(lhs, rhs)) return;                                     \
    } while (0)

#define FL_REQUIRE_NE(lhs, rhs)                                                 \
    do {                                                                         \
        if (!FL_CHECK_NE(lhs, rhs)) return;                                     \
    } while (0)

#define FL_REQUIRE_LT(lhs, rhs)                                                 \
    do {                                                                         \
        if (!FL_CHECK_LT(lhs, rhs)) return;                                     \
    } while (0)

#define FL_REQUIRE_GT(lhs, rhs)                                                 \
    do {                                                                         \
        if (!FL_CHECK_GT(lhs, rhs)) return;                                     \
    } while (0)

#define FL_REQUIRE_LE(lhs, rhs)                                                 \
    do {                                                                         \
        if (!FL_CHECK_LE(lhs, rhs)) return;                                     \
    } while (0)

#define FL_REQUIRE_GE(lhs, rhs)                                                 \
    do {                                                                         \
        if (!FL_CHECK_GE(lhs, rhs)) return;                                     \
    } while (0)

// =============================================================================
// Message and Capture macros
// =============================================================================

// FL_MESSAGE - output a message during test execution (non-failing)
#define FL_MESSAGE(msg)                                                         \
    do {                                                                         \
        fl::StrStream FLTEST_UNIQUE(ss_);                                       \
        FLTEST_UNIQUE(ss_) << msg;                                              \
        fl::test::outputMessage(FLTEST_UNIQUE(ss_).str().c_str(), __FILE__, __LINE__); \
    } while (0)

// FL_INFO - alias for FL_MESSAGE (doctest compatibility)
#define FL_INFO(msg) FL_MESSAGE(msg)

// FL_CAPTURE - capture and print a variable's value
#define FL_CAPTURE(x)                                                           \
    do {                                                                         \
        fl::StrStream FLTEST_UNIQUE(ss_);                                       \
        FLTEST_UNIQUE(ss_) << (x);                                              \
        fl::test::outputCapture(#x, FLTEST_UNIQUE(ss_).str().c_str(), __FILE__, __LINE__); \
    } while (0)

// FL_FAIL - explicit failure (fatal, stops test)
#define FL_FAIL(msg)                                                            \
    do {                                                                         \
        fl::StrStream FLTEST_UNIQUE(ss_);                                       \
        FLTEST_UNIQUE(ss_) << msg;                                              \
        fl::test::fail(FLTEST_UNIQUE(ss_).str().c_str(), __FILE__, __LINE__, true); \
        return;                                                                  \
    } while (0)

// FL_FAIL_CHECK - explicit failure (non-fatal, test continues)
#define FL_FAIL_CHECK(msg)                                                      \
    do {                                                                         \
        fl::StrStream FLTEST_UNIQUE(ss_);                                       \
        FLTEST_UNIQUE(ss_) << msg;                                              \
        fl::test::fail(FLTEST_UNIQUE(ss_).str().c_str(), __FILE__, __LINE__, false); \
    } while (0)

// FL_WARN - warning assertion (logs but doesn't affect pass/fail)
#define FL_WARN(expr)                                                           \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fl::test::outputMessage("Warning: " #expr " is false", __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

// FL_WARN_FALSE - warning assertion for false conditions
#define FL_WARN_FALSE(expr)                                                     \
    do {                                                                         \
        if (expr) {                                                              \
            fl::test::outputMessage("Warning: !(" #expr ") is false", __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

// =============================================================================
// WARN comparison macros - like CHECK variants but don't affect pass/fail
// =============================================================================
// These macros log warnings when conditions aren't met, but don't fail the test

#define FL_WARN_EQ(lhs, rhs)                                                    \
    do {                                                                         \
        if (!((lhs) == (rhs))) {                                                 \
            fl::StrStream _fl_warn_ss;                                           \
            _fl_warn_ss << "Warning: " << #lhs << " == " << #rhs                 \
                       << " failed: " << (lhs) << " != " << (rhs);               \
            fl::test::outputMessage(_fl_warn_ss.str().c_str(), __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

#define FL_WARN_NE(lhs, rhs)                                                    \
    do {                                                                         \
        if (!((lhs) != (rhs))) {                                                 \
            fl::StrStream _fl_warn_ss;                                           \
            _fl_warn_ss << "Warning: " << #lhs << " != " << #rhs                 \
                       << " failed: both equal " << (lhs);                       \
            fl::test::outputMessage(_fl_warn_ss.str().c_str(), __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

#define FL_WARN_LT(lhs, rhs)                                                    \
    do {                                                                         \
        if (!((lhs) < (rhs))) {                                                  \
            fl::StrStream _fl_warn_ss;                                           \
            _fl_warn_ss << "Warning: " << #lhs << " < " << #rhs                  \
                       << " failed: " << (lhs) << " >= " << (rhs);               \
            fl::test::outputMessage(_fl_warn_ss.str().c_str(), __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

#define FL_WARN_GT(lhs, rhs)                                                    \
    do {                                                                         \
        if (!((lhs) > (rhs))) {                                                  \
            fl::StrStream _fl_warn_ss;                                           \
            _fl_warn_ss << "Warning: " << #lhs << " > " << #rhs                  \
                       << " failed: " << (lhs) << " <= " << (rhs);               \
            fl::test::outputMessage(_fl_warn_ss.str().c_str(), __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

#define FL_WARN_LE(lhs, rhs)                                                    \
    do {                                                                         \
        if (!((lhs) <= (rhs))) {                                                 \
            fl::StrStream _fl_warn_ss;                                           \
            _fl_warn_ss << "Warning: " << #lhs << " <= " << #rhs                 \
                       << " failed: " << (lhs) << " > " << (rhs);                \
            fl::test::outputMessage(_fl_warn_ss.str().c_str(), __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

#define FL_WARN_GE(lhs, rhs)                                                    \
    do {                                                                         \
        if (!((lhs) >= (rhs))) {                                                 \
            fl::StrStream _fl_warn_ss;                                           \
            _fl_warn_ss << "Warning: " << #lhs << " >= " << #rhs                 \
                       << " failed: " << (lhs) << " < " << (rhs);                \
            fl::test::outputMessage(_fl_warn_ss.str().c_str(), __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

// FL_SKIP - skip the current test with a reason
// Usage: FL_SKIP("Not implemented yet");
#define FL_SKIP(reason)                                                         \
    do {                                                                         \
        fl::test::skipTest(reason, __FILE__, __LINE__);                         \
        return;                                                                  \
    } while (0)

// =============================================================================
// CHECK_MESSAGE / REQUIRE_MESSAGE - assertions with custom messages
// =============================================================================
// These macros allow adding a custom message when an assertion fails
// Usage: FL_CHECK_MESSAGE(x > 0, "x should be positive, got: " << x);

#define FL_CHECK_MESSAGE(expr, msg)                                             \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fl::StrStream FLTEST_UNIQUE(ss_);                                   \
            FLTEST_UNIQUE(ss_) << msg;                                          \
            fl::test::AssertResult ar(false);                                    \
            ar.mExpression = #expr;                                              \
            ar.mExpanded = FLTEST_UNIQUE(ss_).str();                            \
            ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);        \
            fl::test::TestContext::instance().reportAssert(ar);                 \
        } else {                                                                 \
            fl::test::AssertResult ar(true);                                    \
            ar.mExpression = #expr;                                              \
            ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);        \
            fl::test::TestContext::instance().reportAssert(ar);                 \
        }                                                                        \
    } while (0)

#define FL_REQUIRE_MESSAGE(expr, msg)                                           \
    do {                                                                         \
        if (!(expr)) {                                                           \
            fl::StrStream FLTEST_UNIQUE(ss_);                                   \
            FLTEST_UNIQUE(ss_) << msg;                                          \
            fl::test::AssertResult ar(false);                                    \
            ar.mExpression = #expr;                                              \
            ar.mExpanded = FLTEST_UNIQUE(ss_).str();                            \
            ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);        \
            fl::test::TestContext::instance().reportAssert(ar);                 \
            return;                                                              \
        } else {                                                                 \
            fl::test::AssertResult ar(true);                                    \
            ar.mExpression = #expr;                                              \
            ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);        \
            fl::test::TestContext::instance().reportAssert(ar);                 \
        }                                                                        \
    } while (0)

// =============================================================================
// TEST_SUITE macro - groups tests under a named suite
// =============================================================================
// TEST_SUITE works by defining a namespace scope for test registration.
// Tests registered inside the suite will have the suite name prepended.

// Helper to track current suite name
namespace fl {
namespace test {
namespace detail {
    inline const char*& currentSuiteName() {
        static const char* name = nullptr;
        return name;
    }

    struct SuiteScope {
        const char* mPreviousName;
        SuiteScope(const char* name) {
            mPreviousName = currentSuiteName();
            currentSuiteName() = name;
        }
        ~SuiteScope() {
            currentSuiteName() = mPreviousName;
        }
    };
} // namespace detail
} // namespace test
} // namespace fl

// Note: FL_TEST_SUITE creates a namespace scope. Usage:
// FL_TEST_SUITE("MySuite") {
//     FL_TEST_CASE("test 1") { ... }
//     FL_TEST_CASE("test 2") { ... }
// }
// This will name tests as "MySuite/test 1" and "MySuite/test 2"
#define FL_TEST_SUITE(name)                                                     \
    namespace FLTEST_UNIQUE(FLTEST_SUITE_NS_) {                                 \
        static fl::test::detail::SuiteScope FLTEST_UNIQUE(FLTEST_SUITE_SCOPE_)(name); \
    }                                                                           \
    namespace FLTEST_UNIQUE(FLTEST_SUITE_NS_)

// Alternative TEST_SUITE syntax without braces:
// FL_TEST_SUITE_BEGIN("MySuite")
// FL_TEST_CASE("test 1") { ... }
// FL_TEST_CASE("test 2") { ... }
// FL_TEST_SUITE_END()
//
// This is useful for migration from doctest TEST_SUITE_BEGIN/END.
// Note: Unlike FL_TEST_SUITE, BEGIN/END creates a file-level scope,
// so all tests between BEGIN and END are in the suite.
#define FL_TEST_SUITE_BEGIN(name)                                               \
    namespace {                                                                  \
        static fl::test::detail::SuiteScope FLTEST_UNIQUE(FLTEST_SUITE_SCOPE_)(name);

#define FL_TEST_SUITE_END()                                                     \
    }

// =============================================================================
// BDD-style macros (SCENARIO/GIVEN/WHEN/THEN)
// =============================================================================
// These macros provide BDD (Behavior-Driven Development) syntax sugar.
// They're essentially wrappers around TEST_CASE and SUBCASE with prefixes.

#define FL_SCENARIO(name) FL_TEST_CASE("Scenario: " name)
#define FL_GIVEN(name)    FL_SUBCASE("Given: " name)
#define FL_WHEN(name)     FL_SUBCASE("When: " name)
#define FL_AND_WHEN(name) FL_SUBCASE("And when: " name)
#define FL_THEN(name)     FL_SUBCASE("Then: " name)
#define FL_AND_THEN(name) FL_SUBCASE("And: " name)

// =============================================================================
// CHECK_CLOSE - absolute tolerance floating point comparison
// =============================================================================
// Unlike Approx which uses relative epsilon, CHECK_CLOSE uses absolute tolerance

#define FL_CHECK_CLOSE(a, b, epsilon)                                           \
    do {                                                                         \
        auto _fl_a = (a);                                                        \
        auto _fl_b = (b);                                                        \
        auto _fl_diff = _fl_a - _fl_b;                                           \
        if (_fl_diff < 0) _fl_diff = -_fl_diff;                                  \
        bool _fl_result = _fl_diff <= (epsilon);                                 \
        fl::test::AssertResult ar(_fl_result);                                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream ss;                                                         \
        ss << #a << " ~= " << #b << " (eps=" << (epsilon) << ")";                \
        ar.mExpression = ss.str();                                               \
        if (!_fl_result) {                                                       \
            fl::StrStream ess;                                                    \
            ess << _fl_a << " ~= " << _fl_b << " (diff=" << _fl_diff << ")";     \
            ar.mExpanded = ess.str();                                            \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

#define FL_REQUIRE_CLOSE(a, b, epsilon)                                         \
    do {                                                                         \
        auto _fl_a = (a);                                                        \
        auto _fl_b = (b);                                                        \
        auto _fl_diff = _fl_a - _fl_b;                                           \
        if (_fl_diff < 0) _fl_diff = -_fl_diff;                                  \
        bool _fl_result = _fl_diff <= (epsilon);                                 \
        fl::test::AssertResult ar(_fl_result);                                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream ss;                                                         \
        ss << #a << " ~= " << #b << " (eps=" << (epsilon) << ")";                \
        ar.mExpression = ss.str();                                               \
        if (!_fl_result) {                                                       \
            fl::StrStream ess;                                                    \
            ess << _fl_a << " ~= " << _fl_b << " (diff=" << _fl_diff << ")";     \
            ar.mExpanded = ess.str();                                            \
            fl::test::TestContext::instance().reportAssert(ar);                  \
            return;                                                              \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

// =============================================================================
// TEST_CASE_FIXTURE - run test with fixture setup/teardown
// =============================================================================
// Creates a test that inherits from a fixture class.
// The fixture's constructor is called before each test run,
// and the destructor is called after.
//
// Usage:
//   struct MyFixture {
//       int value;
//       MyFixture() : value(42) {}
//       ~MyFixture() { /* cleanup */ }
//   };
//
//   FL_TEST_CASE_FIXTURE(MyFixture, "test name") {
//       FL_CHECK_EQ(value, 42);  // Accesses fixture members directly
//   }

#define FL_TEST_CASE_FIXTURE(fixture, name)                                     \
    struct FLTEST_UNIQUE(FLTEST_FIXTURE_) : public fixture {                    \
        void run();                                                              \
    };                                                                           \
    static void FLTEST_UNIQUE(FLTEST_FIXTURE_FUNC_)() {                         \
        FLTEST_UNIQUE(FLTEST_FIXTURE_) instance;                                \
        instance.run();                                                          \
    }                                                                           \
    static fl::test::TestRegistrar FLTEST_UNIQUE(FLTEST_FIXTURE_REG_)(          \
        FLTEST_UNIQUE(FLTEST_FIXTURE_FUNC_), name, __FILE__, __LINE__);         \
    void FLTEST_UNIQUE(FLTEST_FIXTURE_)::run()

// =============================================================================
// String comparison macros
// =============================================================================
// Useful for comparing string types without needing iostream

#define FL_CHECK_STR_EQ(a, b)                                                   \
    do {                                                                         \
        fl::string _fl_a_str(a);                                                \
        fl::string _fl_b_str(b);                                                \
        bool _fl_result = (_fl_a_str == _fl_b_str);                             \
        fl::test::AssertResult ar(_fl_result);                                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream ss;                                                         \
        ss << #a << " == " << #b;                                                \
        ar.mExpression = ss.str();                                               \
        if (!_fl_result) {                                                       \
            fl::StrStream ess;                                                    \
            ess << "\"" << _fl_a_str << "\" != \"" << _fl_b_str << "\"";         \
            ar.mExpanded = ess.str();                                            \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

#define FL_CHECK_STR_NE(a, b)                                                   \
    do {                                                                         \
        fl::string _fl_a_str(a);                                                \
        fl::string _fl_b_str(b);                                                \
        bool _fl_result = (_fl_a_str != _fl_b_str);                             \
        fl::test::AssertResult ar(_fl_result);                                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream ss;                                                         \
        ss << #a << " != " << #b;                                                \
        ar.mExpression = ss.str();                                               \
        if (!_fl_result) {                                                       \
            fl::StrStream ess;                                                    \
            ess << "Both equal: \"" << _fl_a_str << "\"";                        \
            ar.mExpanded = ess.str();                                            \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

// Check if string contains substring
#define FL_CHECK_STR_CONTAINS(haystack, needle)                                 \
    do {                                                                         \
        fl::string _fl_haystack(haystack);                                      \
        fl::string _fl_needle(needle);                                          \
        bool _fl_result = (_fl_haystack.find(_fl_needle) != fl::string::npos);  \
        fl::test::AssertResult ar(_fl_result);                                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream _fl_ss;                                                    \
        _fl_ss << #haystack << " contains " << #needle;                          \
        ar.mExpression = _fl_ss.str();                                           \
        if (!_fl_result) {                                                       \
            fl::StrStream _fl_ess;                                               \
            _fl_ess << "\"" << _fl_haystack << "\" does not contain \"" << _fl_needle << "\""; \
            ar.mExpanded = _fl_ess.str();                                        \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

#define FL_REQUIRE_STR_EQ(a, b)                                                 \
    do {                                                                         \
        fl::string _fl_a_str(a);                                                \
        fl::string _fl_b_str(b);                                                \
        bool _fl_result = (_fl_a_str == _fl_b_str);                             \
        fl::test::AssertResult ar(_fl_result);                                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream ss;                                                         \
        ss << #a << " == " << #b;                                                \
        ar.mExpression = ss.str();                                               \
        if (!_fl_result) {                                                       \
            fl::StrStream ess;                                                    \
            ess << "\"" << _fl_a_str << "\" != \"" << _fl_b_str << "\"";         \
            ar.mExpanded = ess.str();                                            \
            fl::test::TestContext::instance().reportAssert(ar);                  \
            return;                                                              \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

#define FL_REQUIRE_STR_CONTAINS(haystack, needle)                               \
    do {                                                                         \
        fl::string _fl_haystack(haystack);                                      \
        fl::string _fl_needle(needle);                                          \
        bool _fl_result = (_fl_haystack.find(_fl_needle) != fl::string::npos);  \
        fl::test::AssertResult ar(_fl_result);                                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream _fl_ss;                                                    \
        _fl_ss << #haystack << " contains " << #needle;                          \
        ar.mExpression = _fl_ss.str();                                           \
        if (!_fl_result) {                                                       \
            fl::StrStream _fl_ess;                                               \
            _fl_ess << "\"" << _fl_haystack << "\" does not contain \"" << _fl_needle << "\""; \
            ar.mExpanded = _fl_ess.str();                                        \
            fl::test::TestContext::instance().reportAssert(ar);                  \
            return;                                                              \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

// =============================================================================
// Array/Container comparison macros
// =============================================================================
// Compare arrays element-by-element with detailed mismatch reporting

// FL_CHECK_ARRAY_EQ - Compare two arrays/spans element by element
// Usage: FL_CHECK_ARRAY_EQ(actual, expected, size)
// Note: Uses a lambda to encapsulate the loop to avoid C++11 scope issues
#define FL_CHECK_ARRAY_EQ(actual, expected, arrsize)                            \
    do {                                                                         \
        bool _fl_arr_match = true;                                               \
        fl::size _fl_arr_mismatch_idx = 0;                                       \
        {                                                                        \
            fl::size _fl_arr_i = 0;                                              \
            fl::size _fl_arr_sz = static_cast<fl::size>(arrsize);                \
            while (_fl_arr_i < _fl_arr_sz) {                                     \
                if (!((actual)[_fl_arr_i] == (expected)[_fl_arr_i])) {           \
                    _fl_arr_match = false;                                       \
                    _fl_arr_mismatch_idx = _fl_arr_i;                            \
                    break;                                                       \
                }                                                                \
                ++_fl_arr_i;                                                     \
            }                                                                    \
        }                                                                        \
        fl::test::AssertResult _fl_arr_ar(_fl_arr_match);                        \
        _fl_arr_ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);     \
        fl::StrStream _fl_arr_ss;                                                \
        _fl_arr_ss << #actual << " == " << #expected << " (size=" << (arrsize) << ")"; \
        _fl_arr_ar.mExpression = _fl_arr_ss.str();                               \
        if (!_fl_arr_match) {                                                    \
            fl::StrStream _fl_arr_ess;                                           \
            _fl_arr_ess << "Mismatch at index " << _fl_arr_mismatch_idx          \
                    << ": " << (actual)[_fl_arr_mismatch_idx]                    \
                    << " != " << (expected)[_fl_arr_mismatch_idx];               \
            _fl_arr_ar.mExpanded = _fl_arr_ess.str();                            \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(_fl_arr_ar);              \
    } while (0)

#define FL_REQUIRE_ARRAY_EQ(actual, expected, arrsize)                          \
    do {                                                                         \
        bool _fl_arr_match = true;                                               \
        fl::size _fl_arr_mismatch_idx = 0;                                       \
        {                                                                        \
            fl::size _fl_arr_i = 0;                                              \
            fl::size _fl_arr_sz = static_cast<fl::size>(arrsize);                \
            while (_fl_arr_i < _fl_arr_sz) {                                     \
                if (!((actual)[_fl_arr_i] == (expected)[_fl_arr_i])) {           \
                    _fl_arr_match = false;                                       \
                    _fl_arr_mismatch_idx = _fl_arr_i;                            \
                    break;                                                       \
                }                                                                \
                ++_fl_arr_i;                                                     \
            }                                                                    \
        }                                                                        \
        fl::test::AssertResult _fl_arr_ar(_fl_arr_match);                        \
        _fl_arr_ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);     \
        fl::StrStream _fl_arr_ss;                                                \
        _fl_arr_ss << #actual << " == " << #expected << " (size=" << (arrsize) << ")"; \
        _fl_arr_ar.mExpression = _fl_arr_ss.str();                               \
        if (!_fl_arr_match) {                                                    \
            fl::StrStream _fl_arr_ess;                                           \
            _fl_arr_ess << "Mismatch at index " << _fl_arr_mismatch_idx          \
                    << ": " << (actual)[_fl_arr_mismatch_idx]                    \
                    << " != " << (expected)[_fl_arr_mismatch_idx];               \
            _fl_arr_ar.mExpanded = _fl_arr_ess.str();                            \
            fl::test::TestContext::instance().reportAssert(_fl_arr_ar);          \
            return;                                                              \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(_fl_arr_ar);              \
    } while (0)

// FL_CHECK_UNARY_PRED - Check that a unary predicate holds for an expression
// Usage: FL_CHECK_UNARY_PRED(pred, expr) where pred is a callable
// Note: Removed - requires C++14 (auto in lambdas). Use FL_CHECK directly instead.
// Example: FL_CHECK(isPositive(42)); FL_CHECK(isEven(100));

// =============================================================================
// Exception testing macros (for platforms that support exceptions)
// =============================================================================
// Note: On embedded devices, exceptions may be disabled. These macros
// gracefully handle that case.

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS)
#define FLTEST_EXCEPTIONS_ENABLED 1
#else
#define FLTEST_EXCEPTIONS_ENABLED 0
#endif

#if FLTEST_EXCEPTIONS_ENABLED

#define FL_CHECK_THROWS(expr)                                                   \
    do {                                                                         \
        bool _fl_threw = false;                                                  \
        try { (void)(expr); }                                                    \
        catch (...) { _fl_threw = true; }                                        \
        fl::test::AssertResult ar(_fl_threw);                                    \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        ar.mExpression = #expr " throws";                                        \
        if (!_fl_threw) {                                                        \
            ar.mExpanded = "No exception was thrown";                            \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

#define FL_CHECK_NOTHROW(expr)                                                  \
    do {                                                                         \
        bool _fl_threw = false;                                                  \
        try { (void)(expr); }                                                    \
        catch (...) { _fl_threw = true; }                                        \
        fl::test::AssertResult ar(!_fl_threw);                                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        ar.mExpression = #expr " nothrow";                                       \
        if (_fl_threw) {                                                         \
            ar.mExpanded = "An exception was thrown";                            \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

#define FL_REQUIRE_THROWS(expr)                                                 \
    do {                                                                         \
        bool _fl_threw = false;                                                  \
        try { (void)(expr); }                                                    \
        catch (...) { _fl_threw = true; }                                        \
        fl::test::AssertResult ar(_fl_threw);                                    \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        ar.mExpression = #expr " throws";                                        \
        if (!_fl_threw) {                                                        \
            ar.mExpanded = "No exception was thrown";                            \
            fl::test::TestContext::instance().reportAssert(ar);                  \
            return;                                                              \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

#define FL_REQUIRE_NOTHROW(expr)                                                \
    do {                                                                         \
        bool _fl_threw = false;                                                  \
        try { (void)(expr); }                                                    \
        catch (...) { _fl_threw = true; }                                        \
        fl::test::AssertResult ar(!_fl_threw);                                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        ar.mExpression = #expr " nothrow";                                       \
        if (_fl_threw) {                                                         \
            ar.mExpanded = "An exception was thrown";                            \
            fl::test::TestContext::instance().reportAssert(ar);                  \
            return;                                                              \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

// CHECK_THROWS_AS - Check that expression throws a specific exception type
// Note: exType must be a complete type (e.g., std::runtime_error, int, MyException)
#define FL_CHECK_THROWS_AS(expr, exType)                                        \
    do {                                                                         \
        bool _fl_threw_correct = false;                                          \
        bool _fl_threw_any = false;                                              \
        try { (void)(expr); }                                                    \
        catch (const exType&) { _fl_threw_correct = true; _fl_threw_any = true; } \
        catch (...) { _fl_threw_any = true; }                                    \
        fl::test::AssertResult ar(_fl_threw_correct);                            \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream _fl_expr_ss;                                               \
        _fl_expr_ss << #expr << " throws " << #exType;                           \
        ar.mExpression = _fl_expr_ss.str();                                      \
        if (!_fl_threw_correct) {                                                \
            if (_fl_threw_any) {                                                 \
                ar.mExpanded = "Threw a different exception type";               \
            } else {                                                             \
                ar.mExpanded = "No exception was thrown";                        \
            }                                                                    \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

#define FL_REQUIRE_THROWS_AS(expr, exType)                                      \
    do {                                                                         \
        bool _fl_threw_correct = false;                                          \
        bool _fl_threw_any = false;                                              \
        try { (void)(expr); }                                                    \
        catch (const exType&) { _fl_threw_correct = true; _fl_threw_any = true; } \
        catch (...) { _fl_threw_any = true; }                                    \
        fl::test::AssertResult ar(_fl_threw_correct);                            \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream _fl_expr_ss;                                               \
        _fl_expr_ss << #expr << " throws " << #exType;                           \
        ar.mExpression = _fl_expr_ss.str();                                      \
        if (!_fl_threw_correct) {                                                \
            if (_fl_threw_any) {                                                 \
                ar.mExpanded = "Threw a different exception type";               \
            } else {                                                             \
                ar.mExpanded = "No exception was thrown";                        \
            }                                                                    \
            fl::test::TestContext::instance().reportAssert(ar);                  \
            return;                                                              \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

// CHECK_THROWS_WITH - Check that expression throws and exception.what() contains message
// Note: This only works with exceptions derived from std::exception
#define FL_CHECK_THROWS_WITH(expr, msg)                                         \
    do {                                                                         \
        bool _fl_threw = false;                                                  \
        bool _fl_msg_match = false;                                              \
        fl::string _fl_actual_msg;                                               \
        try { (void)(expr); }                                                    \
        catch (const std::exception& e) {                                        \
            _fl_threw = true;                                                    \
            _fl_actual_msg = e.what();                                           \
            _fl_msg_match = (_fl_actual_msg.find(msg) != fl::string::npos);      \
        }                                                                        \
        catch (...) { _fl_threw = true; }                                        \
        fl::test::AssertResult ar(_fl_threw && _fl_msg_match);                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream _fl_expr_ss;                                               \
        _fl_expr_ss << #expr << " throws with \"" << msg << "\"";                \
        ar.mExpression = _fl_expr_ss.str();                                      \
        if (!_fl_threw) {                                                        \
            ar.mExpanded = "No exception was thrown";                            \
        } else if (!_fl_msg_match) {                                             \
            fl::StrStream _fl_exp_ss;                                            \
            _fl_exp_ss << "Exception message: \"" << _fl_actual_msg << "\"";     \
            ar.mExpanded = _fl_exp_ss.str();                                     \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
    } while (0)

#define FL_REQUIRE_THROWS_WITH(expr, msg)                                       \
    do {                                                                         \
        bool _fl_threw = false;                                                  \
        bool _fl_msg_match = false;                                              \
        fl::string _fl_actual_msg;                                               \
        try { (void)(expr); }                                                    \
        catch (const std::exception& e) {                                        \
            _fl_threw = true;                                                    \
            _fl_actual_msg = e.what();                                           \
            _fl_msg_match = (_fl_actual_msg.find(msg) != fl::string::npos);      \
        }                                                                        \
        catch (...) { _fl_threw = true; }                                        \
        fl::test::AssertResult ar(_fl_threw && _fl_msg_match);                   \
        ar.mLocation = fl::test::SourceLocation(__FILE__, __LINE__);             \
        fl::StrStream _fl_expr_ss;                                               \
        _fl_expr_ss << #expr << " throws with \"" << msg << "\"";                \
        ar.mExpression = _fl_expr_ss.str();                                      \
        if (!_fl_threw) {                                                        \
            ar.mExpanded = "No exception was thrown";                            \
        } else if (!_fl_msg_match) {                                             \
            fl::StrStream _fl_exp_ss;                                            \
            _fl_exp_ss << "Exception message: \"" << _fl_actual_msg << "\"";     \
            ar.mExpanded = _fl_exp_ss.str();                                     \
        }                                                                        \
        fl::test::TestContext::instance().reportAssert(ar);                      \
        if (!(_fl_threw && _fl_msg_match)) return;                               \
    } while (0)

// =============================================================================
// WARN exception macros - log warnings but don't affect pass/fail
// =============================================================================
// These macros check exception behavior but only log warnings, never failing the test

#define FL_WARN_THROWS(expr)                                                    \
    do {                                                                         \
        bool _fl_threw = false;                                                  \
        try { (void)(expr); }                                                    \
        catch (...) { _fl_threw = true; }                                        \
        if (!_fl_threw) {                                                        \
            fl::test::outputMessage("Warning: " #expr " did not throw", __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

#define FL_WARN_NOTHROW(expr)                                                   \
    do {                                                                         \
        bool _fl_threw = false;                                                  \
        try { (void)(expr); }                                                    \
        catch (...) { _fl_threw = true; }                                        \
        if (_fl_threw) {                                                         \
            fl::test::outputMessage("Warning: " #expr " threw an exception", __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

#define FL_WARN_THROWS_AS(expr, exType)                                         \
    do {                                                                         \
        bool _fl_threw_correct = false;                                          \
        bool _fl_threw_any = false;                                              \
        try { (void)(expr); }                                                    \
        catch (const exType&) { _fl_threw_correct = true; _fl_threw_any = true; } \
        catch (...) { _fl_threw_any = true; }                                    \
        if (!_fl_threw_correct) {                                                \
            if (_fl_threw_any) {                                                 \
                fl::test::outputMessage("Warning: " #expr " threw different type than " #exType, __FILE__, __LINE__); \
            } else {                                                             \
                fl::test::outputMessage("Warning: " #expr " did not throw " #exType, __FILE__, __LINE__); \
            }                                                                    \
        }                                                                        \
    } while (0)

#define FL_WARN_THROWS_WITH(expr, msg)                                          \
    do {                                                                         \
        bool _fl_threw = false;                                                  \
        bool _fl_msg_match = false;                                              \
        fl::string _fl_actual_msg;                                               \
        try { (void)(expr); }                                                    \
        catch (const std::exception& e) {                                        \
            _fl_threw = true;                                                    \
            _fl_actual_msg = e.what();                                           \
            _fl_msg_match = (_fl_actual_msg.find(msg) != fl::string::npos);      \
        }                                                                        \
        catch (...) { _fl_threw = true; }                                        \
        if (!_fl_threw) {                                                        \
            fl::test::outputMessage("Warning: " #expr " did not throw", __FILE__, __LINE__); \
        } else if (!_fl_msg_match) {                                             \
            fl::StrStream _fl_warn_ss;                                           \
            _fl_warn_ss << "Warning: " #expr " threw but message \"" << _fl_actual_msg \
                       << "\" does not contain \"" << msg << "\"";               \
            fl::test::outputMessage(_fl_warn_ss.str().c_str(), __FILE__, __LINE__); \
        }                                                                        \
    } while (0)

#else // !FLTEST_EXCEPTIONS_ENABLED

// Stub macros for platforms without exception support
#define FL_CHECK_THROWS(expr)                                                   \
    do {                                                                         \
        fl::test::outputMessage("CHECK_THROWS skipped (exceptions disabled)", __FILE__, __LINE__); \
    } while (0)

#define FL_CHECK_NOTHROW(expr)                                                  \
    do { (void)(expr); } while (0)  // Just execute it

#define FL_REQUIRE_THROWS(expr)                                                 \
    do {                                                                         \
        fl::test::outputMessage("REQUIRE_THROWS skipped (exceptions disabled)", __FILE__, __LINE__); \
    } while (0)

#define FL_REQUIRE_NOTHROW(expr)                                                \
    do { (void)(expr); } while (0)  // Just execute it

#define FL_CHECK_THROWS_AS(expr, exType)                                        \
    do {                                                                         \
        fl::test::outputMessage("CHECK_THROWS_AS skipped (exceptions disabled)", __FILE__, __LINE__); \
    } while (0)

#define FL_REQUIRE_THROWS_AS(expr, exType)                                      \
    do {                                                                         \
        fl::test::outputMessage("REQUIRE_THROWS_AS skipped (exceptions disabled)", __FILE__, __LINE__); \
    } while (0)

#define FL_CHECK_THROWS_WITH(expr, msg)                                         \
    do {                                                                         \
        fl::test::outputMessage("CHECK_THROWS_WITH skipped (exceptions disabled)", __FILE__, __LINE__); \
    } while (0)

#define FL_REQUIRE_THROWS_WITH(expr, msg)                                       \
    do {                                                                         \
        fl::test::outputMessage("REQUIRE_THROWS_WITH skipped (exceptions disabled)", __FILE__, __LINE__); \
    } while (0)

#define FL_WARN_THROWS(expr)                                                    \
    do {                                                                         \
        fl::test::outputMessage("WARN_THROWS skipped (exceptions disabled)", __FILE__, __LINE__); \
    } while (0)

#define FL_WARN_NOTHROW(expr)                                                   \
    do { (void)(expr); } while (0)

#define FL_WARN_THROWS_AS(expr, exType)                                         \
    do {                                                                         \
        fl::test::outputMessage("WARN_THROWS_AS skipped (exceptions disabled)", __FILE__, __LINE__); \
    } while (0)

#define FL_WARN_THROWS_WITH(expr, msg)                                          \
    do {                                                                         \
        fl::test::outputMessage("WARN_THROWS_WITH skipped (exceptions disabled)", __FILE__, __LINE__); \
    } while (0)

#endif // FLTEST_EXCEPTIONS_ENABLED

// =============================================================================
// Compatibility aliases for gradual migration from doctest
// =============================================================================
#ifdef FLTEST_ENABLE_DOCTEST_COMPAT
#define TEST_CASE(name) FL_TEST_CASE(name)
#define SUBCASE(name) FL_SUBCASE(name)
#define CHECK(expr) FL_CHECK(expr)
#define CHECK_FALSE(expr) FL_CHECK_FALSE(expr)
#define CHECK_EQ(a, b) FL_CHECK_EQ(a, b)
#define CHECK_NE(a, b) FL_CHECK_NE(a, b)
#define CHECK_LT(a, b) FL_CHECK_LT(a, b)
#define CHECK_GT(a, b) FL_CHECK_GT(a, b)
#define CHECK_LE(a, b) FL_CHECK_LE(a, b)
#define CHECK_GE(a, b) FL_CHECK_GE(a, b)
#define REQUIRE(expr) FL_REQUIRE(expr)
#define REQUIRE_FALSE(expr) FL_REQUIRE_FALSE(expr)
#define REQUIRE_EQ(a, b) FL_REQUIRE_EQ(a, b)
#define REQUIRE_NE(a, b) FL_REQUIRE_NE(a, b)
#define REQUIRE_LT(a, b) FL_REQUIRE_LT(a, b)
#define REQUIRE_GT(a, b) FL_REQUIRE_GT(a, b)
#define REQUIRE_LE(a, b) FL_REQUIRE_LE(a, b)
#define REQUIRE_GE(a, b) FL_REQUIRE_GE(a, b)
#define MESSAGE(msg) FL_MESSAGE(msg)
#define INFO(msg) FL_INFO(msg)
#define CAPTURE(x) FL_CAPTURE(x)
#define FAIL(msg) FL_FAIL(msg)
#define FAIL_CHECK(msg) FL_FAIL_CHECK(msg)
#define WARN(expr) FL_WARN(expr)
// WARN comparison macros (log warnings but don't fail)
#define WARN_FALSE(expr) FL_WARN_FALSE(expr)
#define WARN_EQ(a, b) FL_WARN_EQ(a, b)
#define WARN_NE(a, b) FL_WARN_NE(a, b)
#define WARN_LT(a, b) FL_WARN_LT(a, b)
#define WARN_GT(a, b) FL_WARN_GT(a, b)
#define WARN_LE(a, b) FL_WARN_LE(a, b)
#define WARN_GE(a, b) FL_WARN_GE(a, b)
// CHECK_MESSAGE/REQUIRE_MESSAGE for assertions with custom messages
#define CHECK_MESSAGE(expr, msg) FL_CHECK_MESSAGE(expr, msg)
#define REQUIRE_MESSAGE(expr, msg) FL_REQUIRE_MESSAGE(expr, msg)
// BDD-style macros
#define SCENARIO(name) FL_SCENARIO(name)
#define GIVEN(name) FL_GIVEN(name)
#define WHEN(name) FL_WHEN(name)
#define AND_WHEN(name) FL_AND_WHEN(name)
#define THEN(name) FL_THEN(name)
#define AND_THEN(name) FL_AND_THEN(name)
// Fixture and suite
#define TEST_CASE_FIXTURE(fixture, name) FL_TEST_CASE_FIXTURE(fixture, name)
#define TEST_SUITE(name) FL_TEST_SUITE(name)
// CHECK_CLOSE for absolute tolerance
#define CHECK_CLOSE(a, b, eps) FL_CHECK_CLOSE(a, b, eps)
#define REQUIRE_CLOSE(a, b, eps) FL_REQUIRE_CLOSE(a, b, eps)
// String comparison macros
#define CHECK_STR_EQ(a, b) FL_CHECK_STR_EQ(a, b)
#define CHECK_STR_NE(a, b) FL_CHECK_STR_NE(a, b)
#define CHECK_STR_CONTAINS(str, substr) FL_CHECK_STR_CONTAINS(str, substr)
#define REQUIRE_STR_EQ(a, b) FL_REQUIRE_STR_EQ(a, b)
#define REQUIRE_STR_CONTAINS(str, substr) FL_REQUIRE_STR_CONTAINS(str, substr)
// Exception testing macros
#define CHECK_THROWS(expr) FL_CHECK_THROWS(expr)
#define CHECK_NOTHROW(expr) FL_CHECK_NOTHROW(expr)
#define REQUIRE_THROWS(expr) FL_REQUIRE_THROWS(expr)
#define REQUIRE_NOTHROW(expr) FL_REQUIRE_NOTHROW(expr)
#define CHECK_THROWS_AS(expr, exType) FL_CHECK_THROWS_AS(expr, exType)
#define REQUIRE_THROWS_AS(expr, exType) FL_REQUIRE_THROWS_AS(expr, exType)
#define CHECK_THROWS_WITH(expr, msg) FL_CHECK_THROWS_WITH(expr, msg)
#define REQUIRE_THROWS_WITH(expr, msg) FL_REQUIRE_THROWS_WITH(expr, msg)
// WARN exception macros (log warnings but don't fail)
#define WARN_THROWS(expr) FL_WARN_THROWS(expr)
#define WARN_NOTHROW(expr) FL_WARN_NOTHROW(expr)
#define WARN_THROWS_AS(expr, exType) FL_WARN_THROWS_AS(expr, exType)
#define WARN_THROWS_WITH(expr, msg) FL_WARN_THROWS_WITH(expr, msg)
// Skip macro
#define SKIP(reason) FL_SKIP(reason)
// Array/container comparison macros
#define CHECK_ARRAY_EQ(actual, expected, size) FL_CHECK_ARRAY_EQ(actual, expected, size)
#define REQUIRE_ARRAY_EQ(actual, expected, size) FL_REQUIRE_ARRAY_EQ(actual, expected, size)
// Template test macros
#define TYPE_TO_STRING(type, str) FL_TYPE_TO_STRING(type, str)
#define TYPE_TO_STRING_AS(str, type) FL_TYPE_TO_STRING_AS(str, type)
#define TEST_CASE_TEMPLATE(name, T, ...) FL_TEST_CASE_TEMPLATE(name, T, __VA_ARGS__)
#define TEST_CASE_TEMPLATE_DEFINE(name, T, id) FL_TEST_CASE_TEMPLATE_DEFINE(name, T, id)
#define TEST_CASE_TEMPLATE_INVOKE(id, ...) FL_TEST_CASE_TEMPLATE_INVOKE(id, __VA_ARGS__)
#define TEST_CASE_TEMPLATE_APPLY(id, typelist) FL_TEST_CASE_TEMPLATE_APPLY(id, typelist)

// When using doctest compatibility, also provide doctest::Approx alias
namespace doctest {
    using Approx = fl::test::Approx;
}
#endif

// =============================================================================
// TEST_CASE_TEMPLATE - Parameterized type testing
// =============================================================================
// Runs the same test body for multiple types. This is useful for testing
// generic code with different type parameters.
//
// Usage:
//   FL_TEST_CASE_TEMPLATE("vector operations", T, int, float, double) {
//       fl::vector<T> v;
//       v.push_back(T(42));
//       FL_CHECK_EQ(v.size(), 1u);
//   }
//
// The test will run once for each type in the list. The type name is appended
// to the test case name: "vector operations<int>", "vector operations<float>", etc.
//
// For custom type names, use FL_TYPE_TO_STRING:
//   FL_TYPE_TO_STRING(MyType, "MyType")
//   FL_TEST_CASE_TEMPLATE("test", T, MyType, int) { ... }

namespace fl {
namespace test {
namespace detail {

// Type name extraction using compiler-specific intrinsics
// This extracts the type name from __PRETTY_FUNCTION__ or __FUNCSIG__
template <typename T>
inline fl::string getTypeName() {
#if defined(__clang__) || defined(__GNUC__)
    // __PRETTY_FUNCTION__ format: "fl::string fl::test::detail::getTypeName() [T = int]"
    const char* func = __PRETTY_FUNCTION__;
    const char* begin = func;
    // Find "T = "
    while (*begin && !(begin[0] == 'T' && begin[1] == ' ' && begin[2] == '=' && begin[3] == ' '))
        ++begin;
    if (*begin) {
        begin += 4;  // Skip "T = "
        const char* end = begin;
        int depth = 0;
        while (*end) {
            if (*end == '<') ++depth;
            else if (*end == '>') {
                if (depth == 0) break;
                --depth;
            }
            else if (*end == ']' && depth == 0) break;
            ++end;
        }
        return fl::string(begin, static_cast<fl::size>(end - begin));
    }
#elif defined(_MSC_VER)
    // __FUNCSIG__ format: "class fl::string __cdecl fl::test::detail::getTypeName<int>(void)"
    const char* func = __FUNCSIG__;
    const char* begin = func;
    // Find "getTypeName<"
    while (*begin && !(begin[0] == 'g' && begin[1] == 'e' && begin[2] == 't' && begin[3] == 'T' &&
                       begin[4] == 'y' && begin[5] == 'p' && begin[6] == 'e' && begin[7] == 'N' &&
                       begin[8] == 'a' && begin[9] == 'm' && begin[10] == 'e' && begin[11] == '<'))
        ++begin;
    if (*begin) {
        begin += 12;  // Skip "getTypeName<"
        const char* end = begin;
        int depth = 1;  // We're inside the first <
        while (*end && depth > 0) {
            if (*end == '<') ++depth;
            else if (*end == '>') --depth;
            ++end;
        }
        if (end > begin && depth == 0) --end;  // Back up before final >
        return fl::string(begin, static_cast<fl::size>(end - begin));
    }
#endif
    return fl::string("{unknown}");
}

// Specialization holder for custom type names
template <typename T>
struct TypeNameHolder {
    static const char* name() {
        static fl::string cachedName = getTypeName<T>();
        return cachedName.c_str();
    }
};

// TypeList for storing types
template <typename... Ts>
struct TypeList {};

// Helper to iterate over TypeList and register tests
template <typename TL, typename TestFunc>
struct TypeIterator;

// Base case: empty TypeList
template <typename TestFunc>
struct TypeIterator<TypeList<>, TestFunc> {
    static void iterate(const char*, const char*, int, int) {}
};

// Recursive case: TypeList with at least one type
template <typename T, typename... Rest, typename TestFunc>
struct TypeIterator<TypeList<T, Rest...>, TestFunc> {
    static void iterate(const char* baseName, const char* file, int line, int index) {
        // Build the test name: "baseName<TypeName>"
        fl::StrStream ss;
        ss << baseName << "<" << TypeNameHolder<T>::name() << ">";
        fl::string testName = ss.str();

        // Register this instantiation
        TestContext::instance().registerTest(TestFunc::template run<T>, testName.c_str(), file, line);

        // Continue with remaining types
        TypeIterator<TypeList<Rest...>, TestFunc>::iterate(baseName, file, line, index + 1);
    }
};

} // namespace detail
} // namespace test
} // namespace fl

// FL_TYPE_TO_STRING - Define custom stringification for a type
// Usage: FL_TYPE_TO_STRING(MyType, "MyType")
#define FL_TYPE_TO_STRING(type, str)                                            \
    namespace fl { namespace test { namespace detail {                           \
        template <>                                                              \
        struct TypeNameHolder<type> {                                            \
            static const char* name() { return str; }                            \
        };                                                                       \
    }}}

// FL_TYPE_TO_STRING_AS - Alias for FL_TYPE_TO_STRING (doctest compatibility)
#define FL_TYPE_TO_STRING_AS(str, type) FL_TYPE_TO_STRING(type, str)

// FL_TEST_CASE_TEMPLATE - Define a test that runs for multiple types
// Usage: FL_TEST_CASE_TEMPLATE("test name", T, int, float, double) { ... }
//
// Implementation note: We use a struct with a static template method to hold
// the test function, which allows us to instantiate it for each type.
#define FL_TEST_CASE_TEMPLATE(name, T, ...)                                     \
    template <typename T>                                                        \
    static void FLTEST_UNIQUE(FLTEST_TMPL_FUNC_)();                             \
    struct FLTEST_UNIQUE(FLTEST_TMPL_REG_) {                                    \
        template <typename T>                                                    \
        static void run() {                                                      \
            FLTEST_UNIQUE(FLTEST_TMPL_FUNC_)<T>();                              \
        }                                                                        \
    };                                                                           \
    static struct FLTEST_UNIQUE(FLTEST_TMPL_INIT_) {                            \
        FLTEST_UNIQUE(FLTEST_TMPL_INIT_)() {                                    \
            using TL = fl::test::detail::TypeList<__VA_ARGS__>;                 \
            fl::test::detail::TypeIterator<TL, FLTEST_UNIQUE(FLTEST_TMPL_REG_)>::iterate( \
                name, __FILE__, __LINE__, 0);                                   \
        }                                                                        \
    } FLTEST_UNIQUE(FLTEST_TMPL_INST_);                                         \
    template <typename T>                                                        \
    static void FLTEST_UNIQUE(FLTEST_TMPL_FUNC_)()

// FL_TEST_CASE_TEMPLATE_DEFINE - Define a template test without immediate instantiation
// Usage:
//   FL_TEST_CASE_TEMPLATE_DEFINE("test name", T, my_test_id) { ... }
//   FL_TEST_CASE_TEMPLATE_INVOKE(my_test_id, int, float, double)
#define FL_TEST_CASE_TEMPLATE_DEFINE(name, T, id)                               \
    template <typename T>                                                        \
    static void FLTEST_CAT(id, _FUNC_)();                                       \
    struct FLTEST_CAT(id, _REG_) {                                              \
        template <typename T>                                                    \
        static void run() {                                                      \
            FLTEST_CAT(id, _FUNC_)<T>();                                        \
        }                                                                        \
        static const char* baseName() { return name; }                          \
    };                                                                           \
    template <typename T>                                                        \
    static void FLTEST_CAT(id, _FUNC_)()

// FL_TEST_CASE_TEMPLATE_INVOKE - Instantiate a previously defined template test
// Usage: FL_TEST_CASE_TEMPLATE_INVOKE(id, int, float, double);  // Note: semicolon required
#define FL_TEST_CASE_TEMPLATE_INVOKE(id, ...)                                   \
    static struct FLTEST_UNIQUE(FLTEST_TMPL_INVOKE_) {                          \
        FLTEST_UNIQUE(FLTEST_TMPL_INVOKE_)() {                                  \
            using TL = fl::test::detail::TypeList<__VA_ARGS__>;                 \
            fl::test::detail::TypeIterator<TL, FLTEST_CAT(id, _REG_)>::iterate( \
                FLTEST_CAT(id, _REG_)::baseName(), __FILE__, __LINE__, 0);      \
        }                                                                        \
    } FLTEST_UNIQUE(FLTEST_TMPL_INVOKE_INST_);                                  \
    static_assert(true, "")

// FL_TEST_CASE_TEMPLATE_APPLY - Same as INVOKE but takes a TypeList instead of types
// Usage: FL_TEST_CASE_TEMPLATE_APPLY(my_test_id, MyTypeList);  // Note: semicolon required
// where MyTypeList = fl::test::detail::TypeList<int, float, double>
#define FL_TEST_CASE_TEMPLATE_APPLY(id, typelist)                               \
    static struct FLTEST_UNIQUE(FLTEST_TMPL_APPLY_) {                           \
        FLTEST_UNIQUE(FLTEST_TMPL_APPLY_)() {                                   \
            fl::test::detail::TypeIterator<typelist, FLTEST_CAT(id, _REG_)>::iterate( \
                FLTEST_CAT(id, _REG_)::baseName(), __FILE__, __LINE__, 0);      \
        }                                                                        \
    } FLTEST_UNIQUE(FLTEST_TMPL_APPLY_INST_);                                   \
    static_assert(true, "")

// =============================================================================
// Standalone main implementation macro
// =============================================================================
// Define FLTEST_IMPLEMENT_MAIN in exactly one source file to provide a main()
// that runs all registered tests. This is useful for standalone test executables.
//
// Usage:
//   // In your main test file:
//   #define FLTEST_IMPLEMENT_MAIN
//   #include "fl/fltest.h"
//
//   FL_TEST_CASE("my test") { FL_CHECK(1 == 1); }
//
// For embedded devices, you can instead call fl::test::TestContext::instance().run()
// directly from your setup() function.

#ifdef FLTEST_IMPLEMENT_MAIN
int main(int argc, char** argv) {
    return fl::test::TestContext::instance().run(argc, const_cast<const char**>(argv));
}
#endif

// =============================================================================
// Arduino/ESP32 test runner macro
// =============================================================================
// For Arduino-style embedded environments, use this in your setup() function:
//
//   #include "fl/fltest.h"
//
//   void setup() {
//       Serial.begin(115200);
//       FL_RUN_ALL_TESTS_ARDUINO(Serial);
//   }
//
// This macro:
// 1. Creates a SerialReporter that outputs to the provided serial port
// 2. Runs all tests and prints results
// 3. Halts (infinite loop) after tests complete

#define FL_RUN_ALL_TESTS_ARDUINO(serial_obj)                                    \
    do {                                                                         \
        auto _fl_serial_print = [](const char* msg) { serial_obj.print(msg); }; \
        fl::test::SerialReporter _fl_reporter(_fl_serial_print);                \
        fl::test::TestContext::instance().setReporter(&_fl_reporter);           \
        int _fl_result = fl::test::TestContext::instance().run();               \
        if (_fl_result == 0) {                                                   \
            serial_obj.println("\n=== ALL TESTS PASSED ===");                   \
        } else {                                                                 \
            serial_obj.println("\n=== TESTS FAILED ===");                       \
        }                                                                        \
        while (true) { delay(1000); }  /* Halt after tests */                   \
    } while (0)

/// @file fl/fltest.cpp
/// @brief Implementation of the portable test framework

#include "fl/fltest.h"
#include "fl/log.h"
#include "fl/stl/stdio.h"

namespace fl {
namespace test {

// =============================================================================
// DefaultReporter implementation
// =============================================================================

void DefaultReporter::testRunStart() {
    fl::printf("\n");
    fl::printf("===============================================================================\n");
    fl::printf("FL TEST: Running tests...\n");
    fl::printf("===============================================================================\n");
}

void DefaultReporter::testRunEnd(const TestStats& stats) {
    fl::printf("\n");
    fl::printf("===============================================================================\n");
    fl::printf("FL TEST: Results\n");
    fl::printf("-------------------------------------------------------------------------------\n");
    if (stats.mTestCasesSkipped > 0) {
        fl::printf("Test cases: %u passed, %u failed, %u skipped, %u total\n",
                   stats.mTestCasesPassed, stats.mTestCasesFailed,
                   stats.mTestCasesSkipped, stats.mTestCasesRun);
    } else {
        fl::printf("Test cases: %u passed, %u failed, %u total\n",
                   stats.mTestCasesPassed, stats.mTestCasesFailed, stats.mTestCasesRun);
    }
    fl::printf("Assertions: %u passed, %u failed\n",
               stats.mAssertsPassed, stats.mAssertsFailed);
    if (stats.mTotalDurationMs > 0) {
        fl::printf("Duration: %u ms\n", stats.mTotalDurationMs);
    }
    fl::printf("===============================================================================\n");

    if (stats.allPassed()) {
        fl::printf("Status: SUCCESS\n");
    } else {
        fl::printf("Status: FAILURE\n");
    }
    fl::printf("\n");
}

void DefaultReporter::testCaseStart(const char* name) {
    fl::printf("\n--- Test: %s\n", name);
}

void DefaultReporter::testCaseEnd(bool passed, fl::u32 durationMs) {
    if (passed) {
        if (durationMs > 0) {
            fl::printf("    [PASSED] (%u ms)\n", durationMs);
        } else {
            fl::printf("    [PASSED]\n");
        }
    } else {
        if (durationMs > 0) {
            fl::printf("    [FAILED] (%u ms)\n", durationMs);
        } else {
            fl::printf("    [FAILED]\n");
        }
    }
}

void DefaultReporter::subcaseStart(const char* name) {
    fl::printf("  Subcase: %s\n", name);
}

void DefaultReporter::subcaseEnd() {
    // Nothing needed
}

void DefaultReporter::assertResult(const AssertResult& result) {
    if (!result.mPassed) {
        fl::printf("    FAILED: %s:%d\n", result.mLocation.mFile, result.mLocation.mLine);
        fl::printf("    Expression: %s\n", result.mExpression.c_str());
        if (!result.mExpanded.empty()) {
            fl::printf("    Expanded: %s\n", result.mExpanded.c_str());
        }
    }
}

// =============================================================================
// TestContext implementation
// =============================================================================

TestContext::TestContext() {
    mReporter = &mDefaultReporter;
}

TestContext& TestContext::instance() {
    static TestContext ctx;
    return ctx;
}

int TestContext::registerTest(TestFunc func, const char* name, const char* file, int line) {
    TestCaseInfo info;
    info.mFunc = func;
    info.mName = name;
    info.mFile = file;
    info.mLine = line;
    mTestCases.push_back(info);
    return static_cast<int>(mTestCases.size());
}

int TestContext::run(int argc, const char* const* argv) {
    // Extract filter from first arg if present
    const char* filter = nullptr;
    if (argc > 1 && argv && argv[1]) {
        filter = argv[1];
    }
    return run(filter);
}

int TestContext::run(const char* filter) {
    mStats.reset();
    mReporter->testRunStart();

    for (fl::size i = 0; i < mTestCases.size(); ++i) {
        // Apply filter if provided
        if (filter && filter[0] != '\0') {
            if (!matchesFilter(mTestCases[i].mName, filter)) {
                continue;  // Skip tests that don't match
            }
        }
        runTestCase(mTestCases[i]);
    }

    mReporter->testRunEnd(mStats);
    return mStats.allPassed() ? 0 : 1;
}

fl::size TestContext::listTests(const char* filter) const {
    fl::size count = 0;
    fl::printf("\nRegistered tests:\n");
    fl::printf("----------------\n");

    for (fl::size i = 0; i < mTestCases.size(); ++i) {
        // Apply filter if provided
        if (filter && filter[0] != '\0') {
            if (!matchesFilter(mTestCases[i].mName, filter)) {
                continue;
            }
        }
        count++;
        fl::printf("  [%u] %s\n", static_cast<fl::u32>(count), mTestCases[i].mName);
        fl::printf("      File: %s:%d\n", mTestCases[i].mFile, mTestCases[i].mLine);
    }

    fl::printf("----------------\n");
    fl::printf("Total: %u tests\n\n", static_cast<fl::u32>(count));
    return count;
}

bool TestContext::matchesFilter(const char* name, const char* filter) const {
    if (!filter || filter[0] == '\0') {
        return true;  // No filter means match all
    }

    // Simple wildcard matching: * matches any sequence, ? matches one char
    const char* n = name;
    const char* f = filter;

    // Check if filter contains wildcards
    bool hasWildcards = false;
    for (const char* p = filter; *p; ++p) {
        if (*p == '*' || *p == '?') {
            hasWildcards = true;
            break;
        }
    }

    if (!hasWildcards) {
        // No wildcards - do substring match
        while (*n) {
            const char* nn = n;
            const char* ff = filter;
            while (*nn && *ff && *nn == *ff) {
                ++nn;
                ++ff;
            }
            if (*ff == '\0') {
                return true;  // Found substring match
            }
            ++n;
        }
        return false;
    }

    // Wildcard matching using recursion simulation with stack
    while (*f) {
        if (*f == '*') {
            ++f;
            if (*f == '\0') {
                return true;  // Trailing * matches everything
            }
            // Try matching remaining pattern at each position
            while (*n) {
                if (matchesFilter(n, f)) {
                    return true;
                }
                ++n;
            }
            return false;
        } else if (*f == '?') {
            if (*n == '\0') {
                return false;  // ? needs a character to match
            }
            ++f;
            ++n;
        } else {
            if (*n != *f) {
                return false;  // Literal mismatch
            }
            ++f;
            ++n;
        }
    }

    return *n == '\0';  // Both must be exhausted
}

// Skip test support - global state
// Thread-local would be ideal, but for embedded compatibility we use a global
static bool sCurrentTestSkipped = false;
static const char* sSkipReason = nullptr;

void TestContext::runTestCase(const TestCaseInfo& info) {
    mStats.mTestCasesRun++;
    mReporter->testCaseStart(info.mName);

    mCurrentTestFailed = false;
    mCurrentTestTimedOut = false;
    mCurrentTestName = info.mName;
    mSubcaseStack.clear();
    mNextSubcaseStack.clear();
    mFullyTraversedHashes.clear();
    mCurrentSubcaseDepth = 0;
    mSubcaseDiscoveryDepth = 0;
    mShouldReenter = true;

    // Reset skip flag for this test
    sCurrentTestSkipped = false;

    // Record test start time if we have a time function
    if (mGetMillis) {
        mCurrentTestStartMs = mGetMillis();
    }

    // Run the test multiple times to explore all subcase paths
    // On each run, we follow the path in mNextSubcaseStack and then discover one new subcase
    while (mShouldReenter && !mCurrentTestTimedOut && !sCurrentTestSkipped) {
        mShouldReenter = false;
        mCurrentSubcaseDepth = 0;
        mSubcaseDiscoveryDepth = 0;

        // Copy next stack to current stack for this iteration
        mSubcaseStack = mNextSubcaseStack;
        mNextSubcaseStack.clear();

        // Call the test function
        info.mFunc();

        // Check for skip (FL_SKIP was called)
        if (sCurrentTestSkipped) {
            break;
        }

        // Check for timeout after each iteration
        if (mDefaultTimeoutMs > 0 && checkTimeout()) {
            break;
        }
    }

    mCurrentTestName = nullptr;

    // Calculate test duration
    fl::u32 testDurationMs = 0;
    if (mGetMillis) {
        testDurationMs = mGetMillis() - mCurrentTestStartMs;
        mStats.mTotalDurationMs += testDurationMs;
    }

    if (sCurrentTestSkipped) {
        mStats.mTestCasesSkipped++;
        mReporter->testCaseEnd(true, testDurationMs);  // Skipped tests count as "not failed"
    } else if (mCurrentTestFailed || mCurrentTestTimedOut) {
        mStats.mTestCasesFailed++;
        mReporter->testCaseEnd(false, testDurationMs);
    } else {
        mStats.mTestCasesPassed++;
        mReporter->testCaseEnd(true, testDurationMs);
    }
}

bool TestContext::checkTimeout() {
    if (!mGetMillis || mDefaultTimeoutMs == 0) {
        return false;  // No timeout configured
    }

    fl::u32 now = mGetMillis();
    fl::u32 elapsed = now - mCurrentTestStartMs;

    if (elapsed > mDefaultTimeoutMs) {
        mCurrentTestTimedOut = true;

        // Call timeout handler if set
        if (mTimeoutHandler) {
            mTimeoutHandler(mCurrentTestName ? mCurrentTestName : "unknown", elapsed);
        } else {
            // Default behavior: print timeout message
            fl::printf("  [TIMEOUT] Test exceeded %u ms (elapsed: %u ms)\n",
                       mDefaultTimeoutMs, elapsed);
        }
        return true;
    }
    return false;
}

fl::u32 TestContext::getElapsedMs() const {
    if (!mGetMillis) {
        return 0;
    }
    return mGetMillis() - mCurrentTestStartMs;
}

bool TestContext::enterSubcase(const SubcaseSignature& sig) {
    // We're at subcase discovery depth mSubcaseDiscoveryDepth
    // mSubcaseStack contains the path we're following this iteration

    if (mSubcaseDiscoveryDepth < mSubcaseStack.size()) {
        // We're following a predetermined path
        if (mSubcaseStack[mSubcaseDiscoveryDepth] == sig) {
            // This is the subcase we should enter
            mSubcaseDiscoveryDepth++;
            mCurrentSubcaseDepth++;
            mReporter->subcaseStart(sig.mName);
            return true;
        }
        // Not the subcase we're looking for, skip it
        return false;
    } else {
        // We're past our predetermined path, discovering new subcases
        fl::u32 pathHash = hashCurrentPath(sig);

        if (isFullyTraversed(pathHash)) {
            // This subcase path has already been fully explored
            return false;
        }

        if (!mShouldReenter) {
            // First untraversed subcase at this level - enter it
            mSubcaseDiscoveryDepth++;
            mCurrentSubcaseDepth++;
            mReporter->subcaseStart(sig.mName);
            return true;
        } else {
            // We've already found a subcase to explore next time
            // Just record this one for future exploration
            if (mNextSubcaseStack.empty() || mNextSubcaseStack.size() == mSubcaseDiscoveryDepth) {
                // Build the path to this subcase for next iteration
                mNextSubcaseStack.clear();
                for (fl::size i = 0; i < mSubcaseDiscoveryDepth && i < mSubcaseStack.size(); ++i) {
                    mNextSubcaseStack.push_back(mSubcaseStack[i]);
                }
                mNextSubcaseStack.push_back(sig);
            }
            return false;
        }
    }
}

void TestContext::exitSubcase(const SubcaseSignature& sig) {
    mCurrentSubcaseDepth--;
    mSubcaseDiscoveryDepth--;

    // If we don't have a next path queued, we might need to reenter
    // to explore sibling subcases
    if (mNextSubcaseStack.empty() && mSubcaseDiscoveryDepth < mSubcaseStack.size()) {
        // Mark this path as traversed
        markFullyTraversed(hashCurrentPath(sig));
    } else if (mNextSubcaseStack.size() > 0) {
        // We have more paths to explore
        mShouldReenter = true;
    }

    mReporter->subcaseEnd();
}

void TestContext::reportAssert(const AssertResult& result) {
    if (result.mPassed) {
        mStats.mAssertsPassed++;
    } else {
        mStats.mAssertsFailed++;
        mCurrentTestFailed = true;
    }
    mReporter->assertResult(result);
}

void TestContext::checkFailed(const char* expr, const char* file, int line) {
    AssertResult result(false);
    result.mExpression = expr;
    result.mLocation = SourceLocation(file, line);
    reportAssert(result);
}

void TestContext::requireFailed(const char* expr, const char* file, int line) {
    checkFailed(expr, file, line);
    // Note: The return happens in the macro
}

fl::u32 TestContext::hashCurrentPath(const SubcaseSignature& sig) const {
    fl::u32 hash = 0;
    for (fl::size i = 0; i < mSubcaseDiscoveryDepth && i < mSubcaseStack.size(); ++i) {
        hash = hash * 31 + hashSubcaseSignature(mSubcaseStack[i]);
    }
    hash = hash * 31 + hashSubcaseSignature(sig);
    return hash;
}

bool TestContext::isFullyTraversed(fl::u32 hash) const {
    for (fl::size i = 0; i < mFullyTraversedHashes.size(); ++i) {
        if (mFullyTraversedHashes[i] == hash) {
            return true;
        }
    }
    return false;
}

void TestContext::markFullyTraversed(fl::u32 hash) {
    if (!isFullyTraversed(hash)) {
        mFullyTraversedHashes.push_back(hash);
    }
}

// =============================================================================
// Subcase implementation
// =============================================================================

Subcase::Subcase(const char* name, const char* file, int line)
    : mSignature{name, file, line}
    , mEntered(false) {
    mEntered = TestContext::instance().enterSubcase(mSignature);
}

Subcase::~Subcase() {
    if (mEntered) {
        TestContext::instance().exitSubcase(mSignature);
    }
}

// =============================================================================
// Message/Capture/Fail helpers
// =============================================================================

void outputMessage(const char* msg, const char* file, int line) {
    fl::printf("  [MESSAGE] %s:%d: %s\n", file, line, msg);
}

void outputCapture(const char* name, const char* value, const char* file, int line) {
    fl::printf("  [CAPTURE] %s:%d: %s := %s\n", file, line, name, value);
}

void fail(const char* msg, const char* file, int line, bool isFatal) {
    AssertResult result(false);
    result.mExpression = msg;
    result.mLocation = SourceLocation(file, line);
    TestContext::instance().reportAssert(result);
    if (isFatal) {
        fl::printf("    FAIL (fatal): %s:%d: %s\n", file, line, msg);
    } else {
        fl::printf("    FAIL_CHECK: %s:%d: %s\n", file, line, msg);
    }
}

// =============================================================================
// Skip test support functions
// =============================================================================

// Note: sCurrentTestSkipped and sSkipReason are defined above runTestCase()

void skipTest(const char* reason, const char* file, int line) {
    sCurrentTestSkipped = true;
    sSkipReason = reason;
    fl::printf("  [SKIPPED] %s:%d: %s\n", file, line, reason);
}

bool isTestSkipped() {
    return sCurrentTestSkipped;
}

// =============================================================================
// SerialReporter implementation
// =============================================================================

// Helper function to print using either custom print func or fl::printf
void SerialReporter::print(const char* msg) {
    if (mPrintFunc) {
        mPrintFunc(msg);
    } else {
        fl::printf("%s", msg);
    }
}

void SerialReporter::testRunStart() {
    print("\n");
    print("====== FL TEST: Running tests... ======\n");
}

void SerialReporter::testRunEnd(const TestStats& stats) {
    print("\n====== FL TEST: Results ======\n");

    // Format stats using snprintf-style approach
    fl::StrStream ss;
    ss << "Passed: " << stats.mTestCasesPassed
       << "/" << stats.mTestCasesRun << " tests\n";
    print(ss.str().c_str());

    if (stats.mTestCasesSkipped > 0) {
        fl::StrStream ss_skip;
        ss_skip << "Skipped: " << stats.mTestCasesSkipped << "\n";
        print(ss_skip.str().c_str());
    }

    if (stats.mAssertsFailed > 0) {
        fl::StrStream ss2;
        ss2 << "Failed assertions: " << stats.mAssertsFailed << "\n";
        print(ss2.str().c_str());
    }

    if (stats.allPassed()) {
        print("Status: PASS\n");
    } else {
        print("Status: FAIL\n");
    }
}

void SerialReporter::testCaseStart(const char* name) {
    fl::StrStream ss;
    ss << "\n[TEST] " << name << "\n";
    print(ss.str().c_str());
}

void SerialReporter::testCaseEnd(bool passed, fl::u32 durationMs) {
    if (passed) {
        if (durationMs > 0) {
            fl::StrStream ss;
            ss << "[PASS] (" << durationMs << " ms)\n";
            print(ss.str().c_str());
        } else {
            print("[PASS]\n");
        }
    } else {
        if (durationMs > 0) {
            fl::StrStream ss;
            ss << "[FAIL] (" << durationMs << " ms)\n";
            print(ss.str().c_str());
        } else {
            print("[FAIL]\n");
        }
    }
}

void SerialReporter::subcaseStart(const char* name) {
    fl::StrStream ss;
    ss << "  [SUBCASE] " << name << "\n";
    print(ss.str().c_str());
}

void SerialReporter::subcaseEnd() {
    // Nothing needed
}

void SerialReporter::assertResult(const AssertResult& result) {
    if (!result.mPassed) {
        fl::StrStream ss;
        ss << "  FAIL: " << result.mLocation.mFile
           << ":" << result.mLocation.mLine << "\n";
        ss << "  Expr: " << result.mExpression << "\n";
        if (!result.mExpanded.empty()) {
            ss << "  Got:  " << result.mExpanded << "\n";
        }
        print(ss.str().c_str());
    }
}

// =============================================================================
// XMLReporter implementation (JUnit format)
// =============================================================================

fl::string XMLReporter::escapeXml(const char* text) {
    if (!text) return fl::string();

    fl::string result;
    for (const char* p = text; *p; ++p) {
        switch (*p) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default:   result += *p; break;
        }
    }
    return result;
}

void XMLReporter::testRunStart() {
    mTestCaseResults.clear();
}

void XMLReporter::testRunEnd(const TestStats& stats) {
    if (!mOutput) return;

    fl::StrStream ss;

    // XML header
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";

    // Testsuite element with summary
    ss << "<testsuite name=\"" << escapeXml(mSuiteName) << "\" ";
    ss << "tests=\"" << stats.mTestCasesRun << "\" ";
    ss << "failures=\"" << stats.mTestCasesFailed << "\" ";
    ss << "errors=\"0\" ";
    ss << "skipped=\"" << stats.mTestCasesSkipped << "\">\n";

    // Add all test case results
    for (fl::size i = 0; i < mTestCaseResults.size(); ++i) {
        ss << mTestCaseResults[i];
    }

    ss << "</testsuite>\n";

    *mOutput = ss.str();
}

void XMLReporter::testCaseStart(const char* name) {
    mCurrentTestName = name ? name : "unknown";
    mCurrentTestFailures.clear();
    mCurrentTestPassed = true;
}

void XMLReporter::testCaseEnd(bool passed, fl::u32 durationMs) {
    fl::StrStream ss;
    ss << "  <testcase name=\"" << escapeXml(mCurrentTestName.c_str()) << "\"";

    // Add time attribute if duration is available (convert ms to seconds)
    if (durationMs > 0) {
        // Format as seconds with millisecond precision
        fl::u32 secs = durationMs / 1000;
        fl::u32 millis = durationMs % 1000;
        ss << " time=\"" << secs << "." << (millis < 100 ? (millis < 10 ? "00" : "0") : "") << millis << "\"";
    }

    if (passed) {
        ss << "/>\n";
    } else {
        ss << ">\n";
        ss << "    <failure message=\"Test failed\">\n";
        ss << "<![CDATA[" << mCurrentTestFailures << "]]>\n";
        ss << "    </failure>\n";
        ss << "  </testcase>\n";
    }

    mTestCaseResults.push_back(ss.str());
}

void XMLReporter::subcaseStart(const char* /*name*/) {
    // Subcases are noted in the failure message if they fail
}

void XMLReporter::subcaseEnd() {
    // Nothing to do
}

void XMLReporter::assertResult(const AssertResult& result) {
    if (!result.mPassed) {
        mCurrentTestPassed = false;

        fl::StrStream ss;
        ss << result.mLocation.mFile << ":" << result.mLocation.mLine << "\n";
        ss << "  Expression: " << result.mExpression << "\n";
        if (!result.mExpanded.empty()) {
            ss << "  Expanded: " << result.mExpanded << "\n";
        }
        ss << "\n";
        mCurrentTestFailures += ss.str();
    }
}

// =============================================================================
// JSONReporter implementation
// =============================================================================

fl::string JSONReporter::escapeJson(const char* text) {
    if (!text) return fl::string();

    fl::string result;
    for (const char* p = text; *p; ++p) {
        switch (*p) {
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                // Handle control characters
                if (static_cast<unsigned char>(*p) < 32) {
                    // Skip control characters or encode them
                    result += "\\u00";
                    char hex[3];
                    hex[0] = "0123456789abcdef"[(*p >> 4) & 0xF];
                    hex[1] = "0123456789abcdef"[*p & 0xF];
                    hex[2] = '\0';
                    result += hex;
                } else {
                    result += *p;
                }
                break;
        }
    }
    return result;
}

void JSONReporter::testRunStart() {
    mTestResults.clear();
}

void JSONReporter::testRunEnd(const TestStats& stats) {
    if (!mOutput) return;

    fl::StrStream ss;

    ss << "{\n";
    ss << "  \"summary\": {\n";
    ss << "    \"total\": " << stats.mTestCasesRun << ",\n";
    ss << "    \"passed\": " << stats.mTestCasesPassed << ",\n";
    ss << "    \"failed\": " << stats.mTestCasesFailed << ",\n";
    ss << "    \"skipped\": " << stats.mTestCasesSkipped << ",\n";
    ss << "    \"assertionsPassed\": " << stats.mAssertsPassed << ",\n";
    ss << "    \"assertionsFailed\": " << stats.mAssertsFailed << "\n";
    ss << "  },\n";
    ss << "  \"tests\": [\n";

    for (fl::size i = 0; i < mTestResults.size(); ++i) {
        ss << mTestResults[i];
        if (i < mTestResults.size() - 1) {
            ss << ",";
        }
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";

    *mOutput = ss.str();
}

void JSONReporter::testCaseStart(const char* name) {
    mCurrentTestName = name ? name : "unknown";
    mCurrentTestFailures.clear();
    mCurrentTestPassed = true;
}

void JSONReporter::testCaseEnd(bool passed, fl::u32 durationMs) {
    fl::StrStream ss;
    ss << "    {\n";
    ss << "      \"name\": \"" << escapeJson(mCurrentTestName.c_str()) << "\",\n";
    ss << "      \"passed\": " << (passed ? "true" : "false");

    // Add duration if available
    if (durationMs > 0) {
        ss << ",\n";
        ss << "      \"durationMs\": " << durationMs;
    }

    if (!passed && !mCurrentTestFailures.empty()) {
        ss << ",\n";
        ss << "      \"failures\": [\n";
        for (fl::size i = 0; i < mCurrentTestFailures.size(); ++i) {
            ss << "        " << mCurrentTestFailures[i];
            if (i < mCurrentTestFailures.size() - 1) {
                ss << ",";
            }
            ss << "\n";
        }
        ss << "      ]\n";
    } else {
        ss << "\n";
    }

    ss << "    }";

    mTestResults.push_back(ss.str());
}

void JSONReporter::subcaseStart(const char* /*name*/) {
    // Subcases are noted in failures
}

void JSONReporter::subcaseEnd() {
    // Nothing to do
}

void JSONReporter::assertResult(const AssertResult& result) {
    if (!result.mPassed) {
        mCurrentTestPassed = false;

        fl::StrStream ss;
        ss << "{\n";
        ss << "          \"file\": \"" << escapeJson(result.mLocation.mFile) << "\",\n";
        ss << "          \"line\": " << result.mLocation.mLine << ",\n";
        ss << "          \"expression\": \"" << escapeJson(result.mExpression.c_str()) << "\"";

        if (!result.mExpanded.empty()) {
            ss << ",\n";
            ss << "          \"expanded\": \"" << escapeJson(result.mExpanded.c_str()) << "\"";
        }
        ss << "\n        }";

        mCurrentTestFailures.push_back(ss.str());
    }
}

// =============================================================================
// TAPReporter implementation (Test Anything Protocol)
// =============================================================================

void TAPReporter::output(const char* line) {
    if (mPrintFunc) {
        mPrintFunc(line);
        mPrintFunc("\n");
    } else if (mOutput) {
        mStreamingOutput += line;
        mStreamingOutput += "\n";
    }
}

void TAPReporter::testRunStart() {
    mTestNumber = 0;
    mStreamingOutput.clear();

    // Output TAP version header
    output("TAP version 13");

    // If we know the total, output the plan at the start
    if (mTotalTests > 0) {
        fl::StrStream ss;
        ss << "1.." << mTotalTests;
        output(ss.str().c_str());
    }
}

void TAPReporter::testRunEnd(const TestStats& stats) {
    // If we didn't know the total, output the plan at the end
    if (mTotalTests == 0) {
        fl::StrStream ss;
        ss << "1.." << stats.mTestCasesRun;
        output(ss.str().c_str());
    }

    // Output summary as diagnostics
    fl::StrStream ss;
    ss << "# Tests: " << stats.mTestCasesRun
       << ", Passed: " << stats.mTestCasesPassed
       << ", Failed: " << stats.mTestCasesFailed;
    if (stats.mTestCasesSkipped > 0) {
        ss << ", Skipped: " << stats.mTestCasesSkipped;
    }
    output(ss.str().c_str());

    // Copy to output buffer if using buffer mode
    if (mOutput) {
        *mOutput = mStreamingOutput;
    }
}

void TAPReporter::testCaseStart(const char* name) {
    mTestNumber++;
    mCurrentTestName = name ? name : "unknown";
    mCurrentTestPassed = true;
    mDiagnostics.clear();
}

void TAPReporter::testCaseEnd(bool passed, fl::u32 durationMs) {
    fl::StrStream ss;

    if (passed) {
        ss << "ok " << mTestNumber << " - " << mCurrentTestName;
    } else {
        ss << "not ok " << mTestNumber << " - " << mCurrentTestName;
    }

    // TAP supports duration via directive (non-standard but widely recognized)
    if (durationMs > 0) {
        ss << " # (" << durationMs << " ms)";
    }
    output(ss.str().c_str());

    // Output diagnostics for failed tests
    for (fl::size i = 0; i < mDiagnostics.size(); ++i) {
        fl::StrStream diag;
        diag << "# " << mDiagnostics[i];
        output(diag.str().c_str());
    }
}

void TAPReporter::subcaseStart(const char* name) {
    // TAP doesn't have native subcase support, but we can add diagnostics
    fl::StrStream ss;
    ss << "  Subcase: " << (name ? name : "unknown");
    mDiagnostics.push_back(ss.str());
}

void TAPReporter::subcaseEnd() {
    // Nothing to do
}

void TAPReporter::assertResult(const AssertResult& result) {
    if (!result.mPassed) {
        mCurrentTestPassed = false;

        fl::StrStream ss;
        ss << "  Failed at " << result.mLocation.mFile << ":" << result.mLocation.mLine;
        mDiagnostics.push_back(ss.str());

        fl::StrStream ss2;
        ss2 << "    Expression: " << result.mExpression;
        mDiagnostics.push_back(ss2.str());

        if (!result.mExpanded.empty()) {
            fl::StrStream ss3;
            ss3 << "    Expanded: " << result.mExpanded;
            mDiagnostics.push_back(ss3.str());
        }
    }
}

} // namespace test
} // namespace fl

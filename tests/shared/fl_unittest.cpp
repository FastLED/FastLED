#include "fl_unittest.h"

namespace fl {
namespace test {

// ============================================================================
// SubcaseImpl Implementation
// ============================================================================

SubcaseImpl::SubcaseImpl(const char* name, const char* file, int line) : m_entered(false) {
    SubcaseCtx* ctx = _fl_get_subcase_ctx();
    if (!ctx) return;

    SubcaseCtx& subcase_ctx = *ctx;

    // Expand path if needed
    if (subcase_ctx.depth >= (int)subcase_ctx.path.size()) {
        subcase_ctx.path.resize(subcase_ctx.depth + 1);
    }

    SubcaseCtx::Node& node = subcase_ctx.path[subcase_ctx.depth];

    // Check if we should enter this subcase
    if (node.to_enter == node.seen) {
        // We want to execute this subcase iteration
        m_entered = true;
        subcase_ctx.needs_rerun = true;  // Signal that another pass is needed
    }

    node.seen++;
    subcase_ctx.depth++;
}

SubcaseImpl::~SubcaseImpl() {
    SubcaseCtx* ctx = _fl_get_subcase_ctx();
    if (!ctx) return;

    SubcaseCtx& subcase_ctx = *ctx;
    subcase_ctx.depth--;

    // If we exited deeper levels, we need to reset them for next pass
    if (subcase_ctx.depth < (int)subcase_ctx.path.size()) {
        for (int i = subcase_ctx.depth + 1; i < (int)subcase_ctx.path.size(); i++) {
            subcase_ctx.path[i].seen = 0;
        }
    }
}

// ============================================================================
// Assertion Recording
// ============================================================================

void record_assertion(
    const AssertionResult& result,
    const char* expr,
    const char* file,
    int line,
    bool is_require)
{
    TestState* test_state = _fl_get_test_state();
    if (!test_state) return;

    if (result.passed) {
        test_state->checks_passed++;
        return;
    }

    // Build failure message
    fl::sstream msg;
    msg << file << ":" << line << ": ";

    if (!result.lhs_str.empty()) {
        msg << result.lhs_str << " " << result.op_str << " " << result.rhs_str;
    } else {
        msg << expr;
    }

    fl::string failure_msg = msg.str();

    if (is_require) {
        test_state->requires_failed++;
        test_state->failures.push_back(failure_msg);
        // Note: Can't throw exceptions when compiled with -fno-exceptions
        // Mark failure and continue
    } else {
        test_state->checks_failed++;
        test_state->failures.push_back(failure_msg);
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

void fl_fail(const char* msg, const char* file, int line, bool is_require) {
    AssertionResult r{false, msg, "", "", ""};
    record_assertion(r, msg, file, line, is_require);
}

void fl_message(const char* msg, const char* file, int line) {
    fl::cout << file << ":" << line << ": MESSAGE: " << msg << fl::endl;
}

// ============================================================================
// Command-line Parsing
// ============================================================================

RunOptions parse_args(int argc, const char** argv) {
    RunOptions opts;

    for (int i = 1; i < argc; i++) {
        fl::string arg = argv[i];

        if (arg == "--success") {
            opts.show_success = true;
        } else if (arg == "--no-colors") {
            opts.no_colors = true;
        } else if (arg == "-r" || arg == "--reporters") {
            if (i + 1 < argc) {
                opts.reporter = argv[++i];
            }
        } else if (arg == "-tc" || arg == "--test-case") {
            if (i + 1 < argc) {
                opts.test_filter = argv[++i];
            }
        } else if (arg.find("--test-case=") == 0) {
            // Handle --test-case=value format
            opts.test_filter = arg.substr(12);
        }
    }

    return opts;
}

// ============================================================================
// Test Runner
// ============================================================================

int run_all(const RunOptions& opts) {
    const auto& entries = get_entries();

    int passed = 0;
    int failed = 0;
    int skipped = 0;

    for (const auto& entry : entries) {
        // Skip if test doesn't match filter
        if (!opts.test_filter.empty()) {
            if (fl::string(entry.name).find(opts.test_filter) == fl::string::npos) {
                continue;
            }
        }

        // Skip if decorator says to skip
        if (entry.skip) {
            skipped++;
            if (opts.show_success) {
                fl::cout << "SKIP: " << entry.name << " (" << entry.file << ":" << entry.line << ")"
                         << fl::endl;
            }
            continue;
        }

        // Run test with SUBCASE support
        bool test_passed = true;
        SubcaseCtx ctx;
        auto t_start = fl::chrono::steady_clock::now();

        do {
            ctx.depth = 0;
            ctx.needs_rerun = false;

            // Reset seen counters for this iteration
            for (auto& node : ctx.path) {
                node.seen = 0;
            }

            _fl_get_subcase_ctx() = &ctx;

            TestState test_state;
            _fl_get_test_state() = &test_state;

            // Run the test function
            entry.func();

            if (test_state.checks_failed > 0 || test_state.requires_failed > 0) {
                test_passed = false;
            }

            _fl_get_test_state() = nullptr;
            _fl_get_subcase_ctx() = nullptr;

            // Advance to next SUBCASE path if needed
            if (ctx.needs_rerun) {
                // Find which SUBCASE to run next
                bool found_next = false;
                for (int i = 0; i < (int)ctx.path.size(); i++) {
                    // Check if this level has more branches to explore
                    if (ctx.path[i].seen > ctx.path[i].to_enter) {
                        ctx.path[i].to_enter++;
                        // Reset all deeper levels
                        for (int j = i + 1; j < (int)ctx.path.size(); j++) {
                            ctx.path[j].to_enter = 0;
                        }
                        found_next = true;
                        break;
                    }
                }
                if (!found_next) {
                    ctx.needs_rerun = false;
                }
            }

        } while (ctx.needs_rerun);

        // Check timing
        auto ms = fl::chrono::duration_cast<fl::chrono::milliseconds>(
            fl::chrono::steady_clock::now() - t_start).count();
        if (ms > 5000) {
            fl::cout << "WARNING: test '" << entry.name << "' took "
                     << (ms/1000.0) << "s" << fl::endl;
        }

        if (test_passed) {
            passed++;
            if (opts.show_success) {
                fl::cout << "PASS: " << entry.name << fl::endl;
            }
        } else {
            failed++;
            fl::cout << "FAIL: " << entry.name << " at " << entry.file << ":" << entry.line << fl::endl;
            // Note: failure details would be printed during test execution to cerr or similar
        }
    }

    // Print summary (mimics doctest output format)
    fl::cout << "[doctest] test cases: " << (passed + failed + skipped)
             << " | " << passed << " passed"
             << " | " << failed << " failed";
    if (skipped > 0) {
        fl::cout << " | " << skipped << " skipped";
    }
    fl::cout << fl::endl;

    return failed > 0 ? 1 : 0;
}

} // namespace test
} // namespace fl

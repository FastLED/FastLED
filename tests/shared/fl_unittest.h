#pragma once

#include "fl/stl/string.h"
#include "fl/stl/vector.h"
#include "fl/stl/map.h"
#include "fl/stl/memory.h"
#include "fl/stl/sstream.h"
#include "fl/stl/chrono.h"
#include "fl/stl/ostream.h"
#include "fl/stl/function.h"
#include "fl/log.h"
#include "fl/stl/thread_local.h"
#include "fl/stl/cstring.h"
#include "fl/stl/type_traits.h"

/**
 * Custom test framework replacing doctest.
 *
 * Provides full control over test registration, execution, and reporting.
 * Key features:
 * - Native decorator support (skip, tags)
 * - SUBCASE with re-execution semantics
 * - Expression decomposition for clear assertion messages
 * - No external test framework dependencies
 */
namespace fl {
namespace test {

// ============================================================================
// Approx - floating-point comparison with tolerance
// ============================================================================

template<typename T>
struct TestApprox {
    T value;
    T eps;

    explicit TestApprox(T val) : value(val), eps(static_cast<T>(1e-5)) {}

    // For doctest compatibility
    TestApprox& epsilon(T e) {
        this->eps = e;
        return *this;
    }

    TestApprox& scale(T s) {
        eps *= fl::abs(s);
        return *this;
    }

    bool operator==(const T& other) const {
        return fl::abs(value - other) <= eps;
    }

    bool operator==(const TestApprox& other) const {
        return fl::abs(value - other.value) <= eps;
    }

    bool operator!=(const T& other) const {
        return !(*this == other);
    }

    bool operator!=(const TestApprox& other) const {
        return !(*this == other);
    }
};

// Reverse operators for TestApprox (e.g., double == TestApprox)
template<typename T>
inline bool operator==(const T& lhs, const TestApprox<T>& rhs) {
    return rhs == lhs;
}

template<typename T>
inline bool operator!=(const T& lhs, const TestApprox<T>& rhs) {
    return rhs != lhs;
}

// Templated reverse operators for different types (e.g., float == TestApprox<double>)
template<typename L, typename R>
inline bool operator==(const L& lhs, const TestApprox<R>& rhs) {
    return fl::abs(static_cast<R>(lhs) - rhs.value) <= rhs.eps;
}

template<typename L, typename R>
inline bool operator!=(const L& lhs, const TestApprox<R>& rhs) {
    return !(lhs == rhs);
}

// ============================================================================
// Test Entry Registration
// ============================================================================

struct TestEntry {
    const char* name;
    const char* file;
    int line;
    fl::function<void()> func;
    bool skip;
    const char* description;
};

// Global test registry
inline fl::vector<TestEntry>& get_entries() {
    static fl::vector<TestEntry> entries;
    return entries;
}

inline int register_test(
    const char* name, const char* file, int line,
    fl::function<void()> func = fl::function<void()>(), bool skip = false, const char* desc = "")
{
    get_entries().push_back({name, file, line, func, skip, desc});
    return 0;
}

// ============================================================================
// Decorator Types
// ============================================================================

struct SkipDecorator {
    bool value;
    SkipDecorator(bool v = true) : value(v) {}
};

struct TagsDecorator {
    const char* value;
    TagsDecorator(const char* t) : value(t) {}
};

inline SkipDecorator skip(bool v = true) { return SkipDecorator(v); }
inline TagsDecorator tags(const char* t) { return TagsDecorator(t); }

// ============================================================================
// Decorator Detection
// ============================================================================

inline bool _fl_extract_skip() {
    return false;
}

inline bool _fl_extract_skip(fl::test::SkipDecorator s, ...) {
    return s.value;
}

#define _FL_HAS_SKIP(...) fl::test::_fl_extract_skip(__VA_ARGS__)

// ============================================================================
// FL_TEST_CASE Macro
// ============================================================================

#define _FL_CONCAT(a, b) a##b
#define _FL_CONCAT_EXPANDED(a, b) _FL_CONCAT(a, b)

#define _FL_TC_IMPL(N, name, ...)                                              \
    static void _FL_CONCAT_EXPANDED(_fl_func_, N)();                           \
    namespace {                                                                \
        static int _FL_CONCAT_EXPANDED(_fl_reg_, N) =                          \
            ::fl::test::register_test(                                         \
                name, __FILE__, __LINE__,                                      \
                ::fl::function<void()>(&_FL_CONCAT_EXPANDED(_fl_func_, N)),    \
                _FL_HAS_SKIP(__VA_ARGS__));                                    \
    }                                                                          \
    static void _FL_CONCAT_EXPANDED(_fl_func_, N)()

#define FL_TEST_CASE(name, ...) _FL_TC_IMPL(__COUNTER__, name, ##__VA_ARGS__)

// ============================================================================
// SUBCASE Implementation
// ============================================================================

struct SubcaseCtx {
    struct Node {
        int seen = 0;
        int to_enter = 0;
    };

    fl::vector<Node> path;
    int depth = 0;
    bool needs_rerun = false;
};

// Thread-local globals using fl::ThreadLocal<T>
inline fl::ThreadLocal<SubcaseCtx*>& _fl_get_subcase_ctx() {
    static fl::ThreadLocal<SubcaseCtx*> ctx;
    return ctx;
}

class SubcaseImpl {
    bool m_entered;

public:
    SubcaseImpl(const char* name, const char* file, int line);
    ~SubcaseImpl();
    operator bool() const { return m_entered; }
};

#define FL_SUBCASE(name) if (fl::test::SubcaseImpl _sc = fl::test::SubcaseImpl(name, __FILE__, __LINE__))

// ============================================================================
// Assertion Infrastructure
// ============================================================================

struct AssertionResult {
    bool passed;
    fl::string expr_str;
    fl::string lhs_str;
    fl::string op_str;
    fl::string rhs_str;
};

// Test state tracking
struct TestState {
    int checks_passed = 0;
    int checks_failed = 0;
    int requires_failed = 0;
    fl::vector<fl::string> failures;
};

inline fl::ThreadLocal<TestState*>& _fl_get_test_state() {
    static fl::ThreadLocal<TestState*> state;
    return state;
}

// ============================================================================
// Expression Decomposition
// ============================================================================

struct ExprDecomposer {
    template<typename L>
    struct ExprLhs {
        const L& lhs;

        // Simple boolean check
        operator AssertionResult() const {
            bool passed = (bool)lhs;
            return {passed, "", "", "", ""};
        }
    };

    template<typename L>
    ExprLhs<L> operator<<(const L& val) { return {val}; }
};

// Forward declaration
void record_assertion(
    const AssertionResult& result,
    const char* expr,
    const char* file,
    int line,
    bool is_require
);

// ============================================================================
// Assertion Macros
// ============================================================================

#define FL_CHECK(expr) do { \
    fl::test::AssertionResult _res = (fl::test::ExprDecomposer{} << (expr)); \
    if (!_res.passed) { \
        fl::test::record_assertion(_res, #expr, __FILE__, __LINE__, false); \
    } \
} while(0)

#define FL_CHECK_FALSE(expr) FL_CHECK(!(expr))
#define FL_CHECK_TRUE(expr) FL_CHECK(expr)

// Comparison helpers that handle mixed signed/unsigned without warnings.
// Uses partial specialization + tag dispatch for safe cross-sign comparison.
namespace detail {

template<typename A, typename B,
         bool MixedSign = (fl::is_integral<A>::value &&
                           fl::is_integral<B>::value &&
                           fl::is_signed<A>::value != fl::is_signed<B>::value)>
struct SafeCompare {
    static bool eq(const A& a, const B& b) { return a == b; }
    static bool ne(const A& a, const B& b) { return a != b; }
};

template<typename A, typename B>
struct SafeCompare<A, B, true> {
    static bool eq(const A& a, const B& b) {
        return eq_dispatch(a, b, fl::integral_constant<bool, fl::is_signed<A>::value>{});
    }
    static bool ne(const A& a, const B& b) { return !eq(a, b); }
private:
    // A signed, B unsigned
    static bool eq_dispatch(const A& a, const B& b, fl::true_type) {
        return a >= 0 && static_cast<typename fl::make_unsigned<A>::type>(a) == b;
    }
    // A unsigned, B signed
    static bool eq_dispatch(const A& a, const B& b, fl::false_type) {
        return b >= 0 && a == static_cast<typename fl::make_unsigned<B>::type>(b);
    }
};

} // namespace detail

// Note: already inside namespace fl::test
template<typename A, typename B>
inline bool check_eq(const A& a, const B& b) { return detail::SafeCompare<A, B>::eq(a, b); }
inline bool check_eq(const char* a, const char* b) { return ::fl::strcmp(a, b) == 0; }
template<typename A, typename B>
inline bool check_ne(const A& a, const B& b) { return detail::SafeCompare<A, B>::ne(a, b); }
inline bool check_ne(const char* a, const char* b) { return ::fl::strcmp(a, b) != 0; }

#define FL_CHECK_EQ(a, b) FL_CHECK(fl::test::check_eq((a), (b)))
#define FL_CHECK_NE(a, b) FL_CHECK(fl::test::check_ne((a), (b)))
#define FL_CHECK_LT(a, b) FL_CHECK((bool)((a) < (b)))
#define FL_CHECK_LE(a, b) FL_CHECK((bool)((a) <= (b)))
#define FL_CHECK_GT(a, b) FL_CHECK((bool)((a) > (b)))
#define FL_CHECK_GE(a, b) FL_CHECK((bool)((a) >= (b)))

#define FL_REQUIRE(expr) do { \
    fl::test::AssertionResult _res = (fl::test::ExprDecomposer{} << (expr)); \
    if (!_res.passed) { \
        fl::test::record_assertion(_res, #expr, __FILE__, __LINE__, true); \
    } \
} while(0)

#define FL_REQUIRE_FALSE(expr) FL_REQUIRE(!(expr))
#define FL_REQUIRE_TRUE(expr) FL_REQUIRE(expr)

#define FL_REQUIRE_EQ(a, b) FL_REQUIRE((a) == (b))
#define FL_REQUIRE_NE(a, b) FL_REQUIRE((a) != (b))
#define FL_REQUIRE_LT(a, b) FL_REQUIRE((a) < (b))
#define FL_REQUIRE_LE(a, b) FL_REQUIRE((a) <= (b))
#define FL_REQUIRE_GT(a, b) FL_REQUIRE((a) > (b))
#define FL_REQUIRE_GE(a, b) FL_REQUIRE((a) >= (b))

// Other macros (compatibility with doctest)
#define FL_CHECK_UNARY(expr) FL_CHECK(expr)
#define FL_CHECK_UNARY_FALSE(expr) FL_CHECK_FALSE(expr)
#define FL_REQUIRE_UNARY(expr) FL_REQUIRE(expr)
#define FL_REQUIRE_UNARY_FALSE(expr) FL_REQUIRE_FALSE(expr)

// Exception-related (no-op without try/catch due to -fno-exceptions)
#define FL_CHECK_THROWS(expr) FL_CHECK(false)
#define FL_CHECK_THROWS_AS(expr, ...) FL_CHECK(false)
#define FL_CHECK_THROWS_WITH(expr, ...) FL_CHECK(false)
#define FL_CHECK_NOTHROW(expr) FL_CHECK(true)

#define FL_REQUIRE_THROWS(expr) FL_REQUIRE(false)
#define FL_REQUIRE_THROWS_AS(expr, ...) FL_REQUIRE(false)
#define FL_REQUIRE_THROWS_WITH(expr, ...) FL_REQUIRE(false)
#define FL_REQUIRE_NOTHROW(expr) FL_REQUIRE(true)

// Warning family (non-failing)
#define FL_DWARN(expr) do { (void)(expr); } while(0)
#define FL_DWARN_FALSE(expr) do { (void)(expr); } while(0)
#define FL_WARN_EQ(a, b) do { (void)((a) == (b)); } while(0)
#define FL_WARN_NE(a, b) do { (void)((a) != (b)); } while(0)
#define FL_WARN_GT(a, b) do { (void)((a) > (b)); } while(0)
#define FL_WARN_GE(a, b) do { (void)((a) >= (b)); } while(0)
#define FL_WARN_LT(a, b) do { (void)((a) < (b)); } while(0)
#define FL_WARN_LE(a, b) do { (void)((a) <= (b)); } while(0)

// Explicit failure and messages
#define FL_FAIL(...) \
    do { \
        fl::sstream _msg; \
        _msg << __VA_ARGS__; \
        fl::string _msg_str = _msg.str(); \
        fl::test::record_assertion({false, _msg_str, "", "", ""}, _msg_str.c_str(), __FILE__, __LINE__, true); \
    } while(0)

#define FL_FAIL_CHECK(...) \
    do { \
        fl::sstream _msg; \
        _msg << __VA_ARGS__; \
        fl::string _msg_str = _msg.str(); \
        fl::test::record_assertion({false, _msg_str, "", "", ""}, _msg_str.c_str(), __FILE__, __LINE__, false); \
    } while(0)
#define FL_MESSAGE(...) \
    do { \
        fl::sstream _msg; \
        _msg << __VA_ARGS__; \
        fl::cout << __FILE__ << ":" << __LINE__ << ": MESSAGE: " << _msg.str() << fl::endl; \
    } while(0)
#define FL_DINFO(...) FL_MESSAGE(__VA_ARGS__)
#define FL_CAPTURE(x) FL_MESSAGE(#x " = " << fl::to_string(x))

#define FL_CHECK_MESSAGE(condition, msg) \
    do { \
        if (!(condition)) { \
            fl::sstream _msg; \
            _msg << msg; \
            fl::test::record_assertion({false, _msg.str(), "", "", ""}, #condition, __FILE__, __LINE__, false); \
        } else { \
            fl::test::record_assertion({true, "", "", "", ""}, #condition, __FILE__, __LINE__, false); \
        } \
    } while(0)

#define FL_CHECK_FALSE_MESSAGE(condition, msg) FL_CHECK_MESSAGE(!(condition), msg)

#define FL_REQUIRE_MESSAGE(condition, msg) \
    do { \
        if (!(condition)) { \
            fl::sstream _msg; \
            _msg << msg; \
            fl::test::record_assertion({false, _msg.str(), "", "", ""}, #condition, __FILE__, __LINE__, true); \
        } else { \
            fl::test::record_assertion({true, "", "", "", ""}, #condition, __FILE__, __LINE__, true); \
        } \
    } while(0)

// BDD-style macros
#define FL_SCENARIO(name) FL_SUBCASE("SCENARIO: " name)
#define FL_GIVEN(name) FL_SUBCASE("GIVEN: " name)
#define FL_WHEN(name) FL_SUBCASE("WHEN: " name)
#define FL_AND_WHEN(name) FL_SUBCASE("AND_WHEN: " name)
#define FL_THEN(name) FL_SUBCASE("THEN: " name)
#define FL_AND_THEN(name) FL_SUBCASE("AND_THEN: " name)

// Fixtures and templates
#define FL_TEST_CASE_FIXTURE(Fixture, name) \
    struct _FL_CONCAT_EXPANDED(_fixture_, __LINE__) : public Fixture { \
        void run_test(); \
    }; \
    FL_TEST_CASE(name) { \
        _FL_CONCAT_EXPANDED(_fixture_, __LINE__) _f; \
        _f.run_test(); \
    } \
    void _FL_CONCAT_EXPANDED(_fixture_, __LINE__)::run_test()

#define FL_TEST_CASE_TEMPLATE(name, T, ...) FL_TEST_CASE(name)
// Doctest TEST_SUITE compatibility - creates an anonymous namespace for test organization
#define FL_TEST_SUITE(name) namespace

// ============================================================================
// Test Runner Configuration
// ============================================================================

struct RunOptions {
    fl::string test_filter;
    bool show_success = false;
    bool no_colors = false;
    const char* reporter = "console";
};

// Forward declarations - implementations in fl_unittest.cpp
RunOptions parse_args(int argc, const char** argv);
int run_all(const RunOptions& opts);
void fl_fail(const char* msg, const char* file, int line, bool is_require);
void fl_message(const char* msg, const char* file, int line);

} // namespace test
} // namespace fl

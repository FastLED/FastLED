/// @file trace.cpp
/// @brief Test for internal call stack tracking system

#include "fl/trace.h"
#include "doctest.h"
#include "fl/log.h"
#include "fl/stl/strstream.h"
#include "fl/int.h"

// Stack tracing is controlled by FASTLED_DEBUG_STACK_TRACE at build time.
// When enabled, full tests run. When disabled, only macro no-op tests run.

using namespace fl;

TEST_CASE("StackTrace - basic push and pop") {
    // Clear any previous state
    ScopedTrace::clear();

    // Initial state: empty stack
    CHECK(ScopedTrace::depth() == 0);

    // Push one entry
    ScopedTrace::push("function_a", 100);
    CHECK(ScopedTrace::depth() == 1);

    // Push another entry
    ScopedTrace::push("function_b", 200);
    CHECK(ScopedTrace::depth() == 2);

    // Pop entries in reverse order
    ScopedTrace::pop();
    CHECK(ScopedTrace::depth() == 1);

    ScopedTrace::pop();
    CHECK(ScopedTrace::depth() == 0);
}

TEST_CASE("StackTrace - RAII scoped trace") {
    ScopedTrace::clear();
    CHECK(ScopedTrace::depth() == 0);

    {
        ScopedTrace trace1("outer_function", 101);
        CHECK(ScopedTrace::depth() == 1);

        {
            ScopedTrace trace2("inner_function", 102);
            CHECK(ScopedTrace::depth() == 2);
        }  // trace2 destructor auto-pops

        CHECK(ScopedTrace::depth() == 1);
    }  // trace1 destructor auto-pops

    CHECK(ScopedTrace::depth() == 0);
}

TEST_CASE("StackTrace - overflow handling") {
    ScopedTrace::clear();

    // Push exactly at capacity (32 entries)
    for (fl::size i = 0; i < 32; ++i) {
        ScopedTrace::push("function", (int)i);
    }
    CHECK(ScopedTrace::depth() == 32);

    // Push beyond capacity - depth should still increment
    ScopedTrace::push("overflow_1", 999);
    CHECK(ScopedTrace::depth() == 33);

    ScopedTrace::push("overflow_2", 1000);
    CHECK(ScopedTrace::depth() == 34);

    // Pop should bring us back
    ScopedTrace::pop();
    CHECK(ScopedTrace::depth() == 33);

    ScopedTrace::pop();
    CHECK(ScopedTrace::depth() == 32);

    // Continue popping until empty
    for (fl::size i = 0; i < 32; ++i) {
        ScopedTrace::pop();
    }
    CHECK(ScopedTrace::depth() == 0);
}

TEST_CASE("StackTrace - underflow protection") {
    ScopedTrace::clear();
    CHECK(ScopedTrace::depth() == 0);

    // Pop on empty stack should be no-op
    ScopedTrace::pop();
    CHECK(ScopedTrace::depth() == 0);

    ScopedTrace::pop();
    CHECK(ScopedTrace::depth() == 0);

    // Push then verify proper pop
    ScopedTrace::push("test", 103);
    CHECK(ScopedTrace::depth() == 1);

    ScopedTrace::pop();
    CHECK(ScopedTrace::depth() == 0);
}

TEST_CASE("StackTrace - null function handling") {
    ScopedTrace::clear();

    // Pushing null should be ignored
    ScopedTrace::push(nullptr, 104);
    CHECK(ScopedTrace::depth() == 0);

    // ScopedTrace with null should not affect stack
    {
        ScopedTrace trace(nullptr, 105);
        CHECK(ScopedTrace::depth() == 0);
    }
    CHECK(ScopedTrace::depth() == 0);
}

TEST_CASE("StackTrace - macro FL_SCOPED_TRACE_NAMED") {
    ScopedTrace::clear();

    {
        FL_SCOPED_TRACE_NAMED("macro_test");
        CHECK(ScopedTrace::depth() == 1);

        {
            FL_SCOPED_TRACE_NAMED("nested_macro");
            CHECK(ScopedTrace::depth() == 2);
        }

        CHECK(ScopedTrace::depth() == 1);
    }

    CHECK(ScopedTrace::depth() == 0);
}

TEST_CASE("StackTrace - dump output") {
    ScopedTrace::clear();

    // This test just verifies dump doesn't crash
    // Visual inspection of output needed to verify formatting
    FL_SCOPED_TRACE_NAMED("outer");
    FL_SCOPED_TRACE_NAMED("middle");
    FL_SCOPED_TRACE_NAMED("inner");

    CHECK(ScopedTrace::depth() == 3);
    FL_TRACE_DUMP();  // Should print stack trace via FL_DBG
}

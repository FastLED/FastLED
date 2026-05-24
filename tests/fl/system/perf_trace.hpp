/// @file perf_trace.hpp
/// @brief Unit tests for the thread-local performance trace system.
///
/// These tests force-compile the perf_trace module by defining
/// FASTLED_PERF_TRACE locally. The shipped header otherwise no-ops
/// the macros so tests would not exercise the real code paths.

#ifndef FASTLED_PERF_TRACE
#define FASTLED_PERF_TRACE 1
#endif

#include "test.h"

#include "fl/system/perf_trace.h"
#include "fl/stl/chrono.h"
#include "fl/stl/int.h"

using namespace fl;

namespace {

struct PerfTraceFixture {
    PerfTraceFixture() {
        PerfTrace::clear();
        PerfTrace::set_enabled(true);
    }
    ~PerfTraceFixture() {
        PerfTrace::set_enabled(false);
        PerfTrace::clear();
    }
};

}  // namespace

FL_TEST_CASE("PerfTrace - disabled by default after fresh process state") {
    // The fixture below activates tracing; verify the explicit disable path.
    PerfTrace::set_enabled(false);
    PerfTrace::clear();
    FL_CHECK_FALSE(PerfTrace::is_enabled());

    const int slot = PerfTrace::begin(__FILE__, __LINE__, "noop");
    FL_CHECK(slot == -1);
    PerfTrace::end(slot);
    FL_CHECK(PerfTrace::event_count() == 0u);
}

FL_TEST_CASE("PerfTrace - basic begin/end records one event") {
    PerfTraceFixture _;

    const int slot = PerfTrace::begin(__FILE__, __LINE__, "basic");
    FL_CHECK(slot >= 0);
    FL_CHECK(PerfTrace::stack_depth() == 1u);
    PerfTrace::end(slot);
    FL_CHECK(PerfTrace::stack_depth() == 0u);
    FL_CHECK(PerfTrace::event_count() == 1u);

    const PerfEvent* events = PerfTrace::events();
    FL_CHECK(events != nullptr);
    FL_CHECK(events[0].depth == 0);
    FL_CHECK(events[0].end_us >= events[0].start_us);
    FL_CHECK(events[0].name != nullptr);
}

FL_TEST_CASE("PerfTrace - FL_PERF_SCOPE RAII guard records on exit") {
    PerfTraceFixture _;
    {
        FL_PERF_SCOPE("outer");
        FL_CHECK(PerfTrace::stack_depth() == 1u);
        {
            FL_PERF_SCOPE("inner");
            FL_CHECK(PerfTrace::stack_depth() == 2u);
        }
        FL_CHECK(PerfTrace::stack_depth() == 1u);
        FL_CHECK(PerfTrace::event_count() == 1u);  // inner closed
    }
    FL_CHECK(PerfTrace::stack_depth() == 0u);
    FL_CHECK(PerfTrace::event_count() == 2u);

    // Inner event is emitted before outer because it closes first.
    const PerfEvent* events = PerfTrace::events();
    FL_CHECK(events[0].depth == 1);  // inner
    FL_CHECK(events[1].depth == 0);  // outer
}

FL_TEST_CASE("PerfTrace - runtime disable suppresses events even when compiled in") {
    PerfTraceFixture _;
    PerfTrace::set_enabled(false);

    {
        FL_PERF_SCOPE("ignored");
        FL_CHECK(PerfTrace::stack_depth() == 0u);
    }
    FL_CHECK(PerfTrace::event_count() == 0u);

    PerfTrace::set_enabled(true);
    {
        FL_PERF_SCOPE("captured");
    }
    FL_CHECK(PerfTrace::event_count() == 1u);
}

FL_TEST_CASE("PerfTrace - duration monotonic non-negative") {
    PerfTraceFixture _;

    const int slot = PerfTrace::begin(__FILE__, __LINE__, "dur");
    // Busy work to make the duration measurable on fast hosts.
    fl::u32 spin = 0;
    for (int i = 0; i < 1000; ++i) {
        spin += static_cast<fl::u32>(i);
    }
    (void)spin;
    PerfTrace::end(slot);

    FL_CHECK(PerfTrace::event_count() == 1u);
    const PerfEvent& ev = PerfTrace::events()[0];
    FL_CHECK(ev.end_us >= ev.start_us);
}

FL_TEST_CASE("PerfTrace - file/line/name captured from macro site") {
    PerfTraceFixture _;

    int expected_line = 0;
    {
        expected_line = __LINE__ + 1;
        FL_PERF_SCOPE("located");
    }

    FL_CHECK(PerfTrace::event_count() == 1u);
    const PerfEvent& ev = PerfTrace::events()[0];
    FL_CHECK(ev.name != nullptr);
    FL_CHECK(ev.file != nullptr);
    FL_CHECK(ev.line == expected_line);
}

FL_TEST_CASE("PerfTrace - stack overflow marks dropped() without corruption") {
    PerfTraceFixture _;

    // Open one more than the configured stack capacity.
    int slots[FL_PERF_TRACE_MAX_STACK + 2];
    for (int i = 0; i < FL_PERF_TRACE_MAX_STACK; ++i) {
        slots[i] = PerfTrace::begin(__FILE__, __LINE__, "stk");
        FL_CHECK(slots[i] >= 0);
    }
    slots[FL_PERF_TRACE_MAX_STACK]     = PerfTrace::begin(__FILE__, __LINE__, "ovf1");
    slots[FL_PERF_TRACE_MAX_STACK + 1] = PerfTrace::begin(__FILE__, __LINE__, "ovf2");
    FL_CHECK(slots[FL_PERF_TRACE_MAX_STACK] == -1);
    FL_CHECK(slots[FL_PERF_TRACE_MAX_STACK + 1] == -1);
    FL_CHECK(PerfTrace::dropped());

    // Close in reverse order, including the dropped slots (no-ops).
    PerfTrace::end(slots[FL_PERF_TRACE_MAX_STACK + 1]);
    PerfTrace::end(slots[FL_PERF_TRACE_MAX_STACK]);
    for (int i = FL_PERF_TRACE_MAX_STACK - 1; i >= 0; --i) {
        PerfTrace::end(slots[i]);
    }
    FL_CHECK(PerfTrace::stack_depth() == 0u);
    FL_CHECK(PerfTrace::event_count() == static_cast<fl::size>(FL_PERF_TRACE_MAX_STACK));
}

FL_TEST_CASE("PerfTrace - flush emits and clears the log") {
    PerfTraceFixture _;

    for (int i = 0; i < 3; ++i) {
        FL_PERF_SCOPE("flushable");
    }
    FL_CHECK(PerfTrace::event_count() == 3u);

    struct Counter { int seen = 0; };
    Counter c;
    PerfTrace::flush(
        [](const PerfEvent& ev, void* user) {
            (void)ev;
            static_cast<Counter*>(user)->seen++;
        },
        &c);
    FL_CHECK(c.seen == 3);
    FL_CHECK(PerfTrace::event_count() == 0u);
}

FL_TEST_CASE("PerfTrace - log capacity caps event_count() and sets dropped()") {
    PerfTraceFixture _;

    // Fire one more than the log capacity (flat, no nesting).
    for (int i = 0; i < FL_PERF_TRACE_MAX_EVENTS + 1; ++i) {
        const int slot = PerfTrace::begin(__FILE__, __LINE__, "cap");
        PerfTrace::end(slot);
    }
    FL_CHECK(PerfTrace::event_count() == static_cast<fl::size>(FL_PERF_TRACE_MAX_EVENTS));
    FL_CHECK(PerfTrace::dropped());
}

FL_TEST_CASE("PerfTrace - clear() resets log, stack, and dropped flag") {
    PerfTraceFixture _;

    for (int i = 0; i < FL_PERF_TRACE_MAX_EVENTS + 1; ++i) {
        const int slot = PerfTrace::begin(__FILE__, __LINE__, "reset");
        PerfTrace::end(slot);
    }
    FL_CHECK(PerfTrace::dropped());

    PerfTrace::clear();
    FL_CHECK(PerfTrace::event_count() == 0u);
    FL_CHECK(PerfTrace::stack_depth() == 0u);
    FL_CHECK_FALSE(PerfTrace::dropped());
}

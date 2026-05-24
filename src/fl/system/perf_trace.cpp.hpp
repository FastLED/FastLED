/**
 * @file perf_trace.cpp
 * @brief Implementation of thread-local push/pop performance tracing.
 */

#include "fl/system/perf_trace.h"

#include "fl/stl/chrono.h"     // fl::micros
#include "fl/stl/noexcept.h"
#include "fl/stl/singleton.h"  // SingletonThreadLocal
#include "fl/stl/stdio.h"      // fl::sprintf
#include "fl/stl/string.h"
#include "fl/stl/vector.h"     // FixedVector
#include "fl/log/log.h"        // FL_INFO

namespace fl {

namespace detail {

/// @brief Single-thread perf-trace storage. Held in SingletonThreadLocal.
struct PerfTraceStorage {
    /// Stack of in-flight scopes (slot index = stack index when opened).
    FixedVector<PerfEvent, FL_PERF_TRACE_MAX_STACK> in_flight;
    /// Completed-event log; flushed at frame boundaries.
    FixedVector<PerfEvent, FL_PERF_TRACE_MAX_EVENTS> log;
    /// True once an event was dropped (log full / stack overflow).
    bool dropped = false;
};

static PerfTraceStorage& storage() FL_NOEXCEPT {
    return SingletonThreadLocal<PerfTraceStorage>::instance();
}

/// @brief Process-wide runtime master switch. False by default — the
/// caller (autoresearch RPC, sketch, test) must turn it on explicitly.
///
/// Plain bool is fine here: workers only ever read it; flipping the
/// switch from another thread races with capture by at most one event,
/// which is acceptable for performance tracing.
static bool g_enabled = false;

} // namespace detail

void PerfTrace::set_enabled(bool on) FL_NOEXCEPT {
    detail::g_enabled = on;
}

bool PerfTrace::is_enabled() FL_NOEXCEPT {
    return detail::g_enabled;
}

int PerfTrace::begin(const char* file, int line, const char* name) FL_NOEXCEPT {
    if (!detail::g_enabled) {
        return -1;
    }
    auto& s = detail::storage();
    if (s.in_flight.size() >= FL_PERF_TRACE_MAX_STACK) {
        s.dropped = true;
        return -1;
    }
    PerfEvent ev;
    ev.file     = file;
    ev.name     = name;
    ev.line     = line;
    ev.depth    = static_cast<int>(s.in_flight.size());
    ev.start_us = fl::micros();
    ev.end_us   = 0;
    const int slot = static_cast<int>(s.in_flight.size());
    s.in_flight.push_back(ev);
    return slot;
}

void PerfTrace::end(int slot) FL_NOEXCEPT {
    if (slot < 0) {
        return;  // begin() was suppressed (disabled or overflowed)
    }
    auto& s = detail::storage();
    const int top = static_cast<int>(s.in_flight.size()) - 1;
    if (top < 0) {
        return;  // underflow — nothing to close
    }
    if (slot != top) {
        // Mis-nested end() — drop the top (best effort) without corrupting
        // the log. Caller likely has unbalanced begin/end pairs.
        s.dropped = true;
        s.in_flight.resize(s.in_flight.size() - 1);
        return;
    }

    PerfEvent ev = s.in_flight[top];
    ev.end_us = fl::micros();
    s.in_flight.resize(s.in_flight.size() - 1);

    if (s.log.size() >= FL_PERF_TRACE_MAX_EVENTS) {
        s.dropped = true;
        return;
    }
    s.log.push_back(ev);
}

fl::size PerfTrace::event_count() FL_NOEXCEPT {
    return detail::storage().log.size();
}

const PerfEvent* PerfTrace::events() FL_NOEXCEPT {
    auto& log = detail::storage().log;
    return log.empty() ? nullptr : &log[0];
}

fl::size PerfTrace::stack_depth() FL_NOEXCEPT {
    return detail::storage().in_flight.size();
}

bool PerfTrace::dropped() FL_NOEXCEPT {
    return detail::storage().dropped;
}

void PerfTrace::clear() FL_NOEXCEPT {
    auto& s = detail::storage();
    s.log.resize(0);
    s.in_flight.resize(0);
    s.dropped = false;
}

void PerfTrace::flush(PerfEmitFn cb, void* user) FL_NOEXCEPT {
    auto& s = detail::storage();
    const fl::size n = s.log.size();
    for (fl::size i = 0; i < n; ++i) {
        const PerfEvent& ev = s.log[i];
        if (cb) {
            cb(ev, user);
            continue;
        }
        const fl::u32 dur_us = ev.end_us - ev.start_us;
        // Compact single-line dump — easy to grep, easy to parse.
        // Format: PT d=<depth> dur_us=<dur> start_us=<start> name=<name> @<file>:<line>
        char buf[256];
        fl::sprintf(buf,
            "PT d=%d dur_us=%lu start_us=%lu name=%s @%s:%d",
            ev.depth,
            static_cast<unsigned long>(dur_us),
            static_cast<unsigned long>(ev.start_us),
            ev.name ? ev.name : "<null>",
            ev.file ? ev.file : "<null>",
            ev.line);
        FL_INFO(buf);
    }
    if (s.dropped) {
        FL_INFO("PT WARN dropped events since last flush");
    }
    s.log.resize(0);
    s.dropped = false;
}

} // namespace fl

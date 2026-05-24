#pragma once

/**
## Performance Trace System

Thread-local push/pop performance tracing for profiling hot paths in
FastLED (e.g. CFastLED::show and the PARLIO driver's DMA wait loop).

Each scope captures (file, line, name, depth, start_us, end_us) and is
appended to a per-thread event log when the scope closes. The log can be
flushed at frame boundaries to produce a chronological dump in the
classic begin/end performance-tracing idiom:

```
PT depth=0 dur_us=16320 start_us=12345678 file=src/FastLED.cpp.hpp line=233 name=show
PT depth=1 dur_us=11800 start_us=12345990 file=src/fl/channels/manager.cpp.hpp line=287 name=flush
PT depth=2 dur_us=11700 start_us=12345992 file=src/fl/channels/manager.cpp.hpp line=358 name=waitForReady
```

### Compile gate
`FASTLED_PERF_TRACE=1` — when undefined, all `FL_PERF_*` macros expand
to no-ops and the entire module is dead code.

### Runtime gate
Even when compiled in, `fl::PerfTrace::set_enabled(true)` must be called
to capture events. Autoresearch can toggle this over JSON-RPC.

### Usage
```cpp
#include "fl/system/perf_trace.h"

void CFastLED::show(u8 scale) {
    FL_PERF_SCOPE("show");
    // ...
}

// At a natural boundary (end of frame):
FL_PERF_FRAME_FLUSH();
```

### Configuration
- `FL_PERF_TRACE_MAX_EVENTS` (default 128) — completed-event log capacity
- `FL_PERF_TRACE_MAX_STACK`  (default 32)  — in-flight scope depth
 */

#include "fl/stl/stdint.h"
#include "fl/stl/noexcept.h"

#ifndef FL_PERF_TRACE_MAX_EVENTS
#define FL_PERF_TRACE_MAX_EVENTS 128
#endif

#ifndef FL_PERF_TRACE_MAX_STACK
#define FL_PERF_TRACE_MAX_STACK 32
#endif

namespace fl {

/// @brief A single completed performance-trace event.
/// All string pointers are non-owning — they must reference string
/// literals (`__FILE__` and the user-provided name argument).
struct PerfEvent {
    const char* file;   ///< __FILE__ (string literal)
    const char* name;   ///< user-supplied name (string literal)
    int line;           ///< __LINE__
    int depth;          ///< nesting depth at scope open (0 = root)
    fl::u32 start_us;   ///< fl::micros() at scope open
    fl::u32 end_us;     ///< fl::micros() at scope close
};

/// @brief Optional emit callback signature for PerfTrace::flush().
using PerfEmitFn = void(*)(const PerfEvent& event, void* user);

/// @brief Per-thread performance trace machinery.
///
/// All public methods are no-ops when `FASTLED_PERF_TRACE` is not
/// defined at compile time, or when `set_enabled(true)` has not been
/// called at runtime. Inline implementations live in `perf_trace.cpp.hpp`.
class PerfTrace {
public:
    /// @brief Runtime master switch. Initially disabled.
    static void set_enabled(bool on) FL_NOEXCEPT;

    /// @brief Query whether tracing is currently active.
    static bool is_enabled() FL_NOEXCEPT;

    /// @brief Open a new scope. Captures `fl::micros()` and pushes onto
    ///        the thread-local in-flight stack. Returns a slot id which
    ///        must be passed to `end()`. Returns -1 if dropped (disabled
    ///        or stack overflow).
    static int begin(const char* file, int line, const char* name) FL_NOEXCEPT;

    /// @brief Close the most recent matching scope. The slot id from
    ///        the matching `begin()` is passed in; if it does not match
    ///        the top of the stack, the end is silently dropped.
    static void end(int slot) FL_NOEXCEPT;

    /// @brief Number of completed events in the log.
    static fl::size event_count() FL_NOEXCEPT;

    /// @brief Pointer to the log[0..event_count()) (thread-local).
    ///        Valid only until the next `begin`/`end`/`clear`/`flush`.
    static const PerfEvent* events() FL_NOEXCEPT;

    /// @brief Current in-flight stack depth.
    static fl::size stack_depth() FL_NOEXCEPT;

    /// @brief Whether any event was dropped since the last `clear()` /
    ///        `flush()` (log full or stack overflow).
    static bool dropped() FL_NOEXCEPT;

    /// @brief Clear the event log and in-flight stack.
    static void clear() FL_NOEXCEPT;

    /// @brief Emit every completed event through `cb` (or via FL_INFO
    ///        if `cb == nullptr`), then clear the log.
    /// @param cb Optional callback. Receives each event + `user` pointer.
    static void flush(PerfEmitFn cb = nullptr, void* user = nullptr) FL_NOEXCEPT;
};

/// @brief RAII guard: begins a scope on construction, ends it on
///        destruction. Non-copyable, non-movable.
class PerfScope {
public:
    PerfScope(const char* file, int line, const char* name) FL_NOEXCEPT
        : mSlot(PerfTrace::begin(file, line, name)) {}
    ~PerfScope() FL_NOEXCEPT { PerfTrace::end(mSlot); }

    PerfScope(const PerfScope&) = delete;
    PerfScope& operator=(const PerfScope&) = delete;
    PerfScope(PerfScope&&) = delete;
    PerfScope& operator=(PerfScope&&) = delete;
private:
    int mSlot;
};

} // namespace fl

#ifdef FASTLED_PERF_TRACE

#define FL_PERF_TRACE_CONCAT2(a, b) a##b
#define FL_PERF_TRACE_CONCAT(a, b) FL_PERF_TRACE_CONCAT2(a, b)

/// @brief Open a scoped perf-trace event. The scope ends automatically
///        at the end of the enclosing block.
/// @param name Static string label (string literal preferred).
#define FL_PERF_SCOPE(name) \
    ::fl::PerfScope FL_PERF_TRACE_CONCAT(_fl_perf_scope_, __LINE__){__FILE__, __LINE__, (name)}

/// @brief Begin a manual perf-trace event. Returns an int slot id you
///        must pass to FL_PERF_END.
#define FL_PERF_BEGIN(name) (::fl::PerfTrace::begin(__FILE__, __LINE__, (name)))

/// @brief End a manual perf-trace event opened by FL_PERF_BEGIN.
#define FL_PERF_END(slot) (::fl::PerfTrace::end(slot))

/// @brief Emit all accumulated events from the current thread and clear
///        the log. Typically called at the end of each frame.
#define FL_PERF_FRAME_FLUSH() (::fl::PerfTrace::flush())

#else  // !FASTLED_PERF_TRACE

#define FL_PERF_SCOPE(name) do { (void)sizeof((name)); } while (0)
#define FL_PERF_BEGIN(name) ((void)sizeof((name)), 0)
#define FL_PERF_END(slot)   do { (void)(slot); } while (0)
#define FL_PERF_FRAME_FLUSH() do {} while (0)

#endif // FASTLED_PERF_TRACE

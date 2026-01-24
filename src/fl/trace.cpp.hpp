/**
 * @file trace.cpp
 * @brief Implementation of internal call stack tracking for debugging
 *
 * This module provides a lightweight, ISR-safe call stack tracking system
 * that uses a fixed-size buffer to store function names during execution.
 */

#include "fl/trace.h"

#include "fl/stl/vector.h"
#include "fl/stl/string.h"
#include "fl/stl/time.h"
#include "fl/log.h"
#include "fl/thread_local.h"
#include "fl/stl/stdio.h"

// Configuration: Maximum stack depth (can be overridden at compile time)
#ifndef FL_STACK_TRACE_MAX_DEPTH
#define FL_STACK_TRACE_MAX_DEPTH 32
#endif

namespace fl {

/// @brief A single stack trace entry with location information
struct TraceEntry {
    const char* function;
    int line;

    TraceEntry() : function(nullptr), line(0) {}
    TraceEntry(const char* f, int l) : function(f), line(l) {}
};

/// @brief Internal storage for the trace system
/// Encapsulates call stack and depth tracking per-thread
struct TraceStorage {
    // Call stack storage using FixedVector for bounds checking
    FixedVector<TraceEntry, FL_STACK_TRACE_MAX_DEPTH> callStack;

    // Separate depth counter to track actual stack depth (can exceed capacity)
    // This allows us to detect overflow conditions without corrupting the stack
    fl::size stackDepth = 0;
};

// Thread-local storage for trace data
// When trace functions are not used, the optimizer can eliminate this entirely
static ThreadLocal<TraceStorage>& getTraceStorage() {
    static ThreadLocal<TraceStorage> storage;
    return storage;
}

// ScopedTrace static method implementations
void ScopedTrace::push(const char* function, int line) {
    if (!function) {
        return;  // Ignore null function names
    }

    auto& storage = getTraceStorage().access();

    // Always increment depth counter (tracks true depth even on overflow)
    storage.stackDepth++;

    // Only push to storage if we have capacity
    if (storage.callStack.size() < FL_STACK_TRACE_MAX_DEPTH) {
        storage.callStack.push_back(TraceEntry(function, line));
    }
    // If size >= MAX_DEPTH, we're in overflow - just track depth
}

void ScopedTrace::pop() {
    auto& storage = getTraceStorage().access();

    // Guard against underflow
    if (storage.stackDepth == 0) {
        return;
    }

    // Decrement depth counter first
    storage.stackDepth--;

    // Pop from callStack only if:
    // 1. We have entries in the stack
    // 2. After decrement, depth matches or is less than callStack size
    //    (this handles both normal operation and overflow recovery)
    if (!storage.callStack.empty() && storage.stackDepth < storage.callStack.size()) {
        // Use manual pop since FixedVector doesn't have pop_back
        storage.callStack.resize(storage.callStack.size() - 1);
    }
}

fl::size ScopedTrace::depth() {
    return getTraceStorage().access().stackDepth;
}

fl::string ScopedTrace::dump() {
    auto& storage = getTraceStorage().access();
    fl::string result = "Stack trace (depth ";

    char depth_str[32];
    fl::sprintf(depth_str, "%zu", (size_t)storage.stackDepth);
    result += depth_str;
    result += "):\n";

    if (storage.callStack.empty()) {
        result += "  <empty>\n";
        return result;
    }

    // Show overflow warning if depth exceeds capacity
    if (storage.stackDepth > FL_STACK_TRACE_MAX_DEPTH) {
        result += "  <WARNING: Stack overflow - showing last ";
        char max_str[32];
        fl::sprintf(max_str, "%d", FL_STACK_TRACE_MAX_DEPTH);
        result += max_str;
        result += " of ";
        result += depth_str;
        result += " entries>\n";
    }

    // Dump all stored entries in format: functionName(lineNo)
    for (fl::size i = 0; i < storage.callStack.size(); ++i) {
        const auto& entry = storage.callStack[i];
        char line_buf[256];
        if (entry.line > 0) {
            fl::sprintf(line_buf, "  [%zu] %s(%d)\n", (size_t)i, entry.function, entry.line);
        } else {
            fl::sprintf(line_buf, "  [%zu] %s\n", (size_t)i, entry.function);
        }
        result += line_buf;
    }

    return result;
}

void ScopedTrace::dump(fl::vector<TracePoint>* out) {
    if (!out) {
        return;
    }

    auto& storage = getTraceStorage().access();
    out->clear();

    // Copy all stored entries to the output vector as TracePoints
    for (fl::size i = 0; i < storage.callStack.size(); ++i) {
        const auto& entry = storage.callStack[i];
        // TracePoint is tuple<const char*, int, fl::u32> (file, line, timestamp)
        // Since we don't store file or timestamp, we'll use the function name and line
        out->push_back(fl::make_tuple(entry.function, entry.line, fl::u32(0)));
    }
}

void ScopedTrace::clear() {
    auto& storage = getTraceStorage().access();
    storage.callStack.resize(0);
    storage.stackDepth = 0;
}

// ScopedTrace RAII implementation
ScopedTrace::ScopedTrace(const char* function, int line) {
    push(function, line);
}

ScopedTrace::~ScopedTrace() {
    pop();
}

} // namespace fl

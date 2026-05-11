#pragma once

// IWYU pragma: private
// ok no namespace fl — EM_JS macros generate extern "C" functions

#include "platforms/wasm/is_wasm.h"

// JSPI glue is compiled out when the pthread back-end is selected.
// See platforms/wasm/coroutine_platform_wasm_pthread.hpp.
#if defined(FL_IS_WASM) && !defined(FASTLED_WASM_PTHREADS)

// Register a context in the JS scheduler state.
EM_JS(void, _jspi_register, (int id), {
    if (!Module['_jspiCtx']) Module['_jspiCtx'] = {};
    Module['_jspiCtx'][id] = { started : false, resolve : null };
});

// Unregister a context, unblocking it if suspended.
// NOLINTBEGIN
EM_JS(void, _jspi_unregister, (int id), {
    var ctx = Module['_jspiCtx'];
    if (!ctx) return;
    if (ctx[id] && ctx[id].resolve) {
        ctx[id].resolve();
    }
    delete ctx[id]; // ok bare allocation - JS code, not C++
});
// NOLINTEND

// Core context switch: suspend 'from', resume or start 'to'.
//
// Flow for STARTING a new coroutine (first contextSwitch to it):
//   1. Store from's resume callback
//   2. Call Module['_jspi_run_entry'](to_id) - promising WASM export
//   3. Coroutine WASM runs synchronously until first yield
//   4. Coroutine's yield resolves from's resume callback (synchronous)
//   5. await resumes from - contextSwitch returns
//
// Flow for RESUMING an existing coroutine:
//   1. Store from's resume callback
//   2. Resolve to's pending promise (schedules coroutine resume)
//   3. await from's resume - yields to microtask queue
//   4. Coroutine resumes, runs until yield, resolves from's callback
//   5. from resumes - contextSwitch returns
// NOLINTBEGIN
EM_ASYNC_JS(void, _jspi_context_switch, (int from_id, int to_id), {
    var ctx = Module['_jspiCtx'];

    // Suspend caller: create a promise that resolves when we're resumed
    var myResume = new Promise(function(r) { ctx[from_id].resolve = r; }); // ok bare allocation - JS code

    if (!ctx[to_id].started) {
        // First time: start the coroutine as a new JSPI-promising WASM call.
        // Runs synchronously until the coroutine's first yield, which
        // resolves our myResume promise before we await it.
        ctx[to_id].started = true;
        Module['_jspi_run_entry'](to_id);
    } else if (ctx[to_id].resolve) {
        // Resume: resolve the target's suspended promise
        var r = ctx[to_id].resolve;
        ctx[to_id].resolve = null;
        r();
    }

    // Wait for someone to resume us
    await myResume;
});
// NOLINTEND

#endif  // FL_IS_WASM && !FASTLED_WASM_PTHREADS

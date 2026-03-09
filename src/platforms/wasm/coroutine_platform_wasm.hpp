#pragma once

// IWYU pragma: private

/// @file coroutine_platform_wasm.hpp
/// @brief WASM coroutine platform using JSPI (JavaScript Promise Integration)
///
/// Implements ICoroutinePlatform using JSPI for cooperative context switching
/// in WASM. Unlike asyncify, JSPI handles stack management at the engine level
/// with zero WASM code transformation — no whitelist needed, no size overhead.
///
/// Each coroutine runs as a separate JSPI-promising WASM call. Context switching
/// suspends the caller via an EM_ASYNC_JS import and resumes the target by
/// resolving its pending Promise. A lightweight JS scheduler coordinates this.
///
/// Requires -sJSPI linker flag. Supported in Chrome 123+.

#include "platforms/wasm/is_wasm.h"

#ifdef FL_IS_WASM

// IWYU pragma: begin_keep
#include <emscripten.h>
#include <emscripten/em_js.h>
// IWYU pragma: end_keep

#include "platforms/coroutine_runtime.h"
#include "fl/int.h"

namespace fl {
namespace platforms {

static constexpr int kMaxJspiContexts = 32;

/// Entry function table — indexed by context ID.
/// ID 0 is reserved for the runner (main thread) context.
static void (*g_jspi_entries[kMaxJspiContexts])() = {};
static int g_jspi_next_id = 1;

}  // namespace platforms
}  // namespace fl

// ==========================================================================
// WASM export: coroutine entry point (called from JS scheduler)
// ==========================================================================
// Must be in JSPI_EXPORTS so the engine wraps it as a promising export.
// When called, the WASM runs synchronously until the coroutine's first yield.

extern "C" EMSCRIPTEN_KEEPALIVE void jspi_run_entry(int id) {
    using namespace fl::platforms;
    if (id >= 0 && id < kMaxJspiContexts && g_jspi_entries[id]) {
        g_jspi_entries[id]();
    }
}

// ==========================================================================
// JS scheduler glue — embedded via EM_JS / EM_ASYNC_JS
// ==========================================================================

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
//   2. Call Module['_jspi_run_entry'](to_id) — promising WASM export
//   3. Coroutine WASM runs synchronously until first yield
//   4. Coroutine's yield resolves from's resume callback (synchronous)
//   5. await resumes from — contextSwitch returns
//
// Flow for RESUMING an existing coroutine:
//   1. Store from's resume callback
//   2. Resolve to's pending promise (schedules coroutine resume)
//   3. await from's resume — yields to microtask queue
//   4. Coroutine resumes, runs until yield, resolves from's callback
//   5. from resumes — contextSwitch returns
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

// ==========================================================================
// CoroutinePlatformWasm — ICoroutinePlatform implementation
// ==========================================================================

namespace fl {
namespace platforms {

// NOLINTBEGIN(reinterpret_cast)
// Context IDs are small ints stored in void* — reinterpret_cast is unavoidable
// because the ICoroutinePlatform interface uses void* for platform contexts.

inline void* id_to_ctx(int id) {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(id));  // ok reinterpret cast
}

inline int ctx_to_id(void* ctx) {
    return static_cast<int>(reinterpret_cast<uintptr_t>(ctx));  // ok reinterpret cast
}
// NOLINTEND(reinterpret_cast)

class CoroutinePlatformWasm : public ICoroutinePlatform {
public:
    void* createContext(void (*entry_fn)(), size_t /*stack_size*/) override {
        // JSPI manages stacks at the engine level — stack_size is ignored
        int id = g_jspi_next_id++;
        if (id >= kMaxJspiContexts) {
            return nullptr;
        }
        g_jspi_entries[id] = entry_fn;
        _jspi_register(id);
        return id_to_ctx(id);
    }

    void* createRunnerContext() override {
        // Runner is always ID 0
        _jspi_register(0);
        return id_to_ctx(0);
    }

    void destroyContext(void* ctx) override {
        int id = ctx_to_id(ctx);
        if (id >= 0 && id < kMaxJspiContexts) {
            g_jspi_entries[id] = nullptr;
            _jspi_unregister(id);
        }
    }

    void contextSwitch(void* from_ctx, void* to_ctx) override {
        _jspi_context_switch(ctx_to_id(from_ctx), ctx_to_id(to_ctx));
    }

    fl::u32 micros() const override {
        // emscripten_get_now() returns milliseconds as double
        return static_cast<fl::u32>(emscripten_get_now() * 1000.0);
    }
};

}  // namespace platforms
}  // namespace fl

#endif  // FL_IS_WASM
